/**
 * src/backend/vector_runtime.cpp
 * 
 * C linkage wrappers for vector operations
 * Called from LLVM-generated code
 */

#include "vector_ops.h"
#include <cstring>

extern "C" {

// ========== Vec2 Runtime Functions ==========

void _aria_vec2_cross(float* result, const float* a, const float* b) {
    // Vec2 doesn't have cross product (returns 0)
    result[0] = 0.0f;
    result[1] = 0.0f;
}

void _aria_vec2_normalize(float* result, const float* v) {
    aria::backend::Vec2 vec(v[0], v[1]);
    aria::backend::Vec2 norm = aria::backend::Vec2Ops::normalize(vec);
    result[0] = norm.x;
    result[1] = norm.y;
}

// ========== Vec3 Runtime Functions ==========

void _aria_vec3_cross(float* result, const float* a, const float* b) {
    aria::backend::Vec3 va(a[0], a[1], a[2]);
    aria::backend::Vec3 vb(b[0], b[1], b[2]);
    aria::backend::Vec3 cross = aria::backend::Vec3Ops::cross(va, vb);
    result[0] = cross.x;
    result[1] = cross.y;
    result[2] = cross.z;
}

void _aria_vec3_normalize(float* result, const float* v) {
    aria::backend::Vec3 vec(v[0], v[1], v[2]);
    aria::backend::Vec3 norm = aria::backend::Vec3Ops::normalize(vec);
    result[0] = norm.x;
    result[1] = norm.y;
    result[2] = norm.z;
}

// ========== Vec4 Runtime Functions ==========

void _aria_vec4_normalize(float* result, const float* v) {
    aria::backend::Vec4 vec(v[0], v[1], v[2], v[3]);
    aria::backend::Vec4 norm = aria::backend::Vec4Ops::normalize(vec);
    result[0] = norm.x;
    result[1] = norm.y;
    result[2] = norm.z;
    result[3] = norm.w;
}

} // extern "C"
