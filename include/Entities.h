#pragma once

#include "Vec3.h"

#include <cstdint>

// ---------------------------------------------------------------------------
// Entidades de la simulacion.
//
// La escena se modela como tres arreglos planos (Struct of Arrays "logico":
// vectores de estructuras contiguas) porque el patron de descomposicion
// elegido es de dominio: el trabajo se reparte por indice de pelota.
// ---------------------------------------------------------------------------

// Pelota del tablero Plinko. Es la unidad de trabajo que se reparte entre hilos.
struct Ball {
    Vec3 position;        // Posicion del centro en coordenadas de mundo.
    Vec3 velocity;        // Velocidad lineal en unidades de mundo por segundo.
    Vec3 color;           // Color RGB pseudoaleatorio en [0, 1].
    float radius;         // Radio de la esfera; interviene en toda colision.
    float mass;           // Masa; pondera el intercambio de impulso pelota-pelota.
    std::uint32_t seed;   // Estado del generador pseudoaleatorio propio.
    std::int32_t bin;     // Casilla alcanzada en el ultimo reciclaje (-1 si ninguna).
    bool active;          // Falso desactiva la pelota sin borrarla del arreglo.
};

// Clavija del tablero. Puede oscilar de forma sinusoidal para que el recorrido
// de las pelotas no sea repetitivo (elemento de trigonometria del proyecto).
struct Peg {
    Vec3 basePosition;    // Centro de reposo de la clavija.
    Vec3 color;           // Color RGB de dibujo.
    float radius;         // Radio del cilindro/esfera de colision.
    float amplitude;      // Amplitud de la oscilacion horizontal.
    float angularSpeed;   // Velocidad angular de la oscilacion (rad/s).
    float phase;          // Desfase inicial, distinto por clavija.
};

// Zona que altera la fisica local de las pelotas que la atraviesan.
enum class ModifierKind : std::uint8_t {
    GravityWell = 0,  // Atrae la pelota hacia el centro de la zona.
    SpeedBoost  = 1,  // Empuja la pelota hacia abajo, acelerandola.
    Turbulence  = 2   // Agrega una perturbacion lateral dependiente del tiempo.
};

struct Modifier {
    Vec3 center;         // Centro de la zona de influencia.
    Vec3 color;          // Color RGB de dibujo.
    float radius;        // Radio de influencia.
    float strength;      // Intensidad del efecto.
    ModifierKind kind;   // Tipo de efecto aplicado.
};

// Devuelve la posicion de la clavija en el instante "time".
// Entradas: "peg" clavija consultada; "time" tiempo de simulacion en segundos.
// Salida: Vec3 con el centro desplazado por la oscilacion sinusoidal.
// Es una funcion pura: puede llamarse desde cualquier hilo sin sincronizacion.
inline Vec3 pegPositionAt(const Peg& peg, float time) {
    const float offset = peg.amplitude * std::sin(peg.angularSpeed * time + peg.phase);
    return {peg.basePosition.x + offset, peg.basePosition.y, peg.basePosition.z};
}
