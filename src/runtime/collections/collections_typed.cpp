/**
 * Typed wrappers for Aria collections runtime.
 * 
 * The generic collection functions use void* for element pointers, but Aria's
 * type checker requires matching pointer types (int8@ matches wild int8*, but
 * int64@ does not). These thin wrappers accept typed values by value and
 * internally handle the address-of operation.
 */

#include <cstdint>
#include <cstring>

// Forward declarations - these are C-linkage symbols from the runtime
extern "C" {
extern void* aria_array_new_simple(size_t element_size, int type_id);
extern void  aria_array_push_simple(void* arr, const void* val);
extern void  aria_array_pop_simple(void* arr, void* out_val);
extern void* aria_array_get_simple(void* arr, size_t index);
extern void  aria_array_set_simple(void* arr, size_t index, const void* val);
extern size_t aria_array_length(const void* arr);
extern void  aria_array_free(void* arr);

extern void* aria_map_new_simple(size_t key_size, size_t value_size);
extern void  aria_map_insert_simple(void* map, const void* key, const void* value);
extern void* aria_map_get_simple(void* map, const void* key);
extern int32_t aria_map_has(void* map, const void* key);
extern void  aria_map_remove(void* map, const void* key);
extern int64_t aria_map_length(const void* map);
extern void  aria_map_clear(void* map);
extern void  aria_map_free(void* map);
}

extern "C" {

// ═══════════════════════════════════════════════════════════════════════
// Vec<int64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_i64_new(void) {
    return aria_array_new_simple(8, 1);
}

void vec_i64_push(void* arr, int64_t val) {
    aria_array_push_simple(arr, &val);
}

int64_t vec_i64_pop(void* arr) {
    int64_t v = 0;
    aria_array_pop_simple(arr, &v);
    return v;
}

int64_t vec_i64_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    int64_t v;
    memcpy(&v, ptr, sizeof(int64_t));
    return v;
}

void vec_i64_set(void* arr, int64_t index, int64_t val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_i64_length(void* arr) {
    return aria_array_length(arr);
}

void vec_i64_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Vec<int32> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_i32_new(void) {
    return aria_array_new_simple(4, 1);
}

void vec_i32_push(void* arr, int32_t val) {
    aria_array_push_simple(arr, &val);
}

int32_t vec_i32_pop(void* arr) {
    int32_t v = 0;
    aria_array_pop_simple(arr, &v);
    return v;
}

int32_t vec_i32_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    int32_t v;
    memcpy(&v, ptr, sizeof(int32_t));
    return v;
}

void vec_i32_set(void* arr, int64_t index, int32_t val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_i32_length(void* arr) {
    return aria_array_length(arr);
}

void vec_i32_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Vec<flt64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_f64_new(void) {
    return aria_array_new_simple(8, 1);
}

void vec_f64_push(void* arr, double val) {
    aria_array_push_simple(arr, &val);
}

double vec_f64_pop(void* arr) {
    double v = 0.0;
    aria_array_pop_simple(arr, &v);
    return v;
}

double vec_f64_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    double v;
    memcpy(&v, ptr, sizeof(double));
    return v;
}

void vec_f64_set(void* arr, int64_t index, double val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_f64_length(void* arr) {
    return aria_array_length(arr);
}

void vec_f64_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Map<int64, int64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* map_i64_new(void) {
    return aria_map_new_simple(8, 8);
}

void map_i64_insert(void* map, int64_t key, int64_t val) {
    aria_map_insert_simple(map, &key, &val);
}

int64_t map_i64_get(void* map, int64_t key) {
    void* ptr = aria_map_get_simple(map, &key);
    int64_t v;
    memcpy(&v, ptr, sizeof(int64_t));
    return v;
}

int32_t map_i64_has(void* map, int64_t key) {
    return aria_map_has(map, &key);
}

void map_i64_remove(void* map, int64_t key) {
    aria_map_remove(map, &key);
}

int64_t map_i64_length(void* map) {
    return aria_map_length(map);
}

void map_i64_clear(void* map) {
    aria_map_clear(map);
}

void map_i64_free(void* map) {
    aria_map_free(map);
}

// ═══════════════════════════════════════════════════════════════════════
// Map<int32, int64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* map_i32_i64_new(void) {
    return aria_map_new_simple(4, 8);
}

void map_i32_i64_insert(void* map, int32_t key, int64_t val) {
    aria_map_insert_simple(map, &key, &val);
}

int32_t map_i32_i64_get(void* map, int32_t key, int64_t* out_value) {
    void* ptr = aria_map_get_simple(map, &key);
    if (!ptr) return 0;
    memcpy(out_value, ptr, sizeof(int64_t));
    return 1;
}

int32_t map_i32_i64_has(void* map, int32_t key) {
    return aria_map_has(map, &key);
}

int32_t map_i32_i64_remove(void* map, int32_t key) {
    if (!aria_map_has(map, &key)) return 0;
    aria_map_remove(map, &key);
    return 1;
}

int64_t map_i32_i64_length(void* map) {
    return aria_map_length(map);
}

void map_i32_i64_clear(void* map) {
    aria_map_clear(map);
}

void map_i32_i64_free(void* map) {
    aria_map_free(map);
}

// ═══════════════════════════════════════════════════════════════════════
// Map<int8, int8> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* map_i8_i8_new(void) {
    return aria_map_new_simple(1, 1);
}

void map_i8_i8_insert(void* map, int8_t key, int8_t val) {
    aria_map_insert_simple(map, &key, &val);
}

int32_t map_i8_i8_get(void* map, int8_t key, int8_t* out_value) {
    void* ptr = aria_map_get_simple(map, &key);
    if (!ptr) return 0;
    memcpy(out_value, ptr, sizeof(int8_t));
    return 1;
}

int32_t map_i8_i8_has(void* map, int8_t key) {
    return aria_map_has(map, &key);
}

int32_t map_i8_i8_remove(void* map, int8_t key) {
    if (!aria_map_has(map, &key)) return 0;
    aria_map_remove(map, &key);
    return 1;
}

int64_t map_i8_i8_length(void* map) {
    return aria_map_length(map);
}

void map_i8_i8_clear(void* map) {
    aria_map_clear(map);
}

void map_i8_i8_free(void* map) {
    aria_map_free(map);
}

// ═══════════════════════════════════════════════════════════════════════
// Set<int64> typed wrappers (thin wrapper over Map<int64, int64>)
// ═══════════════════════════════════════════════════════════════════════

void* set_i64_new(void) {
    return aria_map_new_simple(8, 8);
}

void set_i64_add(void* set, int64_t val) {
    int64_t dummy = 1;
    aria_map_insert_simple(set, &val, &dummy);
}

int32_t set_i64_has(void* set, int64_t val) {
    return aria_map_has(set, &val);
}

void set_i64_remove(void* set, int64_t val) {
    aria_map_remove(set, &val);
}

int64_t set_i64_length(void* set) {
    return aria_map_length(set);
}

void set_i64_clear(void* set) {
    aria_map_clear(set);
}

void set_i64_free(void* set) {
    aria_map_free(set);
}

// ═══════════════════════════════════════════════════════════════════════
// Graph (directed, int64 node IDs) — adjacency list via Map + Vec
// ═══════════════════════════════════════════════════════════════════════

struct AriaGraph {
    void* adj_map;       // Map<int64, int64> where values are Vec handles cast to int64
    void* node_list;     // Vec<int64> of all node IDs (for iteration during free)
    int64_t edge_count;
};

void* graph_new(void) {
    AriaGraph* g = new AriaGraph();
    g->adj_map = aria_map_new_simple(8, 8);
    g->node_list = aria_array_new_simple(8, 1);
    g->edge_count = 0;
    return g;
}

void graph_add_node(void* graph, int64_t node_id) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    // Only add if node doesn't exist
    if (!aria_map_has(g->adj_map, &node_id)) {
        void* neighbors = aria_array_new_simple(8, 1);
        int64_t handle = reinterpret_cast<int64_t>(neighbors);
        aria_map_insert_simple(g->adj_map, &node_id, &handle);
        aria_array_push_simple(g->node_list, &node_id);
    }
}

int32_t graph_has_node(void* graph, int64_t node_id) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    return aria_map_has(g->adj_map, &node_id);
}

void graph_add_edge(void* graph, int64_t from, int64_t to) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    // Auto-add nodes if they don't exist
    graph_add_node(graph, from);
    graph_add_node(graph, to);
    // Get the neighbors Vec for 'from'
    void* handle_ptr = aria_map_get_simple(g->adj_map, &from);
    int64_t handle_val;
    memcpy(&handle_val, handle_ptr, sizeof(int64_t));
    void* neighbors = reinterpret_cast<void*>(handle_val);
    aria_array_push_simple(neighbors, &to);
    g->edge_count++;
}

int32_t graph_has_edge(void* graph, int64_t from, int64_t to) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    if (!aria_map_has(g->adj_map, &from)) return 0;
    void* handle_ptr = aria_map_get_simple(g->adj_map, &from);
    int64_t handle_val;
    memcpy(&handle_val, handle_ptr, sizeof(int64_t));
    void* neighbors = reinterpret_cast<void*>(handle_val);
    int64_t len = static_cast<int64_t>(aria_array_length(neighbors));
    for (int64_t i = 0; i < len; i++) {
        void* elem_ptr = aria_array_get_simple(neighbors, i);
        int64_t elem;
        memcpy(&elem, elem_ptr, sizeof(int64_t));
        if (elem == to) return 1;
    }
    return 0;
}

int64_t graph_degree(void* graph, int64_t node_id) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    if (!aria_map_has(g->adj_map, &node_id)) return 0;
    void* handle_ptr = aria_map_get_simple(g->adj_map, &node_id);
    int64_t handle_val;
    memcpy(&handle_val, handle_ptr, sizeof(int64_t));
    void* neighbors = reinterpret_cast<void*>(handle_val);
    return static_cast<int64_t>(aria_array_length(neighbors));
}

int64_t graph_neighbor_at(void* graph, int64_t node_id, int64_t index) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    void* handle_ptr = aria_map_get_simple(g->adj_map, &node_id);
    int64_t handle_val;
    memcpy(&handle_val, handle_ptr, sizeof(int64_t));
    void* neighbors = reinterpret_cast<void*>(handle_val);
    void* elem_ptr = aria_array_get_simple(neighbors, index);
    int64_t elem;
    memcpy(&elem, elem_ptr, sizeof(int64_t));
    return elem;
}

int64_t graph_node_count(void* graph) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    return aria_map_length(g->adj_map);
}

int64_t graph_edge_count(void* graph) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    return g->edge_count;
}

void graph_remove_edge(void* graph, int64_t from, int64_t to) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    if (!aria_map_has(g->adj_map, &from)) return;
    void* handle_ptr = aria_map_get_simple(g->adj_map, &from);
    int64_t handle_val;
    memcpy(&handle_val, handle_ptr, sizeof(int64_t));
    void* neighbors = reinterpret_cast<void*>(handle_val);
    int64_t len = static_cast<int64_t>(aria_array_length(neighbors));
    for (int64_t i = 0; i < len; i++) {
        void* elem_ptr = aria_array_get_simple(neighbors, i);
        int64_t elem;
        memcpy(&elem, elem_ptr, sizeof(int64_t));
        if (elem == to) {
            // Swap with last and pop
            if (i < len - 1) {
                void* last_ptr = aria_array_get_simple(neighbors, len - 1);
                int64_t last_val;
                memcpy(&last_val, last_ptr, sizeof(int64_t));
                aria_array_set_simple(neighbors, i, &last_val);
            }
            int64_t dummy;
            aria_array_pop_simple(neighbors, &dummy);
            g->edge_count--;
            return;
        }
    }
}

void graph_free(void* graph) {
    AriaGraph* g = static_cast<AriaGraph*>(graph);
    // Free all neighbor Vecs by iterating the node list
    size_t n = aria_array_length(g->node_list);
    for (size_t i = 0; i < n; i++) {
        void* nid_ptr = aria_array_get_simple(g->node_list, i);
        int64_t nid;
        memcpy(&nid, nid_ptr, sizeof(int64_t));
        if (aria_map_has(g->adj_map, &nid)) {
            void* handle_ptr = aria_map_get_simple(g->adj_map, &nid);
            int64_t handle_val;
            memcpy(&handle_val, handle_ptr, sizeof(int64_t));
            void* neighbors = reinterpret_cast<void*>(handle_val);
            aria_array_free(neighbors);
        }
    }
    aria_array_free(g->node_list);
    aria_map_free(g->adj_map);
    delete g;
}

}  // extern "C"
