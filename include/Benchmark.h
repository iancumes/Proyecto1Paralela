#pragma once

#include "SimulationConfig.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Banco de pruebas sin ventana.
//
// Ejecuta la misma fisica que el screensaver pero sin renderizado, de modo que
// el tiempo medido corresponde unicamente al trabajo que se paraleliza. Cada
// combinacion (N, modo, hilos) se repite varias veces y se reportan promedio,
// minimo, maximo y desviacion estandar, ademas del speedup y la eficiencia
// respecto de la version secuencial con el mismo N.
// ---------------------------------------------------------------------------

// Resultado agregado de una combinacion de parametros.
struct BenchmarkRecord {
    int ballCount = 0;             // N evaluado.
    ExecutionMode mode = ExecutionMode::Sequential; // Estrategia evaluada.
    int threads = 1;               // Hilos solicitados.
    int steps = 0;                 // Pasos de fisica cronometrados por repeticion.
    int repetitions = 0;           // Repeticiones realizadas.
    double averageStepMs = 0.0;    // Tiempo promedio por paso de fisica.
    double minimumStepMs = 0.0;    // Mejor tiempo por paso observado.
    double maximumStepMs = 0.0;    // Peor tiempo por paso observado.
    double stdDevStepMs = 0.0;     // Desviacion estandar del tiempo por paso.
    double speedupAverage = 0.0;   // Speedup usando el tiempo promedio.
    double speedupBest = 0.0;      // Speedup usando el mejor tiempo de cada version.
    double efficiency = 0.0;       // Eficiencia = speedup / hilos.
    double maxFps = 0.0;           // Cuadros por segundo que permite la fisica sola.
    bool available = true;         // Falso si la combinacion no pudo ejecutarse.
    std::string note;              // Motivo cuando "available" es falso.
    std::vector<double> samples;   // Tiempo por paso de cada repeticion (ms).
};

// Ejecuta el banco completo descrito en "config".
// Entradas: "config" configuracion validada con "runBenchmark" activo.
// Salidas:  archivos CSV en "config.benchmarkOutput" y un resumen por consola.
// Devuelve 0 si todo fue bien, distinto de 0 si no pudo escribirse el CSV.
int runBenchmark(const AppConfig& config);
