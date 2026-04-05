// ═══════════════════════════════════════════════════════════════════════
// project_config_shim.cpp — C bridge for the self-hosted Project Config
// v0.15.2 Port 12
// ═══════════════════════════════════════════════════════════════════════
// Self-contained: parses aria.toml directly via toml.hpp,
// no dependency on tools/project_config.h.
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <regex>
#include <algorithm>
#include <toml.hpp>

namespace fs = std::filesystem;

// ─── Constants (must match Aria-side) ────────────────────────────────
enum ProjTarget : int32_t {
    TARGET_EXECUTABLE = 0,
    TARGET_LIBRARY    = 1,
    TARGET_STATIC     = 2,
    TARGET_SHARED     = 3
};

enum ProjOpt : int32_t {
    OPT_DEBUG   = 0,
    OPT_RELEASE = 1,
    OPT_SIZE    = 2
};

// ─── Internal structs ────────────────────────────────────────────────

struct CfgDep {
    std::string name;
    std::string version;
};

struct CfgConfig {
    std::string name, version, edition, license, description;
    std::vector<std::string> authors, keywords;
    bool publish = true;
    int32_t target = TARGET_EXECUTABLE;
    int32_t optimization = OPT_DEBUG;
    std::string build_output = "build/", build_main = "src/main.aria";
    std::vector<std::string> build_sources;
    std::map<std::string, CfgDep> dependencies, dev_dependencies;
    std::map<std::string, std::vector<std::string>> features;
    std::vector<std::string> workspace_members;
    bool has_lib = false;
    int bin_count_val = 0;
};

// ─── Global state ────────────────────────────────────────────────────

static std::vector<std::unique_ptr<CfgConfig>> g_configs;
static std::vector<std::string>                g_dep_names;
static thread_local char g_ret_buf[4096];

static const char* ret(const std::string& s) {
    size_t len = std::min(s.size(), sizeof(g_ret_buf) - 1);
    std::memcpy(g_ret_buf, s.c_str(), len);
    g_ret_buf[len] = '\0';
    return g_ret_buf;
}

// ─── Internal parser helpers ─────────────────────────────────────────

static int32_t to_target(const std::string& t) {
    if (t == "executable") return TARGET_EXECUTABLE;
    if (t == "library")    return TARGET_LIBRARY;
    if (t == "static")     return TARGET_STATIC;
    if (t == "shared")     return TARGET_SHARED;
    return TARGET_EXECUTABLE;
}

static int32_t to_opt(const std::string& o) {
    if (o == "debug")   return OPT_DEBUG;
    if (o == "release") return OPT_RELEASE;
    if (o == "size")    return OPT_SIZE;
    return OPT_DEBUG;
}

static bool parse_toml_into(const toml::value& data, CfgConfig& cfg) {
    if (!data.contains("package")) return false;
    const auto& pkg = toml::find(data, "package");
    cfg.name    = toml::find<std::string>(pkg, "name");
    cfg.version = toml::find<std::string>(pkg, "version");
    cfg.authors = toml::find_or<std::vector<std::string>>(pkg, "authors", {});
    cfg.edition = toml::find_or<std::string>(pkg, "edition", "2025");
    cfg.license = toml::find_or<std::string>(pkg, "license", "");
    cfg.description = toml::find_or<std::string>(pkg, "description", "");
    cfg.keywords = toml::find_or<std::vector<std::string>>(pkg, "keywords", {});
    cfg.publish  = toml::find_or<bool>(pkg, "publish", true);

    if (data.contains("build")) {
        const auto& b = toml::find(data, "build");
        if (b.contains("target"))
            cfg.target = to_target(toml::find<std::string>(b, "target"));
        if (b.contains("optimization"))
            cfg.optimization = to_opt(toml::find<std::string>(b, "optimization"));
        cfg.build_output  = toml::find_or<std::string>(b, "output", "build/");
        cfg.build_main    = toml::find_or<std::string>(b, "main", "src/main.aria");
        cfg.build_sources = toml::find_or<std::vector<std::string>>(b, "sources", {});
    }

    auto parse_deps = [](const toml::value& d, std::map<std::string, CfgDep>& out) {
        if (!d.is_table()) return;
        for (const auto& [k, v] : d.as_table()) {
            CfgDep dep; dep.name = k;
            if (v.is_string()) dep.version = v.as_string();
            out[k] = dep;
        }
    };
    if (data.contains("dependencies"))
        parse_deps(toml::find(data, "dependencies"), cfg.dependencies);
    if (data.contains("dev-dependencies"))
        parse_deps(toml::find(data, "dev-dependencies"), cfg.dev_dependencies);

    if (data.contains("features")) {
        const auto& f = toml::find(data, "features");
        for (const auto& [k, v] : f.as_table()) {
            std::vector<std::string> fl;
            for (const auto& item : v.as_array()) fl.push_back(item.as_string());
            cfg.features[k] = fl;
        }
    }

    if (data.contains("workspace"))
        cfg.workspace_members = toml::find_or<std::vector<std::string>>(
            toml::find(data, "workspace"), "members", {});

    cfg.has_lib = data.contains("lib");
    if (data.contains("bin")) {
        const auto& b = toml::find(data, "bin");
        cfg.bin_count_val = b.is_array() ? (int)b.as_array().size() : 1;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
extern "C" {

#define CK(h) if ((h)<0||(h)>=(int64_t)g_configs.size()||!g_configs[(h)])

int64_t projcfg_parse_file(const char* path) {
    try {
        auto data = toml::parse(path);
        auto cfg = std::make_unique<CfgConfig>();
        if (!parse_toml_into(data, *cfg)) return -1;
        g_configs.push_back(std::move(cfg));
        return (int64_t)(g_configs.size() - 1);
    } catch (const std::exception& e) {
        std::cerr << "[projcfg] " << e.what() << std::endl;
        return -1;
    }
}

int64_t projcfg_parse_string(const char* content, const char* base_path) {
    try {
        std::istringstream iss(content);
        auto data = toml::parse(iss, "aria.toml");
        auto cfg = std::make_unique<CfgConfig>();
        if (!parse_toml_into(data, *cfg)) return -1;
        g_configs.push_back(std::move(cfg));
        return (int64_t)(g_configs.size() - 1);
    } catch (const std::exception& e) {
        std::cerr << "[projcfg] " << e.what() << std::endl;
        return -1;
    }
}

int32_t projcfg_free(int64_t h) {
    CK(h) return -1; g_configs[h].reset(); return 0;
}

const char* projcfg_pkg_name(int64_t h)    { CK(h) return ret(""); return ret(g_configs[h]->name); }
const char* projcfg_pkg_version(int64_t h) { CK(h) return ret(""); return ret(g_configs[h]->version); }
const char* projcfg_pkg_edition(int64_t h) { CK(h) return ret(""); return ret(g_configs[h]->edition); }
const char* projcfg_pkg_license(int64_t h) { CK(h) return ret(""); return ret(g_configs[h]->license); }
const char* projcfg_pkg_description(int64_t h) { CK(h) return ret(""); return ret(g_configs[h]->description); }

int32_t projcfg_pkg_author_count(int64_t h) { CK(h) return 0; return (int32_t)g_configs[h]->authors.size(); }
const char* projcfg_pkg_author(int64_t h, int32_t idx) {
    CK(h) return ret("");
    auto& a = g_configs[h]->authors;
    if (idx < 0 || idx >= (int32_t)a.size()) return ret("");
    return ret(a[idx]);
}
int32_t projcfg_pkg_publish(int64_t h) { CK(h) return 0; return g_configs[h]->publish ? 1 : 0; }
int32_t projcfg_pkg_keyword_count(int64_t h) { CK(h) return 0; return (int32_t)g_configs[h]->keywords.size(); }
const char* projcfg_pkg_keyword(int64_t h, int32_t idx) {
    CK(h) return ret("");
    auto& k = g_configs[h]->keywords;
    if (idx < 0 || idx >= (int32_t)k.size()) return ret("");
    return ret(k[idx]);
}

int32_t projcfg_build_target(int64_t h)       { CK(h) return -1; return g_configs[h]->target; }
int32_t projcfg_build_optimization(int64_t h)  { CK(h) return -1; return g_configs[h]->optimization; }
const char* projcfg_build_output(int64_t h)    { CK(h) return ret(""); return ret(g_configs[h]->build_output); }
const char* projcfg_build_main(int64_t h)      { CK(h) return ret(""); return ret(g_configs[h]->build_main); }
int32_t projcfg_build_source_count(int64_t h)  { CK(h) return 0; return (int32_t)g_configs[h]->build_sources.size(); }

int32_t projcfg_dep_count(int64_t h)     { CK(h) return 0; return (int32_t)g_configs[h]->dependencies.size(); }
int32_t projcfg_dev_dep_count(int64_t h) { CK(h) return 0; return (int32_t)g_configs[h]->dev_dependencies.size(); }

int32_t projcfg_dep_names_refresh(int64_t h) {
    g_dep_names.clear(); CK(h) return 0;
    for (const auto& [n, _] : g_configs[h]->dependencies) g_dep_names.push_back(n);
    return (int32_t)g_dep_names.size();
}
const char* projcfg_dep_name(int32_t idx) {
    if (idx < 0 || idx >= (int32_t)g_dep_names.size()) return ret("");
    return ret(g_dep_names[idx]);
}
const char* projcfg_dep_version_by_name(int64_t h, const char* name) {
    CK(h) return ret("");
    auto it = g_configs[h]->dependencies.find(name);
    if (it == g_configs[h]->dependencies.end()) return ret("");
    return ret(it->second.version);
}

int32_t projcfg_feature_count(int64_t h)           { CK(h) return 0; return (int32_t)g_configs[h]->features.size(); }
int32_t projcfg_has_feature(int64_t h, const char* name) { CK(h) return 0; return g_configs[h]->features.count(name) ? 1 : 0; }

int32_t projcfg_validate_name(const char* name) {
    std::string n(name);
    if (n.empty() || n.length() > 64) return 0;
    if (!std::isalpha((unsigned char)n[0])) return 0;
    std::regex pat("^[a-zA-Z][a-zA-Z0-9_-]*$");
    if (!std::regex_match(n, pat)) return 0;
    if (n == "std" || n == "core" || n == "aria") return 0;
    return 1;
}

int32_t projcfg_validate_version(const char* version) {
    std::string v(version);
    if (v.empty()) return 0;
    std::regex pat(R"(^(\d+)\.(\d+)\.(\d+)(-[a-zA-Z0-9.-]+)?(\+[a-zA-Z0-9.-]+)?$)");
    return std::regex_match(v, pat) ? 1 : 0;
}

int32_t projcfg_is_reserved_name(const char* name) {
    std::string n(name);
    return (n == "std" || n == "core" || n == "aria") ? 1 : 0;
}

int32_t projcfg_workspace_member_count(int64_t h) { CK(h) return 0; return (int32_t)g_configs[h]->workspace_members.size(); }
int32_t projcfg_has_lib(int64_t h)   { CK(h) return 0; return g_configs[h]->has_lib ? 1 : 0; }
int32_t projcfg_bin_count(int64_t h) { CK(h) return 0; return g_configs[h]->bin_count_val; }

int64_t projcfg_create_test_fixture(const char* dir_path) {
    try {
        fs::create_directories(dir_path);
        std::ofstream f(std::string(dir_path) + "/aria.toml");
        if (!f.is_open()) return -1;
        f << "[package]\nname = \"hello-world\"\nversion = \"0.2.0\"\n"
          << "authors = [\"Alice\", \"Bob\"]\nedition = \"2025\"\nlicense = \"MIT\"\n"
          << "description = \"A sample project\"\nkeywords = [\"sample\", \"demo\"]\n"
          << "publish = true\n\n"
          << "[dependencies]\naria-std = \"0.1.0\"\njson-parser = \"1.2.0\"\n\n"
          << "[build]\ntarget = \"executable\"\noptimization = \"release\"\n"
          << "output = \"build/\"\nmain = \"src/main.aria\"\n\n"
          << "[features]\ndefault = [\"logging\"]\nlogging = []\n"
          << "debug-extras = [\"logging\"]\n";
        f.close();
        return 0;
    } catch (...) { return -1; }
}

int64_t projcfg_cleanup_test(const char* path) {
    try { if (fs::exists(path)) fs::remove_all(path); return 0; } catch (...) { return -1; }
}

#undef CK
} // extern "C"
