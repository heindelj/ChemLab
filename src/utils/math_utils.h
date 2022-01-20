////////////////////////////////
// Vector2 operator overloads //
////////////////////////////////
inline Vector2 operator+(const Vector2& v1, const Vector2& v2) {
	return Vector2Add(v1, v2);
}

inline Vector2 operator-(const Vector2& v1, const Vector2& v2) {
	return Vector2Subtract(v1, v2);
}

inline Vector2 operator*(const Vector2& v1, const Vector2& v2) {
	return Vector2Multiply(v1, v2);
}

inline Vector2 operator*(const Vector2& v1, const float a) {
	return Vector2Scale(v1, a);
}

inline Vector2 operator*(const float a, const Vector2& v1) {
	return Vector2Scale(v1, a);
}

inline Vector2 operator/(const Vector2& v1, const float a) {
	return Vector2Scale(v1, 1.0f/a);
}

inline float norm(const Vector2& v) {
	return Vector2Length(v);
}

inline Vector2 normalize(const Vector2& v) {
	return Vector2Normalize(v);
}

inline float dot(const Vector2& v1, const Vector2& v2) {
	return Vector2DotProduct(v1, v2);
}


////////////////////////////////
// Vector3 operator overloads //
////////////////////////////////
inline Vector3 operator+(const Vector3& v1, const Vector3& v2) {
	return Vector3Add(v1, v2);
}

inline Vector3 operator-(const Vector3& v1, const Vector3& v2) {
	return Vector3Subtract(v1, v2);
}

inline Vector3 operator*(const Vector3& v1, const Vector3& v2) {
	return Vector3Multiply(v1, v2);
}

template<typename T>
inline Vector3 operator*(const Vector3& v1, const T a) {
	return Vector3Scale(v1, (float)a);
}

template<typename T>
inline Vector3 operator*(const T a, const Vector3& v1) {
	return Vector3Scale(v1, (float)a);
}

template<typename T>
inline Vector3 operator/(const Vector3& v1, const T a) {
	return Vector3Scale(v1, 1.0f/(float)a);
}

inline float norm(const Vector3& v) {
	return Vector3Length(v);
}

inline Vector3 normalize(const Vector3& v) {
	return Vector3Normalize(v);
}

inline float dot(const Vector3& v1, const Vector3& v2) {
	return Vector3DotProduct(v1, v2);
}

inline Vector3 cross(const Vector3& v1, const Vector3& v2) {
	return Vector3CrossProduct(v1, v2);
}