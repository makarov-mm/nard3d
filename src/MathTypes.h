#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

// Small vector and matrix types. Matrices are column-major, like OpenGL's, and multiply column vectors.
namespace math {

constexpr float Pi = std::numbers::pi_v<float>;

struct Vec2 {
    float x = 0, y = 0;
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
constexpr Vec2 operator*(float s, Vec2 a) { return a * s; }

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(float s, Vec3 a) { return a * s; }
constexpr Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) { return a = a + b; }
constexpr Vec3& operator-=(Vec3& a, Vec3 b) { return a = a - b; }
constexpr Vec3& operator*=(Vec3& a, float s) { return a = a * s; }

constexpr float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(Vec2 a) { return std::sqrt(Dot(a, a)); }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Normalize(Vec3 a)
{
    const float length = Length(a);
    return length > 1e-20f ? a / length : Vec3{0, 1, 0};
}

constexpr float Lerp(float a, float b, float t) { return a + (b - a) * t; }
constexpr Vec3 Lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
constexpr float Saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }
constexpr float SmoothStep(float edge0, float edge1, float x)
{
    const float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3 - 2 * t);
}

struct Mat4 {
    std::array<float, 16> m{}; // element (row, column) is m[column * 4 + row]

    constexpr float& operator()(int row, int column) { return m[column * 4 + row]; }
    constexpr float operator()(int row, int column) const { return m[column * 4 + row]; }
    const float* Data() const { return m.data(); }

    static constexpr Mat4 Identity()
    {
        Mat4 result;
        result(0, 0) = result(1, 1) = result(2, 2) = result(3, 3) = 1;
        return result;
    }

    static constexpr Mat4 Translation(Vec3 t)
    {
        Mat4 result = Identity();
        result(0, 3) = t.x;
        result(1, 3) = t.y;
        result(2, 3) = t.z;
        return result;
    }

    static constexpr Mat4 Scale(Vec3 s)
    {
        Mat4 result;
        result(0, 0) = s.x;
        result(1, 1) = s.y;
        result(2, 2) = s.z;
        result(3, 3) = 1;
        return result;
    }

    static Mat4 RotationX(float angle)
    {
        const float c = std::cos(angle), s = std::sin(angle);
        Mat4 result = Identity();
        result(1, 1) = c;
        result(1, 2) = -s;
        result(2, 1) = s;
        result(2, 2) = c;
        return result;
    }

    static Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up)
    {
        const Vec3 forward = Normalize(target - eye);
        const Vec3 right = Normalize(Cross(forward, up));
        const Vec3 trueUp = Cross(right, forward);
        Mat4 result = Identity();
        result(0, 0) = right.x;
        result(0, 1) = right.y;
        result(0, 2) = right.z;
        result(1, 0) = trueUp.x;
        result(1, 1) = trueUp.y;
        result(1, 2) = trueUp.z;
        result(2, 0) = -forward.x;
        result(2, 1) = -forward.y;
        result(2, 2) = -forward.z;
        result(0, 3) = -Dot(right, eye);
        result(1, 3) = -Dot(trueUp, eye);
        result(2, 3) = Dot(forward, eye);
        return result;
    }

    // OpenGL clip space: depth from -1 at the near plane to 1 at the far plane.
    static Mat4 Perspective(float fovY, float aspect, float nearPlane, float farPlane)
    {
        const float f = 1 / std::tan(fovY / 2);
        Mat4 result;
        result(0, 0) = f / aspect;
        result(1, 1) = f;
        result(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        result(2, 3) = 2 * farPlane * nearPlane / (nearPlane - farPlane);
        result(3, 2) = -1;
        return result;
    }

    static constexpr Mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        Mat4 result = Identity();
        result(0, 0) = 2 / (right - left);
        result(1, 1) = 2 / (top - bottom);
        result(2, 2) = -2 / (farPlane - nearPlane);
        result(0, 3) = -(right + left) / (right - left);
        result(1, 3) = -(top + bottom) / (top - bottom);
        result(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        return result;
    }

    constexpr Mat4 operator*(const Mat4& other) const
    {
        Mat4 result;
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column) {
                float sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += (*this)(row, k) * other(k, column);
                result(row, column) = sum;
            }
        return result;
    }

    constexpr Vec4 operator*(Vec4 v) const
    {
        const Mat4& a = *this;
        return {a(0, 0) * v.x + a(0, 1) * v.y + a(0, 2) * v.z + a(0, 3) * v.w,
                a(1, 0) * v.x + a(1, 1) * v.y + a(1, 2) * v.z + a(1, 3) * v.w,
                a(2, 0) * v.x + a(2, 1) * v.y + a(2, 2) * v.z + a(2, 3) * v.w,
                a(3, 0) * v.x + a(3, 1) * v.y + a(3, 2) * v.z + a(3, 3) * v.w};
    }

    constexpr Vec3 TransformPoint(Vec3 p) const
    {
        const Vec4 v = *this * Vec4{p.x, p.y, p.z, 1};
        return Vec3{v.x, v.y, v.z} / v.w;
    }

    Mat4 Inverse() const
    {
        // Cofactors, after the expansion in Mesa's gluInvertMatrix.
        const auto& a = m;
        std::array<float, 16> inv{};
        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

        const float determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        Mat4 result;
        if (determinant == 0)
            return Identity();
        for (int i = 0; i < 16; ++i)
            result.m[i] = inv[i] / determinant;
        return result;
    }
};

} // namespace math
