
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

inline Vector3 homogenize(const Vector4& v) {
	return (Vector3){v.x/v.w, v.y/v.w, v.z/v.w};
}

inline Vector3 midpoint(const Vector3& v1, const Vector3& v2) {
	return (v1 + v2) * 0.5f;
}

////////////////////////////////
//     Vector4 operations     //
////////////////////////////////
inline Vector4 ToVector4(const Vector3& v) {
	return (Vector4){v.x, v.y, v.z, 1.0f};
}

inline Vector4 Vector4Zero() {
	return (Vector4){0.0f, 0.0f, 0.0f, 0.0f};
}

inline Vector4 Vector4Ones() {
	return (Vector4){1.0f, 1.0f, 1.0f, 1.0f};
}

////////////////////////////////
//      Matrix operations     //
////////////////////////////////

inline Matrix operator+(const Matrix& m1, const Matrix& m2) {
	return MatrixAdd(m1, m2);
}

inline Matrix operator-(const Matrix& m1, const Matrix& m2) {
	return MatrixSubtract(m1, m2);
}

inline Matrix operator*(const Matrix& m1, const Matrix& m2) {
	return MatrixMultiply(m1, m2);
}

inline Vector4 operator*(const Matrix& m, const Vector4& v) {
	// Caution!!! This code might have rows and columns switched!! test further. Jan. 23rd.
	Vector4 vOut = Vector4Zero();

	float a00 = m.m0,  a01 = m.m1,  a02 = m.m2,  a03 = m.m3;
    float a10 = m.m4,  a11 = m.m5,  a12 = m.m6,  a13 = m.m7;
    float a20 = m.m8,  a21 = m.m9,  a22 = m.m10, a23 = m.m11;
    float a30 = m.m12, a31 = m.m13, a32 = m.m14, a33 = m.m15;

    vOut.x = a00 * v.x + a10 * v.y + a20 * v.z + a30 * v.w;
    vOut.y = a01 * v.x + a11 * v.y + a21 * v.z + a31 * v.w;
    vOut.z = a02 * v.x + a12 * v.y + a22 * v.z + a32 * v.w;
    vOut.w = a03 * v.x + a13 * v.y + a23 * v.z + a33 * v.w;

	return vOut;
}

inline Matrix MatrixTranslate(const Vector3& v) {
	return MatrixTranslate(v.x, v.y, v.z);
}

inline Matrix MatrixScale(const Vector3& v) {
	return MatrixScale(v.x, v.y, v.z);
}

inline Matrix MatrixScale(const float f) {
	return MatrixScale(f, f, f);
}

inline Matrix MatrixAlignToAxis(Vector3 srcAxis, Vector3 targetAxis) {
	/*
	Takes srcAxis (probably a eclidean basis vector) and rotates it to align with targetAxis.
	*/
	float rotAngle = acosf(dot(srcAxis, targetAxis) / (norm(srcAxis) * norm(targetAxis))); // radians
	return MatrixRotate(cross(normalize(srcAxis), normalize(targetAxis)), rotAngle);
}

////////////////////////////////
//       Other operations     //
////////////////////////////////

float norm(float x, float y, float z) {
	return sqrt(x*x + y*y + z*z);
}

// See: https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/3554913

Vector3 PositionVectorFromTransform(const Matrix& m) {
	return (Vector3){m.m12, m.m13, m.m14};
}

Vector3 ScaleVectorFromTransform(const Matrix& m) {
	return (Vector3){norm(m.m0, m.m1, m.m2), norm(m.m4, m.m5, m.m6), norm(m.m8, m.m9, m.m10)};
}

Matrix RotationTransformFromTransform(const Matrix& m) {
	Vector3 scale = ScaleVectorFromTransform(m);
	Matrix mOut = m; 

	// First column
	mOut.m0 /= scale.x;
	mOut.m1 /= scale.x;
	mOut.m2 /= scale.x;

	// Second column
	mOut.m4 /= scale.y;
	mOut.m5 /= scale.y;
	mOut.m6 /= scale.y;

	// Third column
	mOut.m8  /= scale.z;
	mOut.m9  /= scale.z;
	mOut.m10 /= scale.z;

	// remove translation piece
	mOut.m12 = 0.0f;
	mOut.m13 = 0.0f;
	mOut.m14 = 0.0f;
	mOut.m15 = 1.0f;
	return mOut;
}