#pragma once

#include "Entities.h"
#include "SimulationConfig.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BallThreadSystem;

// ---------------------------------------------------------------------------
// Simulation: estado completo del tablero y las cuatro estrategias de avance.
//
// Diseno paralelo (metodo PCAM):
//   Particion    -> una tarea por pelota; es el arreglo que crece con N.
//   Comunicacion -> cada tarea lee el estado del cuadro anterior de todas las
//                   demas pelotas, clavijas y modificadores, pero escribe
//                   unicamente su propia posicion en el buffer de salida.
//   Aglomeracion -> las tareas se agrupan en bloques contiguos de indices para
//                   aprovechar la localidad de cache.
//   Mapeo        -> los bloques se reparten entre hilos con OpenMP.
//
// La clave del diseno es el doble buffer: "current_" es de solo lectura durante
// todo el paso y "next_" es de solo escritura, con un unico escritor por
// indice. Eso elimina por construccion las condiciones de carrera sobre el
// estado de las pelotas y hace que el resultado sea identico bit a bit sin
// importar cuantos hilos se usen.
// ---------------------------------------------------------------------------
class Simulation {
public:
    // Construye la escena a partir de los parametros validados.
    explicit Simulation(const SimulationParams& params);
    ~Simulation();

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    // Regenera pelotas, clavijas y modificadores con la semilla indicada.
    // Entrada: "seed" semilla global del generador pseudoaleatorio.
    void reset(std::uint32_t seed);

    // Cambia la cantidad de pelotas conservando el resto de la escena.
    // Entrada: "ballCount" nuevo valor de N (se recorta al rango valido).
    void resizeBallCount(int ballCount);

    // Avanza la simulacion un cuadro con la estrategia solicitada.
    // Entradas: "dt" tiempo transcurrido en segundos; "mode" estrategia;
    //           "threadCount" hilos de OpenMP (0 = todos los disponibles).
    void step(float dt, ExecutionMode mode, int threadCount);

    // Estrategias individuales. Se exponen para que las pruebas puedan
    // compararlas entre si sobre el mismo estado inicial.
    void stepSequential(float dt);
    void stepStdThreads(float dt);
    void stepOpenMpStatic(float dt, int threadCount);
    void stepOpenMpTuned(float dt, int threadCount);

    // Accesores de solo lectura usados por el renderizador.
    const std::vector<Ball>& balls() const { return current_; }
    const std::vector<Peg>& pegs() const { return pegs_; }
    const std::vector<Modifier>& modifiers() const { return modifiers_; }
    const std::vector<long long>& binCounts() const { return binCounts_; }
    const SimulationParams& params() const { return params_; }
    float time() const { return time_; }
    long long recycledBalls() const { return recycledBalls_; }

    // Cantidad de hilos std::thread vivos (0 si el modo no los usa todavia).
    std::size_t stdThreadCount() const;

    // Hilos realmente utilizados por OpenMP en el ultimo paso paralelo.
    int lastOpenMpThreads() const { return lastOpenMpThreads_; }

    // Desbalance de carga del ultimo paso "omp-tuned": tiempo maximo por hilo
    // dividido entre el tiempo promedio. 1.0 es un reparto perfecto.
    double lastLoadImbalance() const { return lastLoadImbalance_; }

    // Indica si el sistema de un hilo por pelota pudo crearse. Cuando N es muy
    // grande el sistema operativo rechaza la creacion y se reporta el fallo en
    // lugar de terminar el programa.
    bool stdThreadsAvailable() const { return stdThreadsAvailable_; }
    const std::string& stdThreadsError() const { return stdThreadsError_; }

private:
    // Reconstruye el arreglo de clavijas segun filas y columnas configuradas.
    void buildPegs(std::uint32_t& seedState);
    // Reconstruye las zonas modificadoras.
    void buildModifiers(std::uint32_t& seedState);
    // Coloca una pelota en la parte alta del tablero con color y velocidad nuevos.
    void spawnBall(Ball& ball, std::uint32_t index, std::uint32_t& seedState) const;
    // Libera los hilos persistentes si existen.
    void releaseStdThreads();
    // Crea (si hace falta) el sistema de un hilo por pelota.
    void ensureStdThreads();

    SimulationParams params_;              // Parametros inmutables durante el paso.
    std::vector<Ball> current_;            // Buffer de lectura del paso actual.
    std::vector<Ball> next_;               // Buffer de escritura del paso actual.
    std::vector<Peg> pegs_;                // Clavijas fijas u oscilantes.
    std::vector<Modifier> modifiers_;      // Zonas que alteran la fisica.
    std::vector<long long> binCounts_;     // Conteo acumulado por casilla.
    std::vector<int> binHits_;             // Casilla alcanzada por cada pelota en el paso.
    float time_ {0.0F};                    // Reloj de simulacion en segundos.
    long long recycledBalls_ {0};          // Total de pelotas recicladas.
    std::uint32_t seed_ {0};               // Semilla global vigente.
    int lastOpenMpThreads_ {1};            // Hilos usados en el ultimo paso OpenMP.
    double lastLoadImbalance_ {1.0};       // Metrica de desbalance de carga.

    // Tiempo ocupado por hilo, con relleno para que dos hilos nunca escriban
    // en la misma linea de cache (evita "false sharing").
    struct alignas(64) PaddedTime { double seconds {0.0}; };
    std::vector<PaddedTime> threadBusyTime_;

    std::unique_ptr<BallThreadSystem> stdThreads_; // Version con un hilo por pelota.
    bool stdThreadsAvailable_ {true};      // Falso si el sistema operativo los rechazo.
    std::string stdThreadsError_;          // Detalle del fallo, si lo hubo.

    friend class BallThreadSystem;
};

// Aplica el nucleo de fisica a una sola pelota.
// Entradas: "index" pelota a actualizar; los arreglos del estado del cuadro
//           anterior; "time" reloj de simulacion; "dt" paso de integracion.
// Salidas:  "binHit" casilla alcanzada (-1 si la pelota no llego al fondo).
// Devuelve el nuevo estado de la pelota. Es una funcion pura sobre entradas de
// solo lectura, por lo que varios hilos pueden ejecutarla a la vez sin candados.
Ball advanceBall(std::size_t index,
                 const Ball* balls, std::size_t ballCount,
                 const Peg* pegs, std::size_t pegCount,
                 const Modifier* modifiers, std::size_t modifierCount,
                 const SimulationParams& params,
                 float time, float dt,
                 int& binHit);
