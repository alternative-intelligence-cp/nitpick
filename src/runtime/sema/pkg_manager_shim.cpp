// ═══════════════════════════════════════════════════════════════════════
// pkg_manager_shim.cpp — C bridge for the self-hosted Package Manager
// v0.15.2 Port 11
// ═══════════════════════════════════════════════════════════════════════
// Exposes: TOML metadata parsing, name/version validation, directory init,
//          install, remove, list, pack, and metadata field access.
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <toml.hpp>

namespace fs = std::filesystem;

// ─── Internal structs ────────────────────────────────────────────────

struct PkgDependency {
    std::string name;
    std::string version_spec;
};

struct PkgBuildInfo {
    std::string type;   // "library" or "executable"
    std::string entry;
    std::vector<std::string> outputs;
};

struct PkgMetadata {
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string license;
    std::string description;
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
    std::vector<PkgDependency> dependencies;
    std::vector<PkgDependency> dev_dependencies;
    std::optional<PkgBuildInfo> build;
};

struct PkgInstalled {
    std::string name;
    std::string version;
    std::string install_path;
};

// ─── Global state ────────────────────────────────────────────────────

static std::vector<std::unique_ptr<PkgMetadata>> g_metadata;
static std::vector<PkgInstalled>                 g_listed;
static thread_local char g_ret_buf[4096];

static const char* ret(const std::string& s) {
    size_t len = std::min(s.size(), sizeof(g_ret_buf) - 1);
    std::memcpy(g_ret_buf, s.c_str(), len);
    g_ret_buf[len] = '\0';
    return g_ret_buf;
}

// ─── Internal helpers ────────────────────────────────────────────────

static bool parse_toml_metadata(const std::string& toml_path, PkgMetadata& md) {
    try {
        auto data = toml::parse(toml_path);

        const auto& package = toml::find(data, "package");
        md.name    = toml::find<std::string>(package, "name");
        md.version = toml::find<std::string>(package, "version");

        if (package.contains("authors")) {
            md.authors = toml::find<std::vector<std::string>>(package, "authors");
        } else if (package.contains("author")) {
            md.authors.push_back(toml::find<std::string>(package, "author"));
        }
        if (package.contains("license"))
            md.license = toml::find<std::string>(package, "license");
        if (package.contains("description"))
            md.description = toml::find<std::string>(package, "description");
        if (package.contains("keywords"))
            md.keywords = toml::find<std::vector<std::string>>(package, "keywords");
        if (package.contains("categories"))
            md.categories = toml::find<std::vector<std::string>>(package, "categories");

        // dependencies
        if (data.contains("dependencies")) {
            const auto& deps = toml::find(data, "dependencies");
            if (deps.is_table()) {
                for (const auto& [key, value] : deps.as_table()) {
                    PkgDependency dep;
                    dep.name = key;
                    if (value.is_string()) dep.version_spec = value.as_string();
                    md.dependencies.push_back(dep);
                }
            }
        }

        // dev-dependencies
        if (data.contains("dev-dependencies")) {
            const auto& ddeps = toml::find(data, "dev-dependencies");
            if (ddeps.is_table()) {
                for (const auto& [key, value] : ddeps.as_table()) {
                    PkgDependency dep;
                    dep.name = key;
                    if (value.is_string()) dep.version_spec = value.as_string();
                    md.dev_dependencies.push_back(dep);
                }
            }
        }

        // build
        if (data.contains("build")) {
            const auto& b = toml::find(data, "build");
            PkgBuildInfo bi;
            bi.type  = toml::find<std::string>(b, "type");
            bi.entry = toml::find<std::string>(b, "entry");
            if (b.contains("outputs"))
                bi.outputs = toml::find<std::vector<std::string>>(b, "outputs");
            md.build = bi;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[pkg_shim] parse error: " << e.what() << std::endl;
        return false;
    }
}

static bool validate_pkg_name(const std::string& name) {
    if (name.empty() || name.length() < 3 || name.length() > 64) return false;
    if (!std::islower(static_cast<unsigned char>(name[0]))) return false;
    for (char c : name) {
        if (!std::islower(static_cast<unsigned char>(c))
            && !std::isdigit(static_cast<unsigned char>(c))
            && c != '-' && c != '_')
            return false;
    }
    return true;
}

static bool validate_pkg_version(const std::string& ver) {
    if (ver.empty()) return false;
    // Basic semver check: at least one dot
    return ver.find('.') != std::string::npos;
}

// ─── JSON registry helpers ───────────────────────────────────────────

static std::vector<PkgInstalled> load_registry(const std::string& root) {
    std::vector<PkgInstalled> pkgs;
    std::string path = root + "/registry/index.json";
    std::ifstream f(path);
    if (!f.is_open()) return pkgs;

    std::string line;
    PkgInstalled cur;
    bool in_obj = false;
    while (std::getline(f, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        std::string t = line.substr(s);

        if (t == "{") { in_obj = true; cur = {}; continue; }
        if (t[0] == '}') {
            if (in_obj && !cur.name.empty()) pkgs.push_back(cur);
            in_obj = false;
            continue;
        }
        if (!in_obj) continue;

        auto colon = t.find(':');
        if (colon == std::string::npos) continue;
        std::string key = t.substr(0, colon);
        std::string val = t.substr(colon + 1);

        auto kq1 = key.find('"'), kq2 = key.rfind('"');
        if (kq1 == std::string::npos || kq1 == kq2) continue;
        key = key.substr(kq1 + 1, kq2 - kq1 - 1);

        size_t vs = val.find_first_not_of(" \t");
        if (vs != std::string::npos) val = val.substr(vs);
        if (!val.empty() && val.back() == ',') val.pop_back();

        auto vq1 = val.find('"'), vq2 = val.rfind('"');
        std::string sv;
        if (vq1 != std::string::npos && vq1 != vq2) sv = val.substr(vq1 + 1, vq2 - vq1 - 1);

        if      (key == "name")         cur.name = sv;
        else if (key == "version")      cur.version = sv;
        else if (key == "install_path") cur.install_path = sv;
    }
    return pkgs;
}

static bool save_registry(const std::string& root, const std::vector<PkgInstalled>& pkgs) {
    std::string path = root + "/registry/index.json";
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "[\n";
    for (size_t i = 0; i < pkgs.size(); ++i) {
        f << "  {\n"
          << "    \"name\": \"" << pkgs[i].name << "\",\n"
          << "    \"version\": \"" << pkgs[i].version << "\",\n"
          << "    \"install_path\": \"" << pkgs[i].install_path << "\"\n"
          << "  }";
        if (i + 1 < pkgs.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
//  extern "C" bridge
// ═══════════════════════════════════════════════════════════════════════
extern "C" {

// ─── Directory init ──────────────────────────────────────────────────

int32_t pkg_init_directory(const char* root) {
    try {
        std::string r(root);
        fs::create_directories(r);
        fs::create_directories(r + "/registry");
        fs::create_directories(r + "/cache");
        fs::create_directories(r + "/installed");
        fs::create_directories(r + "/bin");
        std::string idx = r + "/registry/index.json";
        if (!fs::exists(idx)) {
            std::ofstream f(idx);
            f << "[]" << std::endl;
        }
        return 0;
    } catch (...) {
        return -1;
    }
}

int32_t pkg_dir_exists(const char* root) {
    return fs::exists(std::string(root) + "/registry/index.json") ? 1 : 0;
}

// ─── TOML metadata parsing ──────────────────────────────────────────

int64_t pkg_parse_metadata(const char* toml_path) {
    auto md = std::make_unique<PkgMetadata>();
    if (!parse_toml_metadata(toml_path, *md)) return -1;
    g_metadata.push_back(std::move(md));
    return static_cast<int64_t>(g_metadata.size() - 1);
}

int32_t pkg_free_metadata(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_metadata.size()) return -1;
    g_metadata[handle].reset();
    return 0;
}

const char* pkg_metadata_name(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    return ret(g_metadata[h]->name);
}

const char* pkg_metadata_version(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    return ret(g_metadata[h]->version);
}

const char* pkg_metadata_description(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    return ret(g_metadata[h]->description);
}

const char* pkg_metadata_license(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    return ret(g_metadata[h]->license);
}

int32_t pkg_metadata_author_count(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return 0;
    return static_cast<int32_t>(g_metadata[h]->authors.size());
}

const char* pkg_metadata_author(int64_t h, int32_t idx) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    auto& a = g_metadata[h]->authors;
    if (idx < 0 || idx >= (int32_t)a.size()) return ret("");
    return ret(a[idx]);
}

int32_t pkg_metadata_dep_count(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return 0;
    return static_cast<int32_t>(g_metadata[h]->dependencies.size());
}

const char* pkg_metadata_dep_name(int64_t h, int32_t idx) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    auto& d = g_metadata[h]->dependencies;
    if (idx < 0 || idx >= (int32_t)d.size()) return ret("");
    return ret(d[idx].name);
}

const char* pkg_metadata_dep_version(int64_t h, int32_t idx) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return ret("");
    auto& d = g_metadata[h]->dependencies;
    if (idx < 0 || idx >= (int32_t)d.size()) return ret("");
    return ret(d[idx].version_spec);
}

int32_t pkg_metadata_keyword_count(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return 0;
    return static_cast<int32_t>(g_metadata[h]->keywords.size());
}

int32_t pkg_metadata_category_count(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return 0;
    return static_cast<int32_t>(g_metadata[h]->categories.size());
}

int32_t pkg_metadata_has_build(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h]) return 0;
    return g_metadata[h]->build.has_value() ? 1 : 0;
}

const char* pkg_metadata_build_type(int64_t h) {
    if (h < 0 || h >= (int64_t)g_metadata.size() || !g_metadata[h] || !g_metadata[h]->build)
        return ret("");
    return ret(g_metadata[h]->build->type);
}

// ─── Validation ──────────────────────────────────────────────────────

int32_t pkg_validate_name(const char* name) {
    return validate_pkg_name(name) ? 1 : 0;
}

int32_t pkg_validate_version(const char* version) {
    return validate_pkg_version(version) ? 1 : 0;
}

// ─── Install from directory ──────────────────────────────────────────

int32_t pkg_install_from_dir(const char* root, const char* pkg_dir) {
    try {
        std::string rd(root), pd(pkg_dir);
        std::string toml_path = pd + "/aria-package.toml";
        if (!fs::exists(toml_path)) return -1;

        PkgMetadata md;
        if (!parse_toml_metadata(toml_path, md)) return -2;
        if (!validate_pkg_name(md.name)) return -3;
        if (!validate_pkg_version(md.version)) return -4;

        std::string install_path = rd + "/installed/" + md.name + "/" + md.version;
        if (fs::exists(install_path)) return -5; // already installed

        fs::create_directories(install_path);
        fs::copy(pd, install_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

        // Update registry
        auto pkgs = load_registry(rd);
        pkgs.push_back({md.name, md.version, install_path});
        save_registry(rd, pkgs);

        return 0;
    } catch (...) {
        return -10;
    }
}

// ─── Remove ──────────────────────────────────────────────────────────

int32_t pkg_remove(const char* root, const char* name) {
    try {
        std::string rd(root), nm(name);
        std::string path = rd + "/installed/" + nm;
        if (!fs::exists(path)) return -1;
        fs::remove_all(path);

        // Update registry — remove entries matching this name
        auto pkgs = load_registry(rd);
        pkgs.erase(
            std::remove_if(pkgs.begin(), pkgs.end(),
                [&nm](const PkgInstalled& p) { return p.name == nm; }),
            pkgs.end());
        save_registry(rd, pkgs);

        return 0;
    } catch (...) {
        return -10;
    }
}

// ─── List ────────────────────────────────────────────────────────────

int32_t pkg_list_refresh(const char* root) {
    g_listed.clear();
    try {
        std::string installed_dir = std::string(root) + "/installed";
        if (!fs::exists(installed_dir)) return 0;

        for (const auto& pe : fs::directory_iterator(installed_dir)) {
            if (!pe.is_directory()) continue;
            std::string pn = pe.path().filename().string();
            for (const auto& ve : fs::directory_iterator(pe.path())) {
                if (!ve.is_directory()) continue;
                PkgInstalled pkg;
                pkg.name = pn;
                pkg.version = ve.path().filename().string();
                pkg.install_path = ve.path().string();
                g_listed.push_back(pkg);
            }
        }
        return static_cast<int32_t>(g_listed.size());
    } catch (...) {
        return -1;
    }
}

int32_t pkg_list_count() {
    return static_cast<int32_t>(g_listed.size());
}

const char* pkg_list_name(int32_t idx) {
    if (idx < 0 || idx >= (int32_t)g_listed.size()) return ret("");
    return ret(g_listed[idx].name);
}

const char* pkg_list_version(int32_t idx) {
    if (idx < 0 || idx >= (int32_t)g_listed.size()) return ret("");
    return ret(g_listed[idx].version);
}

const char* pkg_list_path(int32_t idx) {
    if (idx < 0 || idx >= (int32_t)g_listed.size()) return ret("");
    return ret(g_listed[idx].install_path);
}

// ─── Pack ────────────────────────────────────────────────────────────

int32_t pkg_pack(const char* pkg_dir, const char* output_dir) {
    try {
        std::string pd(pkg_dir), od(output_dir);
        if (!fs::exists(pd) || !fs::is_directory(pd)) return -1;

        std::string toml_path = pd + "/aria-package.toml";
        if (!fs::exists(toml_path)) return -2;

        PkgMetadata md;
        if (!parse_toml_metadata(toml_path, md)) return -3;
        if (!validate_pkg_name(md.name)) return -4;

        std::string filename = md.name + "-" + md.version + ".npk-pkg";
        std::string out_path = od + "/" + filename;

        std::string abs = fs::canonical(pd).string();
        std::string parent = fs::path(abs).parent_path().string();
        std::string dir_name = fs::path(abs).filename().string();

        std::string cmd = "tar -czf '" + out_path + "' -C '" + parent + "' '" + dir_name + "'";
        int rc = system(cmd.c_str());
        return (rc == 0) ? 0 : -5;
    } catch (...) {
        return -10;
    }
}

// ─── Test fixture helper ─────────────────────────────────────────────

int64_t pkg_create_test_fixture(const char* base_dir) {
    try {
        std::string bd(base_dir);
        fs::create_directories(bd);

        // Write aria-package.toml
        std::ofstream f(bd + "/aria-package.toml");
        if (!f.is_open()) return -1;
        f << "[package]\n"
          << "name = \"test-hello\"\n"
          << "version = \"1.0.0\"\n"
          << "authors = [\"Test Author\"]\n"
          << "license = \"MIT\"\n"
          << "description = \"A test package\"\n"
          << "keywords = [\"test\", \"hello\"]\n"
          << "categories = [\"testing\"]\n"
          << "\n"
          << "[dependencies]\n"
          << "aria-core = \">=0.9.0\"\n"
          << "\n"
          << "[build]\n"
          << "type = \"library\"\n"
          << "entry = \"src/main.aria\"\n";
        f.close();

        // Write a dummy source file
        fs::create_directories(bd + "/src");
        std::ofstream src(bd + "/src/main.aria");
        src << "pub func:hello = int32() {\n    pass 0i32;\n};\n";
        src.close();

        return 0;
    } catch (...) {
        return -1;
    }
}

int64_t pkg_cleanup_test(const char* path) {
    try {
        if (fs::exists(path)) fs::remove_all(path);
        return 0;
    } catch (...) {
        return -1;
    }
}

} // extern "C"
