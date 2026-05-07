// ═══════════════════════════════════════════════════════════════════════
// doc_generator_shim.cpp — C bridge for the self-hosted Doc Generator
// v0.15.2 Port 10
// ═══════════════════════════════════════════════════════════════════════
// Exposes: source file parsing, doc comment extraction, item enumeration,
//          HTML generation, module/item metadata access.
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// ─── Item kind constants (must match Aria-side) ──────────────────────
enum DocItemKind : int32_t {
    KIND_MODULE     = 0,
    KIND_FUNCTION   = 1,
    KIND_STRUCT     = 2,
    KIND_ENUM       = 3,
    KIND_TRAIT      = 4,
    KIND_IMPL       = 5,
    KIND_TYPE_ALIAS = 6,
    KIND_CONST      = 7,
    KIND_VARIABLE   = 8
};

// ─── Visibility constants ────────────────────────────────────────────
enum DocVis : int32_t {
    VIS_PUBLIC   = 0,
    VIS_PRIVATE  = 1,
    VIS_INTERNAL = 2
};

// ─── Annotation tag constants ────────────────────────────────────────
enum DocAnnotTag : int32_t {
    TAG_PARAM      = 0,
    TAG_RETURN     = 1,
    TAG_THROWS     = 2,
    TAG_EXAMPLE    = 3,
    TAG_DEPRECATED = 4,
    TAG_SEE        = 5,
    TAG_SINCE      = 6,
    TAG_NOTE       = 7,
    TAG_WARNING    = 8,
    TAG_TYPE       = 9
};

// ─── Internal structures ─────────────────────────────────────────────
struct DocAnnotation {
    int32_t tag;
    std::string name;
    std::string description;
};

struct DocComment {
    std::string summary;
    std::string description;
    std::vector<DocAnnotation> annotations;
    std::vector<std::string> examples;
};

struct DocItem {
    int32_t kind;
    int32_t visibility;
    std::string name;
    std::string signature;
    DocComment doc;
    std::string file_path;
    int32_t line_number;
    bool is_deprecated;
};

struct DocModule {
    std::string name;
    std::string path;
    DocComment module_doc;
    std::vector<DocItem> items;
};

// ─── Thread-local string return buffer ───────────────────────────────
static thread_local std::string g_ret_buf;
static const char* ret(const std::string& s) {
    g_ret_buf = s;
    return g_ret_buf.c_str();
}

// ─── Helper: trim ────────────────────────────────────────────────────
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// ─── Helper: split ───────────────────────────────────────────────────
static std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

// ─── Helper: join ────────────────────────────────────────────────────
static std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) r += sep;
        r += v[i];
    }
    return r;
}

// ─── Parse annotation line ───────────────────────────────────────────
static bool parse_annotation_line(const std::string& line, DocAnnotation& ann) {
    if (line.empty() || line[0] != '@') return false;

    size_t space = line.find(' ');
    std::string tag_str = line.substr(1, space == std::string::npos ? std::string::npos : space - 1);

    if (tag_str == "param" || tag_str == "parameter")      ann.tag = TAG_PARAM;
    else if (tag_str == "return" || tag_str == "returns")   ann.tag = TAG_RETURN;
    else if (tag_str == "throws" || tag_str == "panics")    ann.tag = TAG_THROWS;
    else if (tag_str == "example" || tag_str == "examples") ann.tag = TAG_EXAMPLE;
    else if (tag_str == "deprecated")                       ann.tag = TAG_DEPRECATED;
    else if (tag_str == "see" || tag_str == "seealso")      ann.tag = TAG_SEE;
    else if (tag_str == "since")                            ann.tag = TAG_SINCE;
    else if (tag_str == "note")                             ann.tag = TAG_NOTE;
    else if (tag_str == "warning")                          ann.tag = TAG_WARNING;
    else if (tag_str == "type")                             ann.tag = TAG_TYPE;
    else return false;

    if (space != std::string::npos) {
        std::string rest = trim(line.substr(space + 1));
        if (ann.tag == TAG_PARAM) {
            size_t ne = rest.find(' ');
            if (ne != std::string::npos) {
                ann.name = rest.substr(0, ne);
                ann.description = trim(rest.substr(ne + 1));
            } else {
                ann.name = rest;
            }
        } else {
            ann.description = rest;
        }
    }
    return true;
}

// ─── Parse a doc comment block ───────────────────────────────────────
static DocComment parse_doc_text(const std::string& raw_text) {
    DocComment doc;
    auto lines = split(raw_text, '\n');
    std::vector<std::string> text_lines;

    for (const auto& line : lines) {
        std::string t = trim(line);
        DocAnnotation ann;
        if (parse_annotation_line(t, ann)) {
            doc.annotations.push_back(ann);
        } else {
            text_lines.push_back(t);
        }
    }

    std::string full = join(text_lines, "\n");
    size_t dn = full.find("\n\n");
    if (dn != std::string::npos) {
        doc.summary = trim(full.substr(0, dn));
        doc.description = trim(full.substr(dn + 2));
    } else {
        doc.summary = trim(full);
    }

    // Extract code blocks
    std::regex code_re(R"(```(?:aria)?\s*\n(.*?)```)");
    auto begin = std::sregex_iterator(full.begin(), full.end(), code_re);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        doc.examples.push_back((*it)[1].str());
    }

    return doc;
}

// ─── Determine item kind from code ───────────────────────────────────
static int32_t determine_kind(const std::string& code) {
    if (code.find("func:") != std::string::npos)   return KIND_FUNCTION;
    if (code.find("struct:") != std::string::npos)  return KIND_STRUCT;
    if (code.find("enum:") != std::string::npos)    return KIND_ENUM;
    if (code.find("impl:") != std::string::npos)    return KIND_IMPL;
    if (code.find("trait:") != std::string::npos)    return KIND_TRAIT;
    if (code.find("type:") != std::string::npos)    return KIND_TYPE_ALIAS;
    if (code.find("const ") != std::string::npos)   return KIND_CONST;
    return KIND_VARIABLE;
}

// ─── Extract item name ───────────────────────────────────────────────
static std::string extract_name(const std::string& code, int32_t kind) {
    std::regex re;
    switch (kind) {
        case KIND_FUNCTION:  re = std::regex(R"(func:([a-zA-Z_]\w*))"); break;
        case KIND_STRUCT:    re = std::regex(R"(struct:([a-zA-Z_]\w*))"); break;
        case KIND_ENUM:      re = std::regex(R"(enum:([a-zA-Z_]\w*))"); break;
        case KIND_TRAIT:     re = std::regex(R"(trait:([a-zA-Z_]\w*))"); break;
        case KIND_TYPE_ALIAS:re = std::regex(R"(type:([a-zA-Z_]\w*))"); break;
        case KIND_CONST:     re = std::regex(R"(const\s+\w+:([a-zA-Z_]\w*))"); break;
        case KIND_IMPL:      re = std::regex(R"(impl:\w+:for:([a-zA-Z_]\w*))"); break;
        default:             re = std::regex(R"(\w+:([a-zA-Z_]\w*)\s*=)"); break;
    }
    std::smatch m;
    if (std::regex_search(code, m, re)) return m[1].str();
    return "unknown";
}

// ─── Extract function signature ──────────────────────────────────────
static std::string extract_signature(const std::string& code) {
    size_t fn = code.find("func:");
    if (fn == std::string::npos) return code;

    int depth = 0;
    bool found_open = false;
    size_t end_pos = fn;

    for (size_t i = fn; i < code.size(); ++i) {
        if (code[i] == '(') { found_open = true; depth++; }
        else if (code[i] == ')') {
            depth--;
            if (depth == 0 && found_open) {
                size_t brace = code.find('{', i);
                end_pos = (brace != std::string::npos) ? brace : code.size();
                break;
            }
        }
    }
    return trim(code.substr(fn, end_pos - fn));
}

// ─── Extract visibility ──────────────────────────────────────────────
static int32_t extract_visibility(const std::string& code) {
    std::string t = trim(code);
    if (t.rfind("pub ", 0) == 0) return VIS_PUBLIC;
    if (t.rfind("internal ", 0) == 0) return VIS_INTERNAL;
    return VIS_PRIVATE;
}

// ─── Parse a single source file ──────────────────────────────────────
static DocModule parse_source(const std::string& file_path) {
    DocModule mod;
    mod.path = file_path;

    // Extract module name from path
    size_t slash = file_path.find_last_of("/\\");
    size_t dot   = file_path.find_last_of(".");
    if (slash != std::string::npos && dot != std::string::npos && dot > slash) {
        mod.name = file_path.substr(slash + 1, dot - slash - 1);
    } else {
        mod.name = file_path;
    }

    // Read file
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return mod;
    std::stringstream buf;
    buf << ifs.rdbuf();
    std::string source = buf.str();

    auto lines = split(source, '\n');
    std::vector<std::string> doc_lines;
    std::vector<std::string> code_lines;
    int brace_depth = 0;
    bool in_multi = false;
    int doc_start = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string t = trim(lines[i]);

        // Doc comments
        if (!in_multi && t.rfind("///", 0) == 0) {
            if (doc_lines.empty()) doc_start = (int)i + 1;
            doc_lines.push_back(t.substr(3));
            continue;
        }
        if (!in_multi && t.rfind("//!", 0) == 0) {
            if (doc_lines.empty()) doc_start = (int)i + 1;
            doc_lines.push_back(t.substr(3));
            // Inner doc — module level
            if (code_lines.empty() && brace_depth == 0) {
                mod.module_doc = parse_doc_text(join(doc_lines, "\n"));
                doc_lines.clear();
            }
            continue;
        }
        if (!in_multi && t.rfind("/**", 0) == 0) {
            in_multi = true;
            doc_start = (int)i + 1;
            std::string content = t.substr(3);
            if (content.size() >= 2 && content.substr(content.size() - 2) == "*/") {
                content = content.substr(0, content.size() - 2);
                in_multi = false;
            }
            doc_lines.push_back(content);
            continue;
        }
        if (in_multi) {
            if (t.size() >= 2 && t.substr(t.size() - 2) == "*/") {
                std::string c = t.substr(0, t.size() - 2);
                if (!c.empty() && c[0] == '*') c = c.substr(1);
                doc_lines.push_back(trim(c));
                in_multi = false;
            } else {
                std::string c = t;
                if (!c.empty() && c[0] == '*') c = c.substr(1);
                doc_lines.push_back(trim(c));
            }
            continue;
        }

        // Skip blank / non-doc comments
        if (t.empty() || t.rfind("//", 0) == 0) continue;

        // Code line
        code_lines.push_back(lines[i]);

        for (char ch : t) {
            if (ch == '{') brace_depth++;
            else if (ch == '}') brace_depth--;
        }

        if (brace_depth <= 0 && !t.empty() && (t.back() == '}' || t.back() == ';')) {
            std::string code_block = join(code_lines, "\n");
            int32_t kind = determine_kind(code_block);

            if (kind != KIND_VARIABLE) {
                DocItem item;
                item.kind = kind;
                item.visibility = extract_visibility(code_block);
                item.name = extract_name(code_block, kind);
                item.file_path = file_path;
                item.line_number = doc_start > 0 ? doc_start : (int)(i + 1);

                if (kind == KIND_FUNCTION) {
                    item.signature = extract_signature(code_block);
                }

                if (!doc_lines.empty()) {
                    item.doc = parse_doc_text(join(doc_lines, "\n"));
                    item.is_deprecated = false;
                    for (const auto& a : item.doc.annotations) {
                        if (a.tag == TAG_DEPRECATED) { item.is_deprecated = true; break; }
                    }
                }

                mod.items.push_back(item);
            }

            code_lines.clear();
            doc_lines.clear();
            brace_depth = 0;
        }
    }

    return mod;
}

// ─── Generate HTML page header ───────────────────────────────────────
static std::string html_header(const std::string& title) {
    return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
           "<meta charset=\"UTF-8\">\n"
           "<title>" + title + "</title>\n"
           "<style>\n"
           "body{font-family:sans-serif;max-width:900px;margin:0 auto;padding:20px;color:#333;}\n"
           "h1{color:#6a0dad;}h2{color:#7a1db8;border-bottom:2px solid #6a0dad;padding-bottom:8px;}\n"
           ".badge{background:#6a0dad;color:#fff;padding:3px 10px;border-radius:12px;font-size:.85em;}\n"
           "pre{background:#f8f8f8;border-left:4px solid #6a0dad;padding:12px;overflow-x:auto;}\n"
           "code{font-family:monospace;}.item{padding:10px;margin:8px 0;border-left:3px solid #6a0dad;background:#fafafa;}\n"
           ".item a{color:#6a0dad;font-weight:bold;text-decoration:none;}\n"
           ".summary{color:#555;margin:15px 0;padding:10px;background:#f9f9f9;border-left:4px solid #6a0dad;}\n"
           "footer{margin-top:40px;border-top:2px solid #ddd;text-align:center;color:#999;padding:15px 0;}\n"
           "</style>\n</head>\n<body>\n";
}

static std::string html_footer() {
    return "<footer>Generated by npk-doc</footer>\n</body>\n</html>\n";
}

// ─── Kind label ──────────────────────────────────────────────────────
static const char* kind_label(int32_t k) {
    switch (k) {
        case KIND_FUNCTION:  return "Function";
        case KIND_STRUCT:    return "Struct";
        case KIND_ENUM:      return "Enum";
        case KIND_TRAIT:     return "Trait";
        case KIND_IMPL:      return "Impl";
        case KIND_TYPE_ALIAS:return "Type Alias";
        case KIND_CONST:     return "Constant";
        default:             return "Item";
    }
}

// ─── Generate index page ─────────────────────────────────────────────
static bool generate_index(const DocModule& mod, const std::string& out_dir) {
    std::string path = out_dir + "/index.html";
    std::ofstream out(path);
    if (!out) return false;

    out << html_header(mod.name + " — API Docs");
    out << "<h1>Module: " << mod.name << "</h1>\n";

    if (!mod.module_doc.summary.empty()) {
        out << "<div class=\"summary\">" << mod.module_doc.summary << "</div>\n";
    }

    // Group items
    std::map<int32_t, std::vector<const DocItem*>> groups;
    for (const auto& item : mod.items) {
        if (item.kind == KIND_IMPL || item.kind == KIND_VARIABLE) continue;
        groups[item.kind].push_back(&item);
    }

    const int32_t order[] = {KIND_FUNCTION, KIND_STRUCT, KIND_ENUM, KIND_TRAIT, KIND_TYPE_ALIAS, KIND_CONST};
    const char* section_titles[] = {"Functions", "Structs", "Enums", "Traits", "Type Aliases", "Constants"};

    for (int i = 0; i < 6; ++i) {
        auto it = groups.find(order[i]);
        if (it == groups.end() || it->second.empty()) continue;
        out << "<h2>" << section_titles[i] << "</h2>\n";
        for (const auto* item : it->second) {
            out << "<div class=\"item\">\n";
            out << "<a href=\"" << item->name << ".html\">" << item->name << "</a>\n";
            if (!item->signature.empty()) {
                out << "<pre><code>" << item->signature << "</code></pre>\n";
            }
            if (!item->doc.summary.empty()) {
                out << "<p>" << item->doc.summary << "</p>\n";
            }
            out << "</div>\n";
        }
    }

    out << html_footer();
    return true;
}

// ─── Generate item page ─────────────────────────────────────────────
static bool generate_item_page(const DocItem& item, const std::string& out_dir) {
    std::string path = out_dir + "/" + item.name + ".html";
    std::ofstream out(path);
    if (!out) return false;

    out << html_header(item.name);
    out << "<span class=\"badge\">" << kind_label(item.kind) << "</span>\n";
    out << "<h1>" << item.name << "</h1>\n";

    if (!item.signature.empty()) {
        out << "<pre><code>" << item.signature << "</code></pre>\n";
    }
    if (!item.doc.summary.empty()) {
        out << "<div class=\"summary\">" << item.doc.summary << "</div>\n";
    }
    if (!item.doc.description.empty()) {
        out << "<p>" << item.doc.description << "</p>\n";
    }

    // Annotations
    for (const auto& ann : item.doc.annotations) {
        const char* tag_names[] = {"Parameter", "Returns", "Throws", "Example",
                                   "Deprecated", "See Also", "Since", "Note",
                                   "Warning", "Type"};
        const char* tname = (ann.tag >= 0 && ann.tag <= 9) ? tag_names[ann.tag] : "Info";
        out << "<p><strong>" << tname;
        if (!ann.name.empty()) out << " <code>" << ann.name << "</code>";
        out << ":</strong> " << ann.description << "</p>\n";
    }

    // Examples
    if (!item.doc.examples.empty()) {
        out << "<h2>Examples</h2>\n";
        for (const auto& ex : item.doc.examples) {
            out << "<pre><code>" << ex << "</code></pre>\n";
        }
    }

    out << "<p style=\"color:#999;font-size:.9em;\">Defined in: <code>"
        << item.file_path << ":" << item.line_number << "</code></p>\n";
    out << html_footer();
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// extern "C" API
// ═══════════════════════════════════════════════════════════════════════

static std::vector<DocModule*> g_modules;

extern "C" {

// ─── Lifecycle ───────────────────────────────────────────────────────
int64_t doc_parse_file(const char* path) {
    if (!path) return -1;
    try {
        auto* mod = new DocModule(parse_source(path));
        g_modules.push_back(mod);
        return (int64_t)(g_modules.size() - 1);
    } catch (...) {
        return -1;
    }
}

int32_t doc_free_module(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size()) return -1;
    if (g_modules[handle]) {
        delete g_modules[handle];
        g_modules[handle] = nullptr;
    }
    return 0;
}

// ─── Module info ─────────────────────────────────────────────────────
const char* doc_module_name(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    return ret(g_modules[handle]->name);
}

const char* doc_module_path(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    return ret(g_modules[handle]->path);
}

const char* doc_module_summary(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    return ret(g_modules[handle]->module_doc.summary);
}

int32_t doc_item_count(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    return (int32_t)g_modules[handle]->items.size();
}

// ─── Item accessors ──────────────────────────────────────────────────
const char* doc_item_name(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return ret("");
    return ret(items[idx].name);
}

int32_t doc_item_kind(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return -1;
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return -1;
    return items[idx].kind;
}

int32_t doc_item_visibility(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return -1;
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return -1;
    return items[idx].visibility;
}

const char* doc_item_signature(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return ret("");
    return ret(items[idx].signature);
}

const char* doc_item_summary(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return ret("");
    return ret(items[idx].doc.summary);
}

const char* doc_item_description(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return ret("");
    return ret(items[idx].doc.description);
}

int32_t doc_item_is_deprecated(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return 0;
    return items[idx].is_deprecated ? 1 : 0;
}

int32_t doc_item_line(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return 0;
    return items[idx].line_number;
}

int32_t doc_item_annotation_count(int64_t handle, int32_t idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    auto& items = g_modules[handle]->items;
    if (idx < 0 || idx >= (int32_t)items.size()) return 0;
    return (int32_t)items[idx].doc.annotations.size();
}

int32_t doc_item_annotation_tag(int64_t handle, int32_t item_idx, int32_t ann_idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return -1;
    auto& items = g_modules[handle]->items;
    if (item_idx < 0 || item_idx >= (int32_t)items.size()) return -1;
    auto& anns = items[item_idx].doc.annotations;
    if (ann_idx < 0 || ann_idx >= (int32_t)anns.size()) return -1;
    return anns[ann_idx].tag;
}

const char* doc_item_annotation_name(int64_t handle, int32_t item_idx, int32_t ann_idx) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return ret("");
    auto& items = g_modules[handle]->items;
    if (item_idx < 0 || item_idx >= (int32_t)items.size()) return ret("");
    auto& anns = items[item_idx].doc.annotations;
    if (ann_idx < 0 || ann_idx >= (int32_t)anns.size()) return ret("");
    return ret(anns[ann_idx].name);
}

// ─── Counting helpers ────────────────────────────────────────────────
int32_t doc_count_by_kind(int64_t handle, int32_t kind) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    int32_t count = 0;
    for (const auto& item : g_modules[handle]->items) {
        if (item.kind == kind) count++;
    }
    return count;
}

int32_t doc_count_public(int64_t handle) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return 0;
    int32_t count = 0;
    for (const auto& item : g_modules[handle]->items) {
        if (item.visibility == VIS_PUBLIC) count++;
    }
    return count;
}

// ─── HTML generation ─────────────────────────────────────────────────
int32_t doc_generate_html(int64_t handle, const char* output_dir) {
    if (handle < 0 || handle >= (int64_t)g_modules.size() || !g_modules[handle])
        return -1;
    if (!output_dir) return -1;

    try {
        fs::create_directories(output_dir);
    } catch (...) {
        return -1;
    }

    auto& mod = *g_modules[handle];

    if (!generate_index(mod, output_dir)) return -1;

    for (const auto& item : mod.items) {
        if (item.kind == KIND_IMPL || item.kind == KIND_VARIABLE) continue;
        if (!generate_item_page(item, output_dir)) return -1;
    }

    return 0;
}

int32_t doc_index_exists(const char* output_dir) {
    if (!output_dir) return 0;
    return fs::exists(std::string(output_dir) + "/index.html") ? 1 : 0;
}

int32_t doc_item_page_exists(const char* output_dir, const char* item_name) {
    if (!output_dir || !item_name) return 0;
    return fs::exists(std::string(output_dir) + "/" + item_name + ".html") ? 1 : 0;
}

// ─── Kind/Visibility label helpers ───────────────────────────────────
const char* doc_kind_label(int32_t kind) {
    return ret(kind_label(kind));
}

const char* doc_visibility_label(int32_t vis) {
    switch (vis) {
        case VIS_PUBLIC:   return ret("public");
        case VIS_PRIVATE:  return ret("private");
        case VIS_INTERNAL: return ret("internal");
        default:           return ret("unknown");
    }
}

// ─── Test fixture helper ─────────────────────────────────────────────
// Creates a sample .aria file that tests can parse. Avoids the need for
// long string literals in Aria (which hit a codegen size limit).
int64_t doc_create_test_fixture(const char* path) {
    std::ofstream out(path);
    if (!out.is_open()) return -1;
    out << "/// Add two integers together.\n"
        << "/// This is a pure math helper.\n"
        << "/// @param a The left operand\n"
        << "/// @param b The right operand\n"
        << "/// @return The sum\n"
        << "/// @since v0.1.0\n"
        << "pub func:add = int32(int32:a, int32:b) {\n"
        << "    pass a + b;\n"
        << "};\n\n"
        << "/// A 2D point.\n"
        << "struct:Point {\n"
        << "    int32:x;\n"
        << "    int32:y;\n"
        << "};\n\n"
        << "/// @deprecated Use add instead.\n"
        << "pub func:old_add = int32(int32:a, int32:b) {\n"
        << "    pass a + b;\n"
        << "};\n\n"
        << "func:private_helper = int32() {\n"
        << "    pass 0i32;\n"
        << "};\n\n"
        << "/// Color choices.\n"
        << "pub enum:Color {\n"
        << "    RED;\n"
        << "    GREEN;\n"
        << "    BLUE;\n"
        << "};\n\n"
        << "const int32:MAX_SIZE = 1024i32;\n";
    out.close();
    return 0;
}

} // extern "C"
