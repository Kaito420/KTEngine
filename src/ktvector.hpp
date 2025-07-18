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