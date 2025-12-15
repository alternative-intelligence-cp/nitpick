/**
 * src/backend/vector_ops.cpp
 * 
 * Aria Vector Operations - Implementation
 * Runtime support for vec2/vec3/vec4 operations
 */

#include "vector_ops.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace aria {
namespace backend {

// ========== Vec2 Operations ==========

Vec2 Vec2Ops::add(const Vec2& a, const Vec2& b) {
    return Vec2(a.x + b.x, a.y + b.y);
}

Vec2 Vec2Ops::sub(const Vec2& a, const Vec2& b) {
    return Vec2(a.x - b.x, a.y - b.y);
}

Vec2 Vec2Ops::mul(const Vec2& a, const Vec2& b) {
    return Vec2(a.x * b.x, a.y * b.y);
}

Vec2 Vec2Ops::div(const Vec2& a, const Vec2& b) {
    return Vec2(a.x / b.x, a.y / b.y);
}

Vec2 Vec2Ops::scale(const Vec2& v, float scalar) {
    return Vec2(v.x * scalar, v.y * scalar);
}

Vec2 Vec2Ops::negate(const Vec2& v) {
    return Vec2(-v.x, -v.y);
}

float Vec2Ops::dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

float Vec2Ops::length(const Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float Vec2Ops::lengthSquared(const Vec2& v) {
    return v.x * v.x + v.y * v.y;
}

Vec2 Vec2Ops::normalize(const Vec2& v) {
    float len = length(v);
    if (len < 1e-8f) {
        return Vec2(0.0f, 0.0f);  // Avoid division by zero
    }
    return Vec2(v.x / len, v.y / len);
}

float Vec2Ops::distance(const Vec2& a, const Vec2& b) {
    Vec2 diff = sub(a, b);
    return length(diff);
}

bool Vec2Ops::equals(const Vec2& a, const Vec2& b, float epsilon) {
    return std::fabs(a.x - b.x) < epsilon && std::fabs(a.y - b.y) < epsilon;
}

std::string Vec2Ops::toString(const Vec2& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "vec2(" << v.x << ", " << v.y << ")";
    return oss.str();
}

// ========== Vec3 Operations ==========

Vec3 Vec3Ops::add(const Vec3& a, const Vec3& b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 Vec3Ops::sub(const Vec3& a, const Vec3& b) {
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 Vec3Ops::mul(const Vec3& a, const Vec3& b) {
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

Vec3 Vec3Ops::div(const Vec3& a, const Vec3& b) {
    return Vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

Vec3 Vec3Ops::scale(const Vec3& v, float scalar) {
    return Vec3(v.x * scalar, v.y * scalar, v.z * scalar);
}

Vec3 Vec3Ops::negate(const Vec3& v) {
    return Vec3(-v.x, -v.y, -v.z);
}

float Vec3Ops::dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Vec3Ops::cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float Vec3Ops::length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float Vec3Ops::lengthSquared(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

Vec3 Vec3Ops::normalize(const Vec3& v) {
    float len = length(v);
    if (len < 1e-8f) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return Vec3(v.x / len, v.y / len, v.z / len);
}

float Vec3Ops::distance(const Vec3& a, const Vec3& b) {
    Vec3 diff = sub(a, b);
    return length(diff);
}

bool Vec3Ops::equals(const Vec3& a, const Vec3& b, float epsilon) {
    return std::fabs(a.x - b.x) < epsilon && 
           std::fabs(a.y - b.y) < epsilon && 
           std::fabs(a.z - b.z) < epsilon;
}

std::string Vec3Ops::toString(const Vec3& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    return oss.str();
}

// ========== Vec4 Operations ==========

Vec4 Vec4Ops::add(const Vec4& a, const Vec4& b) {
    return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

Vec4 Vec4Ops::sub(const Vec4& a, const Vec4& b) {
    return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

Vec4 Vec4Ops::mul(const Vec4& a, const Vec4& b) {
    return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

Vec4 Vec4Ops::div(const Vec4& a, const Vec4& b) {
    return Vec4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

Vec4 Vec4Ops::scale(const Vec4& v, float scalar) {
    return Vec4(v.x * scalar, v.y * scalar, v.z * scalar, v.w * scalar);
}

Vec4 Vec4Ops::negate(const Vec4& v) {
    return Vec4(-v.x, -v.y, -v.z, -v.w);
}

float Vec4Ops::dot(const Vec4& a, const Vec4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float Vec4Ops::length(const Vec4& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

float Vec4Ops::lengthSquared(const Vec4& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

Vec4 Vec4Ops::normalize(const Vec4& v) {
    float len = length(v);
    if (len < 1e-8f) {
        return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return Vec4(v.x / len, v.y / len, v.z / len, v.w / len);
}

float Vec4Ops::distance(const Vec4& a, const Vec4& b) {
    Vec4 diff = sub(a, b);
    return length(diff);
}

bool Vec4Ops::equals(const Vec4& a, const Vec4& b, float epsilon) {
    return std::fabs(a.x - b.x) < epsilon && 
           std::fabs(a.y - b.y) < epsilon && 
           std::fabs(a.z - b.z) < epsilon && 
           std::fabs(a.w - b.w) < epsilon;
}

std::string Vec4Ops::toString(const Vec4& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    return oss.str();
}

} // namespace backend
} // namespace aria
