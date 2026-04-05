/**
 * @file diagnostics_shim.cpp
 * @brief C bridge for self-hosted Diagnostics Engine (v0.15.0)
 *
 * Provides extern "C" functions for diagnostic storage and data extraction.
 * The formatting/severity logic lives in Aria (stdlib/diagnostics.aria).
 *
 * Pattern: Same as visibility_shim.cpp — state handle + data extraction.
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════
// Diagnostic Types
// ═══════════════════════════════════════════════════════════════════════

// Level constants match Aria-side constants
// 0=NOTE, 1=WARNING, 2=ERROR, 3=FATAL

struct DiagEntry {
    int32_t level;
    std::string filename;
    int32_t line;
    int32_t col;
    int32_t length;
    std::string message;
    std::vector<std::string> notes;
    std::vector<std::string> suggestions;
};

struct DiagState {
    std::vector<DiagEntry> entries;
    int32_t error_count = 0;
    int32_t warning_count = 0;
    int32_t warnings_as_errors = 0;
};

static thread_local std::string diag_str_buf;

static const char* diag_return_str(const std::string& s) {
    diag_str_buf = s;
    return diag_str_buf.c_str();
}

extern "C" {

// ── State management ─────────────────────────────────────────────────

void* diag_new() {
    return new DiagState();
}

int32_t diag_free(void* state) {
    delete (DiagState*)state;
    return 0;
}

// ── Configuration ────────────────────────────────────────────────────

int32_t diag_set_warnings_as_errors(void* state, int32_t enabled) {
    if (!state) return 0;
    ((DiagState*)state)->warnings_as_errors = enabled;
    return 0;
}

int32_t diag_get_warnings_as_errors(void* state) {
    if (!state) return 0;
    return ((DiagState*)state)->warnings_as_errors;
}

// ── Reporting ────────────────────────────────────────────────────────

int32_t diag_report(void* state, int32_t level, const char* filename,
                    int32_t line, int32_t col, int32_t length,
                    const char* message) {
    if (!state) return 0;
    auto* s = (DiagState*)state;

    // Upgrade warnings to errors if configured
    int32_t actual_level = level;
    if (level == 1 && s->warnings_as_errors) {
        actual_level = 2;  // WARNING → ERROR
    }

    DiagEntry e;
    e.level = actual_level;
    e.filename = filename ? filename : "";
    e.line = line;
    e.col = col;
    e.length = length;
    e.message = message ? message : "";
    s->entries.push_back(e);

    if (actual_level == 2 || actual_level == 3) {
        s->error_count++;
    } else if (actual_level == 1) {
        s->warning_count++;
    }
    return 0;
}

// ── Notes & Suggestions ─────────────────────────────────────────────

int32_t diag_add_note(void* state, const char* note) {
    if (!state || !note) return 0;
    auto* s = (DiagState*)state;
    if (!s->entries.empty()) {
        s->entries.back().notes.push_back(std::string(note));
    }
    return 0;
}

int32_t diag_add_suggestion(void* state, const char* suggestion) {
    if (!state || !suggestion) return 0;
    auto* s = (DiagState*)state;
    if (!s->entries.empty()) {
        s->entries.back().suggestions.push_back(std::string(suggestion));
    }
    return 0;
}

// ── Queries ──────────────────────────────────────────────────────────

int32_t diag_count(void* state) {
    if (!state) return 0;
    return (int32_t)((DiagState*)state)->entries.size();
}

int32_t diag_error_count(void* state) {
    if (!state) return 0;
    return ((DiagState*)state)->error_count;
}

int32_t diag_warning_count(void* state) {
    if (!state) return 0;
    return ((DiagState*)state)->warning_count;
}

int32_t diag_has_errors(void* state) {
    if (!state) return 0;
    return ((DiagState*)state)->error_count > 0 ? 1 : 0;
}

int32_t diag_has_warnings(void* state) {
    if (!state) return 0;
    return ((DiagState*)state)->warning_count > 0 ? 1 : 0;
}

// ── Data extraction for individual diagnostics ──────────────────────

int32_t diag_get_level(void* state, int32_t index) {
    if (!state) return -1;
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return -1;
    return s->entries[index].level;
}

const char* diag_get_message(void* state, int32_t index) {
    if (!state) return "";
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return "";
    return diag_return_str(s->entries[index].message);
}

const char* diag_get_filename(void* state, int32_t index) {
    if (!state) return "";
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return "";
    return diag_return_str(s->entries[index].filename);
}

int32_t diag_get_line(void* state, int32_t index) {
    if (!state) return 0;
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return 0;
    return s->entries[index].line;
}

int32_t diag_get_col(void* state, int32_t index) {
    if (!state) return 0;
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return 0;
    return s->entries[index].col;
}

int32_t diag_get_note_count(void* state, int32_t index) {
    if (!state) return 0;
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return 0;
    return (int32_t)s->entries[index].notes.size();
}

const char* diag_get_note(void* state, int32_t diag_index, int32_t note_index) {
    if (!state) return "";
    auto* s = (DiagState*)state;
    if (diag_index < 0 || diag_index >= (int32_t)s->entries.size()) return "";
    auto& notes = s->entries[diag_index].notes;
    if (note_index < 0 || note_index >= (int32_t)notes.size()) return "";
    return diag_return_str(notes[note_index]);
}

int32_t diag_get_suggestion_count(void* state, int32_t index) {
    if (!state) return 0;
    auto* s = (DiagState*)state;
    if (index < 0 || index >= (int32_t)s->entries.size()) return 0;
    return (int32_t)s->entries[index].suggestions.size();
}

const char* diag_get_suggestion(void* state, int32_t diag_index, int32_t sug_index) {
    if (!state) return "";
    auto* s = (DiagState*)state;
    if (diag_index < 0 || diag_index >= (int32_t)s->entries.size()) return "";
    auto& sugs = s->entries[diag_index].suggestions;
    if (sug_index < 0 || sug_index >= (int32_t)sugs.size()) return "";
    return diag_return_str(sugs[sug_index]);
}

// ── Clear ────────────────────────────────────────────────────────────

int32_t diag_clear(void* state) {
    if (!state) return 0;
    auto* s = (DiagState*)state;
    s->entries.clear();
    s->error_count = 0;
    s->warning_count = 0;
    return 0;
}

} // extern "C"
