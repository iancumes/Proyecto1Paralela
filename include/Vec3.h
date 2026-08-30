#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// Vec3: vector de tres componentes en punto flotante de precision simple.
//
// Se define como un agregado POD (sin constructores ni herencia) para que el
// arreglo "std::vector<Ball>" quede contiguo en memoria y los ciclos que lo
// recorren puedan vectorizarse y repartirse entre hilos sin indirecciones.
// Todas las operaciones son "const" y libres de estado global, por lo que
// pueden invocarse simultaneamente desde varios hilos sin sincronizacion.
// ---------------------------------------------------------------------------
struct Vec3 {
    float x;  // Componente horizontal del tablero (ancho).
    float y;  // Componente vertical del tablero (altura, la gravedad actua aqui).
    float z;  // Componente de profundidad del tablero.

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
};

inline Vec3 operator+(const Vec3& left, const Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vec3 operator-(const Vec3& left, const Vec3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline Vec3 operator*(const Vec3& vector, float scalar) {
    return {vector.x * scalar, vector.y * scalar, vector.z * scalar};
}

inline Vec3 operator*(float scalar, const Vec3& vector) {
    return vector * scalar;
}

// Producto punto: base de las proyecciones usadas al resolver los rebotes.
inline float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

// Magnitud al cuadrado. Se prefiere sobre "length" cuando solo se comparan
// distancias, porque evita la raiz cuadrada dentro de los ciclos O(N^2).
inline float lengthSquared(const Vec3& vector) {
    return dot(vector, vector);
}

inline float length(const Vec3& vector) {
    return std::sqrt(lengthSquared(vector));
}

// Devuelve el vector unitario. Si la magnitud es despreciable regresa un eje
// arbitrario estable para evitar divisiones entre cero (programacion defensiva).
inline Vec3 normalized(const Vec3& vector) {
    const float magnitude = length(vector);
    if (magnitude < 1.0e-6F) {
        return {0.0F, 1.0F, 0.0F};
    }
    const float inverse = 1.0F / magnitude;
    return {vector.x * inverse, vector.y * inverse, vector.z * inverse};
}
