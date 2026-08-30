#include "Benchmark.h"

#include "Simulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <sys/stat.h>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

constexpr float BENCH_DELTA_TIME = 1.0F / 60.0F;  // Paso fijo: elimina el ruido del reloj real.
constexpr double TARGET_FRAME_MS = 1000.0 / 60.0; // Presupuesto de un cuadro a 60 FPS.
constexpr int STD_THREADS_LIMIT = 1024;           // Mas alla de esto el SO rechaza los hilos.

// Presupuesto de trabajo por medicion. Sirve para que N grande no dispare el
// tiempo total del banco: el numero de pasos se reduce de forma inversamente
// proporcional a N^2, que es la complejidad del nucleo.
constexpr double WORK_BUDGET_PAIRS = 6.0e8;

// Calcula cuantos pasos cronometrar para un N dado.
// Entradas: "ballCount" N evaluado; "requested" tope indicado por el usuario.
// Salida: cantidad de pasos, siempre en el rango [4, requested].
int stepsForBallCount(int ballCount, int requested, int substeps) {
    const double pairsPerStep =
        static_cast<double>(ballCount) * static_cast<double>(ballCount) *
        static_cast<double>(std::max(substeps, 1));
    const double allowed = WORK_BUDGET_PAIRS / std::max(pairsPerStep, 1.0);
    const int steps = static_cast<int>(std::min<double>(requested, std::max(4.0, allowed)));
    return std::max(4, steps);
}

// Crea el directorio contenedor de una ruta, si hace falta.
// Programacion defensiva: evita que el banco termine sin poder guardar el CSV
// despues de varios minutos de medicion.
void ensureParentDirectory(const std::string& path) {
    const std::size_t separator = path.find_last_of("/\\");
    if (separator == std::string::npos) {
        return;
    }
    const std::string directory = path.substr(0, separator);
    if (directory.empty()) {
        return;
    }
    // Se crean los niveles intermedios uno por uno.
    std::string partial;
    std::stringstream stream(directory);
    std::string piece;
    const bool absolute = directory[0] == '/';
    while (std::getline(stream, piece, '/')) {
        if (piece.empty()) {
            continue;
        }
        partial += (partial.empty() && !absolute) ? piece : "/" + piece;
#ifdef _WIN32
        _mkdir(partial.c_str());
#else
        mkdir(partial.c_str(), 0755);
#endif
    }
}

// Mide una combinacion concreta de parametros.
// Entradas: parametros de la escena, modo, hilos, repeticiones y pasos.
// Salida: registro con todas las estadisticas y las muestras individuales.
BenchmarkRecord measure(const SimulationParams& params, ExecutionMode mode,
                        int threads, int repetitions, int steps) {
    BenchmarkRecord record;
    record.ballCount = params.ballCount;
    record.mode = mode;
    record.threads = threads;
    record.steps = steps;
    record.repetitions = repetitions;

    if (mode == ExecutionMode::StdThreads && params.ballCount > STD_THREADS_LIMIT) {
        record.available = false;
        record.note = "N supera el limite practico de un hilo por pelota";
        return record;
    }

    Simulation simulation(params);

    // Calentamiento: llena las caches y deja que OpenMP cree su equipo de hilos
    // antes de cronometrar, de modo que ese costo no contamine la primera toma.
    for (int warmup = 0; warmup < 3; ++warmup) {
        simulation.step(BENCH_DELTA_TIME, mode, threads);
    }
    if (mode == ExecutionMode::StdThreads && !simulation.stdThreadsAvailable()) {
        record.available = false;
        record.note = "el sistema operativo rechazo la creacion de hilos: " +
                      simulation.stdThreadsError();
        return record;
    }

    record.samples.reserve(static_cast<std::size_t>(repetitions));
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int step = 0; step < steps; ++step) {
            simulation.step(BENCH_DELTA_TIME, mode, threads);
        }
        const auto end = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        record.samples.push_back(elapsedMs / static_cast<double>(steps));
    }

    const double total = std::accumulate(record.samples.begin(), record.samples.end(), 0.0);
    record.averageStepMs = total / static_cast<double>(record.samples.size());
    record.minimumStepMs = *std::min_element(record.samples.begin(), record.samples.end());
    record.maximumStepMs = *std::max_element(record.samples.begin(), record.samples.end());

    double variance = 0.0;
    for (const double sample : record.samples) {
        const double difference = sample - record.averageStepMs;
        variance += difference * difference;
    }
    variance /= static_cast<double>(record.samples.size());
    record.stdDevStepMs = std::sqrt(variance);
    record.maxFps = record.averageStepMs > 0.0 ? 1000.0 / record.averageStepMs : 0.0;
    return record;
}

// Devuelve la etiqueta corta de un modo mas su cantidad de hilos.
std::string modeLabel(const BenchmarkRecord& record) {
    std::ostringstream label;
    label << executionModeName(record.mode);
    if (record.mode != ExecutionMode::Sequential) {
        label << " x" << record.threads;
    }
    return label.str();
}

}  // namespace

int runBenchmark(const AppConfig& config) {
    AppConfig effective = config;

    // Valores por omision del banco: se eligen para cubrir desde una carga que
    // cabe en cache hasta una que satura los nucleos disponibles.
    if (effective.benchmarkBallCounts.empty()) {
        effective.benchmarkBallCounts = {250, 500, 1000, 2000, 4000};
    }
    if (effective.benchmarkThreadCounts.empty()) {
#ifdef _OPENMP
        const int maximum = std::max(1, omp_get_max_threads());
#else
        const int maximum = 1;
#endif
        effective.benchmarkThreadCounts.clear();
        for (int threads = 1; threads <= maximum; threads *= 2) {
            effective.benchmarkThreadCounts.push_back(threads);
        }
        if (effective.benchmarkThreadCounts.back() != maximum) {
            effective.benchmarkThreadCounts.push_back(maximum);
        }
    }
    std::sort(effective.benchmarkThreadCounts.begin(), effective.benchmarkThreadCounts.end());
    effective.benchmarkThreadCounts.erase(
        std::unique(effective.benchmarkThreadCounts.begin(), effective.benchmarkThreadCounts.end()),
        effective.benchmarkThreadCounts.end());
    std::sort(effective.benchmarkBallCounts.begin(), effective.benchmarkBallCounts.end());

#ifdef _OPENMP
    const int hardwareThreads = omp_get_max_threads();
    std::cout << "OpenMP disponible. Hilos maximos reportados: " << hardwareThreads << '\n';
#else
    std::cout << "AVISO: el binario se compilo sin OpenMP; los modos paralelos "
                 "de OpenMP se ejecutaran en un solo hilo.\n";
#endif
    std::cout << "Repeticiones por medicion: " << effective.benchmarkRepetitions
              << "  |  interacciones pelota-pelota: "
              << (effective.simulation.ballInteraction ? "si" : "no") << "\n\n";

    std::vector<BenchmarkRecord> records;

    for (const int ballCount : effective.benchmarkBallCounts) {
        SimulationParams params = effective.simulation;
        params.ballCount = ballCount;
        const int steps = stepsForBallCount(ballCount, effective.benchmarkSteps, params.substeps);

        std::cout << "== N = " << ballCount << "  (" << steps
                  << " pasos por repeticion) ==\n";

        // Referencia secuencial: es el denominador de todos los speedups.
        BenchmarkRecord baseline =
            measure(params, ExecutionMode::Sequential, 1, effective.benchmarkRepetitions, steps);
        baseline.speedupAverage = 1.0;
        baseline.speedupBest = 1.0;
        baseline.efficiency = 1.0;
        std::printf("  %-22s  %9.4f ms/paso  (min %8.4f  max %8.4f  sd %7.4f)  %7.1f FPS\n",
                    modeLabel(baseline).c_str(), baseline.averageStepMs,
                    baseline.minimumStepMs, baseline.maximumStepMs,
                    baseline.stdDevStepMs, baseline.maxFps);
        records.push_back(baseline);

        // Version historica de un hilo por pelota. Solo tiene sentido cuando N
        // es moderado; por encima del limite se documenta como no viable.
        BenchmarkRecord threadsRecord =
            measure(params, ExecutionMode::StdThreads, ballCount,
                    effective.benchmarkRepetitions, steps);
        if (threadsRecord.available) {
            threadsRecord.speedupAverage = baseline.averageStepMs / threadsRecord.averageStepMs;
            threadsRecord.speedupBest = baseline.minimumStepMs / threadsRecord.minimumStepMs;
            threadsRecord.efficiency =
                threadsRecord.speedupAverage / static_cast<double>(std::max(threadsRecord.threads, 1));
            std::printf("  %-22s  %9.4f ms/paso  (min %8.4f  max %8.4f  sd %7.4f)  "
                        "speedup %5.2fx  eficiencia %6.3f\n",
                        modeLabel(threadsRecord).c_str(), threadsRecord.averageStepMs,
                        threadsRecord.minimumStepMs, threadsRecord.maximumStepMs,
                        threadsRecord.stdDevStepMs, threadsRecord.speedupAverage,
                        threadsRecord.efficiency);
        } else {
            std::printf("  %-22s  no aplicable: %s\n",
                        modeLabel(threadsRecord).c_str(), threadsRecord.note.c_str());
        }
        records.push_back(threadsRecord);

        for (const ExecutionMode mode : {ExecutionMode::OpenMpStatic, ExecutionMode::OpenMpTuned}) {
            for (const int threads : effective.benchmarkThreadCounts) {
                BenchmarkRecord record =
                    measure(params, mode, threads, effective.benchmarkRepetitions, steps);
                record.speedupAverage = baseline.averageStepMs / record.averageStepMs;
                record.speedupBest = baseline.minimumStepMs / record.minimumStepMs;
                record.efficiency =
                    record.speedupAverage / static_cast<double>(std::max(threads, 1));
                std::printf("  %-22s  %9.4f ms/paso  (min %8.4f  max %8.4f  sd %7.4f)  "
                            "speedup %5.2fx  eficiencia %6.3f\n",
                            modeLabel(record).c_str(), record.averageStepMs,
                            record.minimumStepMs, record.maximumStepMs, record.stdDevStepMs,
                            record.speedupAverage, record.efficiency);
                records.push_back(record);
            }
        }
        std::cout << '\n';
    }

    // --- Escritura del CSV agregado ----------------------------------------
    ensureParentDirectory(effective.benchmarkOutput);
    std::ofstream summary(effective.benchmarkOutput);
    if (!summary) {
        std::cerr << "No se pudo escribir el archivo de resultados '"
                  << effective.benchmarkOutput << "'.\n";
        return 2;
    }
    summary << "n_balls,mode,threads,steps,repetitions,avg_ms,min_ms,max_ms,stddev_ms,"
               "speedup_avg,speedup_best,efficiency,max_fps,available,note\n";
    for (const BenchmarkRecord& record : records) {
        summary << record.ballCount << ',' << executionModeName(record.mode) << ','
                << record.threads << ',' << record.steps << ',' << record.repetitions << ','
                << std::fixed << std::setprecision(6)
                << record.averageStepMs << ',' << record.minimumStepMs << ','
                << record.maximumStepMs << ',' << record.stdDevStepMs << ','
                << record.speedupAverage << ',' << record.speedupBest << ','
                << record.efficiency << ',' << record.maxFps << ','
                << (record.available ? 1 : 0) << ',' << record.note << '\n';
    }
    summary.close();

    // --- Escritura del CSV con cada repeticion (bitacora de pruebas) --------
    std::string rawPath = effective.benchmarkOutput;
    const std::size_t dot = rawPath.find_last_of('.');
    rawPath = (dot == std::string::npos ? rawPath : rawPath.substr(0, dot)) + "_muestras.csv";
    std::ofstream raw(rawPath);
    if (!raw) {
        std::cerr << "No se pudo escribir el archivo de muestras '" << rawPath << "'.\n";
        return 3;
    }
    raw << "n_balls,mode,threads,repetition,ms_per_step\n";
    for (const BenchmarkRecord& record : records) {
        for (std::size_t repetition = 0; repetition < record.samples.size(); ++repetition) {
            raw << record.ballCount << ',' << executionModeName(record.mode) << ','
                << record.threads << ',' << (repetition + 1) << ','
                << std::fixed << std::setprecision(6) << record.samples[repetition] << '\n';
        }
    }
    raw.close();

    // --- Maxima cantidad de pelotas que sostiene 60 FPS --------------------
    std::cout << "== Presupuesto de 60 FPS (" << TARGET_FRAME_MS
              << " ms por cuadro) ==\n";
    for (const BenchmarkRecord& record : records) {
        if (!record.available) {
            continue;
        }
        if (record.averageStepMs <= TARGET_FRAME_MS) {
            continue;
        }
        std::printf("  N=%-6d %-22s excede el presupuesto: %.2f ms por paso\n",
                    record.ballCount, modeLabel(record).c_str(), record.averageStepMs);
    }

    std::cout << "\nResultados agregados: " << effective.benchmarkOutput << '\n'
              << "Muestras individuales: " << rawPath << '\n';
    return 0;
}
