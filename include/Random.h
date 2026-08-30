#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Generador pseudoaleatorio determinista y libre de estado compartido.
//
// Motivacion paralela: usar "std::rand" o un unico "std::mt19937" global desde
// varios hilos obligaria a serializar con un mutex y, ademas, haria que el
// resultado dependiera del orden en que los hilos consumen numeros. Aqui cada
// pelota lleva su propia semilla de 32 bits, de modo que la secuencia que
// consume es independiente del numero de hilos y de su calendarizacion.
// El algoritmo es un contador con mezcla estilo SplitMix32.
// ---------------------------------------------------------------------------

// Avanza la semilla y devuelve un entero pseudoaleatorio de 32 bits.
// Entrada/salida: "state" se modifica en el lugar (referencia).
inline std::uint32_t nextRandomUint(std::uint32_t& state) {
    state += 0x9E3779B9u;  // Proporcion aurea en punto fijo de 32 bits.
    std::uint32_t mixed = state;
    mixed = (mixed ^ (mixed >> 16)) * 0x21F0AAADu;
    mixed = (mixed ^ (mixed >> 15)) * 0x735A2D97u;
    return mixed ^ (mixed >> 15);
}

// Devuelve un flotante uniformemente distribuido en [0, 1).
inline float nextRandomFloat(std::uint32_t& state) {
    // 24 bits de mantisa: suficiente para "float" y evita valores repetidos.
    return static_cast<float>(nextRandomUint(state) >> 8) / 16777216.0F;
}

// Devuelve un flotante uniformemente distribuido en [minimum, maximum).
inline float nextRandomInRange(std::uint32_t& state, float minimum, float maximum) {
    return minimum + (maximum - minimum) * nextRandomFloat(state);
}

// Mezcla dos enteros en una semilla nueva. Se usa para derivar la semilla de
// cada pelota a partir de la semilla global y del indice de la pelota, sin
// necesidad de recorrer secuencialmente el generador.
inline std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t index) {
    std::uint32_t mixed = seed ^ (index * 0x9E3779B9u + 0x85EBCA6Bu);
    mixed = (mixed ^ (mixed >> 16)) * 0x7FEB352Du;
    mixed = (mixed ^ (mixed >> 15)) * 0x846CA68Bu;
    return mixed ^ (mixed >> 16);
}
