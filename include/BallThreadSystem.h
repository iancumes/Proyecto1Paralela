#pragma once

#include "Entities.h"
#include "SimulationConfig.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// BallThreadSystem: primera version paralela del proyecto.
//
// Crea un std::thread persistente por pelota y los sincroniza con una barrera
// de dos fases construida sobre un mutex y dos variables de condicion. Se
// conserva en el codigo final porque es el punto de comparacion que motiva el
// cambio a OpenMP: cuando N crece, el costo de despertar y volver a dormir N
// hilos supera con creces el trabajo util de cada uno.
//
// Los hilos se crean una sola vez en el constructor y se destruyen en el
// destructor; nunca se crean por cuadro.
// ---------------------------------------------------------------------------
class BallThreadSystem {
public:
    // Crea "ballCount" hilos persistentes.
    // Lanza std::system_error si el sistema operativo no puede crearlos.
    explicit BallThreadSystem(std::size_t ballCount);
    ~BallThreadSystem();

    BallThreadSystem(const BallThreadSystem&) = delete;
    BallThreadSystem& operator=(const BallThreadSystem&) = delete;

    // Ejecuta una ronda de actualizacion: cada worker calcula exactamente una
    // pelota y el hilo llamador espera a que todos terminen.
    // Entradas: arreglos de solo lectura del cuadro anterior y parametros.
    // Salidas:  "writeBalls" estado nuevo; "binHits" casilla alcanzada por cada
    //           pelota (cada worker escribe unicamente su propio indice).
    void runRound(const Ball* readBalls, Ball* writeBalls, std::size_t ballCount,
                  const Peg* pegs, std::size_t pegCount,
                  const Modifier* modifiers, std::size_t modifierCount,
                  const SimulationParams& params,
                  float time, float dt,
                  int* binHits);

    std::size_t threadCount() const { return workers_.size(); }

private:
    // Ciclo de vida de cada worker: espera su turno, calcula y avisa.
    void workerLoop(std::size_t ballIndex);

    // Datos de la ronda en curso. Solo se leen despues de que el hilo
    // coordinador incremento "generation_" bajo el mutex, lo que establece la
    // relacion "happens-before" con las escrituras del coordinador.
    struct RoundData {
        const Ball* readBalls {nullptr};
        Ball* writeBalls {nullptr};
        std::size_t ballCount {0};
        const Peg* pegs {nullptr};
        std::size_t pegCount {0};
        const Modifier* modifiers {nullptr};
        std::size_t modifierCount {0};
        const SimulationParams* params {nullptr};
        float time {0.0F};
        float dt {0.0F};
        int* binHits {nullptr};
    };

    std::vector<std::thread> workers_;          // Un hilo por pelota.
    std::mutex mutex_;                          // Protege todo el estado de abajo.
    std::condition_variable startCondition_;    // Despierta a los workers.
    std::condition_variable finishedCondition_; // Avisa al coordinador.
    RoundData round_;                           // Datos de la ronda vigente.
    std::uint64_t generation_ {0};              // Numero de ronda; evita despertares espurios.
    std::size_t pendingWorkers_ {0};            // Workers que faltan por terminar.
    bool stopping_ {false};                     // Solicita la salida ordenada de los hilos.
};
