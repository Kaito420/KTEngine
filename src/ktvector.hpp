#ifndef _KTVECTOR_HPP_
#define _KTVECTOR_HPP_

#include <math.h>
#include <stdexcept>

struct KTVECTOR2{
public:
    float x = 0.0f;
    float y = 0.0f;
    KTVECTOR2() = default;
    KTVECTOR2(const KTVECTOR2& k):x(k.x),y(k.y){}
    KTVECTOR2(float x, float y):x(x), y(y){}

    KTVECTOR2& operator=(const KTVECTOR2& k){
        x = k.x; y = k.y;
        return *this;
    }

    bool operator==(const KTVECTOR2& k)const {
        return x == k.x && y == k.y;
    }
    bool operator!=(const KTVECTOR2& k)const {
        return x != k.x || y != k.y;
    }

    KTVECTOR2 operator-() const{
        return KTVECTOR2(-x, -y);
    }

    KTVECTOR2 operator+(const KTVECTOR2& k)const{
        return { x + k.x, y + k.y };
    }

    KTVECTOR2 operator-(const KTVECTOR2& k)const{
        return { x - k.x, y - k.y };
    }

    KTVECTOR2 operator*(float f)const{
        return { x * f, y * f };
    }

    KTVECTOR2 operator/(float f)const{
        if( f == 0.0f )throw std::runtime_error("Division by zero in KTVECTOR2");
        return { x / f, y / f };
    }

    friend KTVECTOR2 operator*(float f, const KTVECTOR2& k){
        return { k.x * f, k.y * f };
    }

    KTVECTOR2& operator+=(const KTVECTOR2& k){
        x += k.x; y += k.y;
        return *this;
    }

    KTVECTOR2& operator-=(const KTVECTOR2& k){
        x -= k.x; y -= k.y;
        return *this;
    }

    KTVECTOR2& operator*=(float f){
        x *= f;
        y *= f;
        return *this;
    }

    float Absolute()const{ //ベクトルの大きさ
        return sqrtf( x * x + y * y);
    }

    KTVECTOR2 Normalize()const{ //ベクトルの正規化
        float len = Absolute();
        return len == 0.0f ? KTVECTOR2(0.0f, 0.0f) : *this / len;
    }

    friend float Dot(const KTVECTOR2& k1, const KTVECTOR2& k2){ //ベクトルの内積
        return k1.x * k2.x + k1.y * k2.y;
    }
};

struct KTVECTOR3{
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    KTVECTOR3() = default;
    KTVECTOR3(const KTVECTOR3& k):x(k.x),y(k.y),z(k.z){}
    KTVECTOR3(float x, float y, float z):x(x),y(y),z(z){}

    KTVECTOR3& operator=(const KTVECTOR3& k){
        x = k.x; y = k.y; z = k.z;
        return *this;
    }

    bool operator==(const KTVECTOR3& k)const {
        return x == k.x && y == k.y && z == k.z;
    }
    bool operator!=(const KTVECTOR3& k)const {
        return x != k.x || y != k.y || z != k.z;
    }

    KTVECTOR3 operator-() const{
        return KTVECTOR3(-x, -y, -z);
    }

    KTVECTOR3 operator+(const KTVECTOR3& k)const{
        return { x + k.x, y + k.y, z + k.z };
    }

    KTVECTOR3 operator-(const KTVECTOR3& k)const{
        return { x - k.x, y - k.y, z - k.z };
    }

    KTVECTOR3 operator*(float f)const{
        return { x * f, y * f, z * f };
    }

    KTVECTOR3 operator/(float f)const{
        if( f == 0.0f )throw std::runtime_error("Division by zero in KTVECTOR3");
        return { x / f, y / f, z / f };
    }

    friend KTVECTOR3 operator*(float f, const KTVECTOR3& k){
        return { k.x * f, k.y * f, k.z * f };
    }

    KTVECTOR3& operator+=(const KTVECTOR3& k){
        x += k.x; y += k.y; z += k.z;
        return *this;
    }

    KTVECTOR3& operator-=(const KTVECTOR3& k){
        x -= k.x; y -= k.y; z -= k.z;
        return *this;
    }

    KTVECTOR3& operator*=(float f){
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }


    float Absolute()const{  //ベクトルの大きさを求める
        return sqrtf(x * x + y * y + z * z);  
    }

    KTVECTOR3 Normalize()const{ //ベクトルの正規化
        float len = Absolute();
        return len == 0.0f ? KTVECTOR3( 0.0f, 0.0f, 0.0f) : *this / len;
    }

    friend float Dot(const KTVECTOR3& k1, const KTVECTOR3& k2){ //ベクトルの内積
        return {k1.x * k2.x + k1.y * k2.y + k1.z * k2.z };
    }

    friend KTVECTOR3 Cross(const KTVECTOR3& k1, const KTVECTOR3& k2){ //ベクトルの外積
        return {k1.y * k2.z - k1.z * k2.y,
                k1.z * k2.x - k1.x * k2.z,
                k1.x * k2.y - k1.y * k2.x
                };
    }

    friend KTVECTOR3 CrossNormalize(const KTVECTOR3& k1, const KTVECTOR3& k2){
        return Cross(k1, k2).Normalize();
    }
};

struct KTVECTOR4 {
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    KTVECTOR4() = default;
    KTVECTOR4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    KTVECTOR4(const KTVECTOR4& k) : x(k.x), y(k.y), z(k.z), w(k.w) {}

    KTVECTOR4& operator=(const KTVECTOR4& k) {
        if (this == &k) return *this;
        x = k.x; y = k.y; z = k.z; w = k.w;
        return *this;
    }

    bool operator==(const KTVECTOR4& k) const {
        return x == k.x && y == k.y && z == k.z && w == k.w;
    }

    bool operator!=(const KTVECTOR4& k) const {
        return !(*this == k);
    }

    KTVECTOR4 operator-() const {
        return { -x, -y, -z, -w };
    }

    KTVECTOR4 operator+(const KTVECTOR4& k) const {
        return { x + k.x, y + k.y, z + k.z, w + k.w };
    }

    KTVECTOR4 operator-(const KTVECTOR4& k) const {
        return { x - k.x, y - k.y, z - k.z, w - k.w };
    }

    KTVECTOR4 operator*(float f) const {
        return { x * f, y * f, z * f, w * f };
    }

    KTVECTOR4 operator/(float f) const {
        if (f == 0.0f) throw std::runtime_error("Division by zero in KTVECTOR4");
        return { x / f, y / f, z / f, w / f };
    }

    friend KTVECTOR4 operator*(float f, const KTVECTOR4& k) {
        return { k.x * f, k.y * f, k.z * f, k.w * f };
    }

    KTVECTOR4& operator+=(const KTVECTOR4& k) {
        x += k.x; y += k.y; z += k.z; w += k.w;
        return *this;
    }

    KTVECTOR4& operator-=(const KTVECTOR4& k) {
        x -= k.x; y -= k.y; z -= k.z; w -= k.w;
        return *this;
    }

    KTVECTOR4& operator*=(float f) {
        x *= f; y *= f; z *= f; w *= f;
        return *this;
    }

    float Absolute() const {
        return sqrtf(x * x + y * y + z * z + w * w);
    }

    KTVECTOR4 Normalize() const {
        float len = Absolute();
        return len == 0.0f ? KTVECTOR4(0, 0, 0, 0) : *this / len;
    }

    friend float Dot(const KTVECTOR4& a, const KTVECTOR4& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
};

struct KTMATRIX3 {
    float m[3][3];  //[row][col]行優先

    KTMATRIX3() { *this = Identity(); }

    KTMATRIX3(float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
	}

    static KTMATRIX3 Identity() {
        return KTMATRIX3(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        );
	}

    KTMATRIX3 Transpose(const KTMATRIX3& mat) {
        return KTMATRIX3(
            mat.m[0][0], mat.m[1][0], mat.m[2][0],
            mat.m[0][1], mat.m[1][1], mat.m[2][1],
            mat.m[0][2], mat.m[1][2], mat.m[2][2]
        );
    }

    KTMATRIX3 operator*(const KTMATRIX3& mat) const {
        KTMATRIX3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = m[i][0] * mat.m[0][j] +
                                 m[i][1] * mat.m[1][j] +
                                 m[i][2] * mat.m[2][j];
            }
        }
        return result;
    }

    KTVECTOR3 operator*(const KTVECTOR3& v) const {
        return KTVECTOR3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
    KTMATRIX3 & operator*=(const KTMATRIX3& mat) {
        *this = *this * mat;
        return *this;
	}

};

struct KTMATRIX4 {
    float m[4][4];  //[row][col]行優先

    KTMATRIX4() = default;    //Identity初期化

    KTMATRIX4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
		m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;

    }

    static KTMATRIX4 Identity() {
        return KTMATRIX4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        );
    }

    static KTMATRIX4 Translation(const KTVECTOR3& k) {
        return KTMATRIX4(
            1, 0, 0, k.x,
            0, 1, 0, k.y,
            0, 0, 1, k.z,
            0, 0, 0, 1
        );
	}

    static KTMATRIX4 Scale(const KTVECTOR3& k) {
        return KTMATRIX4(
            k.x, 0, 0, 0,
            0, k.y, 0, 0,
            0, 0, k.z, 0,
            0, 0, 0, 1
        );
	}

    /// <summary>
    /// X軸周りの回転行列を返す
    /// </summary>
    /// <param name="angle">度数法</param>
    /// <returns></returns>
    static KTMATRIX4 RotationX(float angle) {
		float rad = angle * 3.14159265358979323846f / 180.0f; // 弧度法に変換
        float c = cosf(rad);
        float s = sinf(rad);
        return KTMATRIX4(
            1, 0, 0, 0,
            0, c, -s, 0,
            0, s, c, 0,
            0, 0, 0, 1
        );
    }
    /// <summary>
    /// Y軸周りの回転行列を返す
    /// </summary>
    /// <param name="angle">度数法</param>
    /// <returns></returns>
    static KTMATRIX4 RotationY(float angle) {
		float rad = angle * 3.14159265358979323846f / 180.0f; // 弧度法に変換
        float c = cosf(rad);
        float s = sinf(rad);
        return KTMATRIX4(
            c, 0, s, 0,
            0, 1, 0, 0,
            -s, 0, c, 0,
            0, 0, 0, 1
        );
    }

    /// <summary>
    /// Z軸周りの回転行列を返す
    /// </summary>
    /// <param name="angle">度数法</param>
    /// <returns></returns>
    static KTMATRIX4 RotationZ(float angle) {
		float rad = angle * 3.14159265358979323846f / 180.0f; // 弧度法に変換
        float c = cosf(rad);
        float s = sinf(rad);
        return KTMATRIX4(
            c, -s, 0, 0,
            s, c, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        );
    }

    /// <summary>
    /// 任意の軸を中心とした回転行列を返す
    /// </summary>
    /// <param name="axis">任意の軸</param>
    /// <param name="angle">度数法</param>
    /// <returns></returns>
    static KTMATRIX4 RotationAxis(const KTVECTOR3& axis, float angle) {
        KTVECTOR3 n = axis.Normalize();
		float rad = angle * 3.14159265358979323846f / 180.0f; // 弧度法に変換
        float c = cosf(rad);
        float s = sinf(rad);
        float t = 1 - c;
        return KTMATRIX4(
            t * n.x * n.x + c,       t * n.x * n.y - s * n.z, t * n.x * n.z + s * n.y, 0,
            t * n.x * n.y + s * n.z, t * n.y * n.y + c,       t * n.y * n.z - s * n.x, 0,
            t * n.x * n.z - s * n.y, t * n.y * n.z + s * n.x, t * n.z * n.z + c,       0,
            0,                        0,                        0,                        1
        );
	}   

    KTMATRIX4 operator*(const KTMATRIX4& mat) const {
        KTMATRIX4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = m[i][0] * mat.m[0][j] +
                                 m[i][1] * mat.m[1][j] +
                                 m[i][2] * mat.m[2][j] +
                                 m[i][3] * mat.m[3][j];
            }
        }
        return result;
    }
    KTVECTOR4 operator*(const KTVECTOR4& v) const {
        return KTVECTOR4(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w
        );
    }

    KTMATRIX4 & operator*=(const KTMATRIX4& mat) {
        *this = *this * mat;
        return *this;
	}


    /// <summary>
	/// 引数に与えられた行列の転置行列を返す
    /// </summary>
    /// <param name="mat">転置したい行列</param>
    /// <returns></returns>
    KTMATRIX4 Transpose(const KTMATRIX4& mat) {
        return KTMATRIX4(
            mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
            mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
            mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
            mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
        );
	}

    KTMATRIX4 Inverse() {
        KTMATRIX4 a = *this;
		KTMATRIX4 inv = Identity();
        for (int i = 0; i < 4; i++) {
            //pivot
			float pivot = a.m[i][i];
			if (fabs(pivot) < 1e-6f) throw std::runtime_error("Matrix is singular and cannot be inverted.");
            
			float invPivot = 1.0f / pivot;
            for(int j = 0; j < 4; j++) {
                a.m[i][j] *= invPivot;
                inv.m[i][j] *= invPivot;
            }
            for (int k = 0; k < 4; k++) {
                if (k != i) {
                    float factor = a.m[k][i];
                    for (int j = 0; j < 4; j++) {
                        a.m[k][j] -= factor * a.m[i][j];
                        inv.m[k][j] -= factor * inv.m[i][j];
                    }
                }
			}
        }
    }

    KTMATRIX3 ToMatrix3()const {
        return KTMATRIX3(
            m[0][0], m[0][1], m[0][2],
            m[1][0], m[1][1], m[1][2],
            m[2][0], m[2][1], m[2][2]
        );
	}

};

struct KTQUATERNION {
    float x, y, z, w;

	KTQUATERNION() : x(0), y(0), z(0), w(1) {}
	KTQUATERNION(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    //基本操作
    KTQUATERNION Normalize()const {
        float len = sqrtf(x * x + y * y + z * z + w * w);
        if (len == 0.0f) return KTQUATERNION(0, 0, 0, 1);
		return KTQUATERNION(x / len, y / len, z / len, w / len);
    }

    KTQUATERNION Conjugate()const {
        return KTQUATERNION(-x, -y, -z, w);
	}

    KTQUATERNION Inverse()const {
        float lenSq = x * x + y * y + z * z + w * w;
        if (lenSq == 0.0f) throw std::runtime_error("Cannot invert a zero-length quaternion.");
		return KTQUATERNION(-x / lenSq, -y / lenSq, -z / lenSq, w / lenSq);
    }

    //演算子

    KTQUATERNION operator+(const KTQUATERNION& q)const {
        return KTQUATERNION(x + q.x, y + q.y, z + q.z, w + q.w);
    }

    /// <summary>
    /// 回転の合成
    /// </summary>
    /// <param name="q"></param>
    /// <returns></returns>
    KTQUATERNION operator*(const KTQUATERNION& q)const {
        return KTQUATERNION(
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        );
	}

    /// <summary>
    /// ベクトルの回転
    /// </summary>
    /// <param name="v"></param>
    /// <returns></returns>
    KTVECTOR3 operator*(const KTVECTOR3& v)const {
        KTQUATERNION vq(v.x, v.y, v.z, 0);
        KTQUATERNION rq = (*this) * vq * this->Inverse();
        return KTVECTOR3(rq.x, rq.y, rq.z);
	}

    /// <summary>
	/// 任意の軸を中心とした回転を表すクォータニオンを返す
    /// </summary>
    /// <param name="axis"></param>
    /// <param name="angle">度数法</param>
    /// <returns></returns>
    static KTQUATERNION FromAxisAngle(const KTVECTOR3& axis, float angle) {
        float rad = angle * (3.14159265359f / 180.0f);  // 度をラジアンに変換
        float halfAngle = rad / 2.0f;
        float s = sinf(halfAngle);
        return KTQUATERNION(axis.x * s, axis.y * s, axis.z * s, cosf(halfAngle));
    }

    /// <summary>
	/// オイラー角からクォータニオンを生成
    /// </summary>
    /// <param name="pitch">x軸回りの回転角（度数法）</param>
    /// <param name="yaw">y軸回りの回転角（度数法）</param>
    /// <param name="roll">z軸回りの回転角（度数法）</param>
    /// <returns></returns>
    static KTQUATERNION FromEulerAngles(float pitch, float yaw, float roll) {
        float cy = cosf(yaw * 0.5f * (3.14159265359f / 180.0f));
        float sy = sinf(yaw * 0.5f * (3.14159265359f / 180.0f));
        float cp = cosf(pitch * 0.5f * (3.14159265359f / 180.0f));
        float sp = sinf(pitch * 0.5f * (3.14159265359f / 180.0f));
        float cr = cosf(roll * 0.5f * (3.14159265359f / 180.0f));
        float sr = sinf(roll * 0.5f * (3.14159265359f / 180.0f));
        return KTQUATERNION(
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy
        );
	}

    KTMATRIX4 ToMatrix()const {
		KTQUATERNION q = Normalize();
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;
        return KTMATRIX4(
            1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz),       2.0f * (xz + wy),       0.0f,
            2.0f * (xy + wz),       1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),       0.0f,
            2.0f * (xz - wy),       2.0f * (yz + wx),       1.0f - 2.0f * (xx + yy), 0.0f,
            0.0f,                   0.0f,                   0.0f,                   1.0f
		);
    }

    static KTQUATERNION Slerp(const KTQUATERNION& q1, const KTQUATERNION& q2, float t) {
        // クォータニオンの内積を計算
        float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
        // 内積が負の場合、q2を反転させる（最短経路を取るため）
        KTQUATERNION q2Copy = q2;
        if (dot < 0.0f) {
            dot = -dot;
            q2Copy = KTQUATERNION(-q2.x, -q2.y, -q2.z, -q2.w);
        }
        const float DOT_THRESHOLD = 0.9995f;
        if (dot > DOT_THRESHOLD) {
            // クォータニオンが非常に近い場合、線形補間を使用
            KTQUATERNION result = KTQUATERNION(
                q1.x + t * (q2Copy.x - q1.x),
                q1.y + t * (q2Copy.y - q1.y),
                q1.z + t * (q2Copy.z - q1.z),
                q1.w + t * (q2Copy.w - q1.w)
            );
            return result.Normalize();
        }
        // θを計算
        float theta_0 = acosf(dot);        // θ_0 = acos(dot)
        float theta = theta_0 * t;         // θ = θ_0 * t
        float sin_theta = sinf(theta);      // sin(θ)
        float sin_theta_0 = sinf(theta_0);  // sin(θ_0)
        float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;  // s0 = cos(θ) - dot * sin(θ) / sin(θ_0)
        float s1 = sin_theta / sin_theta_0;                        // s1 = sin(θ) / sin(θ_0)
        return KTQUATERNION(
            (s0 * q1.x) + (s1 * q2Copy.x),
            (s0 * q1.y) + (s1 * q2Copy.y),
            (s0 * q1.z) + (s1 * q2Copy.z),
			(s0 * q1.w) + (s1 * q2Copy.w)
		);
    }
};


#endif // !_KTVECTOR_HPP_