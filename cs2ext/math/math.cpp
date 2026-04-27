#include "math.h"
#include <cmath>
#include <Windows.h>

int screenWidth = GetSystemMetrics(SM_CXSCREEN);
int screenHeight = GetSystemMetrics(SM_CYSCREEN);

Vector3::Vector3(float x, float y, float z) noexcept :
    x(x), y(y), z(z) {
}

Vector3 Vector3::operator-(const Vector3& other) const noexcept {
    return Vector3(x - other.x, y - other.y, z - other.z);
}

Vector3 Vector3::operator+(const Vector3& other) const noexcept {
    return Vector3(x + other.x, y + other.y, z + other.z);
}

Vector3 Vector3::operator*(float factor) const noexcept {
    return Vector3(x * factor, y * factor, z * factor);
}

Vector3 Vector3::operator/(float factor) const noexcept {
    return Vector3(x / factor, y / factor, z / factor);
}

Vector3 Vector3::w2s(view_matrix_t matrix) const {

    float clipX = matrix.matrix[0][0] * x + matrix.matrix[0][1] * y + matrix.matrix[0][2] * z + matrix.matrix[0][3];
    float clipY = matrix.matrix[1][0] * x + matrix.matrix[1][1] * y + matrix.matrix[1][2] * z + matrix.matrix[1][3];
    float clipZ = matrix.matrix[2][0] * x + matrix.matrix[2][1] * y + matrix.matrix[2][2] * z + matrix.matrix[2][3];
    float clipW = matrix.matrix[3][0] * x + matrix.matrix[3][1] * y + matrix.matrix[3][2] * z + matrix.matrix[3][3];

    if (clipW < 0.001f)  // a lit bit tolerance
        return Vector3(0, 0, 0);

    // Perspective division
    float invW = 1.0f / clipW;
    float ndcX = clipX * invW;
    float ndcY = clipY * invW;
    float ndcZ = clipZ * invW;

    // Screen coords
    float screenX = (screenWidth * 0.5f) * (1.0f + ndcX);
    float screenY = (screenHeight * 0.5f) * (1.0f - ndcY);  // flip Y

    return Vector3(screenX, screenY, clipW);
}