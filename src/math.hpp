// Small vector/quaternion helpers for the skate library (no dependencies).
#pragma once
#include <cmath>

namespace skate {

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;

struct Vec3
{
	float x = 0, y = 0, z = 0;
	constexpr Vec3() = default;
	constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
	Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
	Vec3 operator-() const { return {-x, -y, -z}; }
	Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
	Vec3 &operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
	Vec3 &operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
	Vec3 &operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
	float dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
	Vec3 cross(Vec3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
	float length() const { return std::sqrt(dot(*this)); }
	Vec3 normalize_or_zero() const { float l = length(); return l > 1e-6f ? *this * (1.0f / l) : Vec3{}; }
	Vec3 lerp(Vec3 o, float t) const { return *this + (o - *this) * t; }
	float distance(Vec3 o) const { return (*this - o).length(); }
};

constexpr Vec3 UP{0, 1, 0};

struct Quat
{
	float x = 0, y = 0, z = 0, w = 1;
	static Quat axis_angle(Vec3 axis, float angle)
	{
		float s = std::sin(angle * 0.5f);
		return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
	}
	static Quat rotation_x(float a) { return axis_angle({1, 0, 0}, a); }
	static Quat rotation_y(float a) { return axis_angle({0, 1, 0}, a); }
	static Quat rotation_z(float a) { return axis_angle({0, 0, 1}, a); }
	// Shortest arc taking unit vector a to unit vector b.
	static Quat arc(Vec3 a, Vec3 b)
	{
		float d = a.dot(b);
		if (d < -0.9999f)
			return axis_angle({1, 0, 0}, PI);
		Vec3 c = a.cross(b);
		Quat q{c.x, c.y, c.z, 1.0f + d};
		return q.normalized();
	}
	Quat operator*(Quat o) const
	{
		return {w * o.x + x * o.w + y * o.z - z * o.y,
			w * o.y - x * o.z + y * o.w + z * o.x,
			w * o.z + x * o.y - y * o.x + z * o.w,
			w * o.w - x * o.x - y * o.y - z * o.z};
	}
	Quat normalized() const
	{
		float l = std::sqrt(x * x + y * y + z * z + w * w);
		return {x / l, y / l, z / l, w / l};
	}
	Vec3 rotate(Vec3 v) const
	{
		Vec3 u{x, y, z};
		Vec3 t = u.cross(v) * 2.0f;
		return v + t * w + u.cross(t);
	}
};

inline float wrap_pi(float a)
{
	a = std::fmod(a + PI, TAU);
	if (a < 0)
		a += TAU;
	return a - PI;
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Board forward for a yaw: nose at +Z when yaw is 0, yaw turns toward +X.
inline Vec3 forward(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }

} // namespace skate
