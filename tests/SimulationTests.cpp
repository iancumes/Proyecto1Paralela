// ---------------------------------------------------------------------------
// Pruebas automatizadas del proyecto.
//
// El objetivo central es demostrar que la paralelizacion no altera el
// resultado: las cuatro estrategias deben producir exactamente el mismo estado
// a partir de la misma semilla, y el resultado no debe depender de cuantos
// hilos se usen. Eso es lo que valida el diseno de doble buffer y la ausencia
// de condiciones de carrera.
//
// Se ejecutan con:  ctest --test-dir build --output-on-failure
// ---------------------------------------------------------------------------
#include "Simulation.h"
#include "SimulationConfig.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Parametros compartidos por las pruebas: escena pequena pero con todos los
// elementos activos (clavijas oscilantes, modificadores e interacciones).
SimulationParams testParams(int ballCount) {
    SimulationParams params;
    params.ballCount = ballCount;
    params.pegLevels = 7;
    params.pegsPerBaseRing = 14;
    params.modifierCount = 4;
    params.binCount = 10;
    params.substeps = 2;
    params.ballInteraction = true;
    params.seed = 12345U;
    return params;
}

// Compara dos pelotas exigiendo igualdad exacta bit a bit. No se usa una
// tolerancia a proposito: el diseno garantiza que las operaciones de punto
// flotante se ejecutan en el mismo orden en todos los modos, asi que cualquier
// diferencia indicaria una condicion de carrera real.
bool identical(const Ball& left, const Ball& right) {
    return left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && left.velocity.x == right.velocity.x
        && left.velocity.y == right.velocity.y
        && left.velocity.z == right.velocity.z
        && left.seed == right.seed
        && left.active == right.active;
}

// Informa la primera diferencia encontrada entre dos estados.
bool compareStates(const std::vector<Ball>& expected, const std::vector<Ball>& actual,
                   const char* label) {
    if (expected.size() != actual.size()) {
        std::cerr << "FALLA [" << label << "]: cantidad de pelotas distinta ("
                  << expected.size() << " contra " << actual.size() << ").\n";
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (!identical(expected[index], actual[index])) {
            std::cerr << "FALLA [" << label << "]: la pelota " << index << " difiere.\n"
                      << "  esperado: (" << expected[index].position.x << ", "
                      << expected[index].position.y << ", " << expected[index].position.z << ")\n"
                      << "  obtenido: (" << actual[index].position.x << ", "
                      << actual[index].position.y << ", " << actual[index].position.z << ")\n";
            return false;
        }
    }
    return true;
}

// Avanza una simulacion una cantidad fija de pasos con el modo indicado.
void advance(Simulation& simulation, ExecutionMode mode, int threads, int steps) {
    constexpr float DT = 1.0F / 60.0F;
    for (int step = 0; step < steps; ++step) {
        simulation.step(DT, mode, threads);
    }
}

// -------------------------------------------------------------------------
// Prueba 1: los cuatro modos producen el mismo estado final.
// -------------------------------------------------------------------------
int testEquivalencia() {
    constexpr int BALLS = 220;
    constexpr int STEPS = 180;
    const SimulationParams params = testParams(BALLS);

    Simulation reference(params);
    advance(reference, ExecutionMode::Sequential, 1, STEPS);
    const std::vector<Ball> expected = reference.balls();
    const long long expectedRecycled = reference.recycledBalls();

    struct Case {
        ExecutionMode mode;
        int threads;
        const char* label;
    };
    const std::vector<Case> cases {
        {ExecutionMode::StdThreads,   BALLS, "std::thread (1 por pelota)"},
        {ExecutionMode::OpenMpStatic, 2,     "openmp-static x2"},
        {ExecutionMode::OpenMpStatic, 4,     "openmp-static x4"},
        {ExecutionMode::OpenMpTuned,  2,     "openmp-tuned x2"},
        {ExecutionMode::OpenMpTuned,  4,     "openmp-tuned x4"},
        {ExecutionMode::OpenMpTuned,  8,     "openmp-tuned x8"},
    };

    int failures = 0;
    for (const Case& testCase : cases) {
        Simulation candidate(params);
        advance(candidate, testCase.mode, testCase.threads, STEPS);

        if (testCase.mode == ExecutionMode::StdThreads && !candidate.stdThreadsAvailable()) {
            std::cout << "  OMITIDA [" << testCase.label
                      << "]: el sistema operativo no permitio crear los hilos.\n";
            continue;
        }
        if (!compareStates(expected, candidate.balls(), testCase.label)) {
            ++failures;
            continue;
        }
        if (candidate.recycledBalls() != expectedRecycled) {
            std::cerr << "FALLA [" << testCase.label << "]: pelotas recicladas "
                      << candidate.recycledBalls() << " contra " << expectedRecycled << ".\n";
            ++failures;
            continue;
        }
        std::cout << "  OK [" << testCase.label << "] estado identico tras "
                  << STEPS << " pasos.\n";
    }

    if (failures == 0) {
        std::cout << "Equivalencia verificada: la paralelizacion no altera el resultado.\n";
    }
    return failures == 0 ? 0 : 1;
}

// -------------------------------------------------------------------------
// Prueba 2: el resultado no depende de la cantidad de hilos.
// Es la prueba que detectaria una escritura compartida sin proteger.
// -------------------------------------------------------------------------
int testDeterminismo() {
    constexpr int BALLS = 160;
    constexpr int STEPS = 240;
    const SimulationParams params = testParams(BALLS);

    Simulation single(params);
    advance(single, ExecutionMode::OpenMpTuned, 1, STEPS);
    const std::vector<Ball> expected = single.balls();
    const std::vector<long long> expectedBins = single.binCounts();

    int failures = 0;
    for (const int threads : {1, 2, 3, 5, 8, 16}) {
        Simulation candidate(params);
        advance(candidate, ExecutionMode::OpenMpTuned, threads, STEPS);
        const std::string label = "openmp-tuned con " + std::to_string(threads) + " hilos";
        if (!compareStates(expected, candidate.balls(), label.c_str())) {
            ++failures;
            continue;
        }
        if (candidate.binCounts() != expectedBins) {
            std::cerr << "FALLA [" << label << "]: el conteo por casilla difiere.\n";
            ++failures;
            continue;
        }
        std::cout << "  OK [" << label << "] estado y conteos identicos.\n";
    }

    if (failures == 0) {
        std::cout << "Determinismo verificado: el resultado no depende del numero de hilos.\n";
    }
    return failures == 0 ? 0 : 1;
}

// -------------------------------------------------------------------------
// Prueba 3: el analisis de argumentos rechaza las entradas invalidas.
// -------------------------------------------------------------------------
int testArgumentos() {
    struct Case {
        std::vector<std::string> arguments;
        bool shouldSucceed;
        const char* description;
    };

    const std::vector<Case> cases {
        {{"plinko3d", "--no-prompt", "-n", "500"}, true,  "N valido"},
        {{"plinko3d", "--no-prompt", "-n", "0"}, false, "N igual a cero"},
        {{"plinko3d", "--no-prompt", "-n", "-8"}, false, "N negativo"},
        {{"plinko3d", "--no-prompt", "-n", "abc"}, false, "N no numerico"},
        {{"plinko3d", "--no-prompt", "-n", "999999999"}, false, "N fuera de rango"},
        {{"plinko3d", "--no-prompt", "-n"}, false, "opcion sin valor"},
        {{"plinko3d", "--no-prompt", "-w", "320"}, false, "ancho menor que 640"},
        {{"plinko3d", "--no-prompt", "-h", "200"}, false, "alto menor que 480"},
        {{"plinko3d", "--no-prompt", "-w", "800", "-h", "600"}, true, "lienzo valido"},
        {{"plinko3d", "--no-prompt", "--mode", "cuantico"}, false, "modo inexistente"},
        {{"plinko3d", "--no-prompt", "--mode", "omp-tuned"}, true, "modo valido"},
        {{"plinko3d", "--no-prompt", "--threads", "-3"}, false, "hilos negativos"},
        {{"plinko3d", "--no-prompt", "--gravity", "9.81"}, false, "gravedad positiva"},
        {{"plinko3d", "--no-prompt", "--restitution", "1.8"}, false, "restitucion mayor que uno"},
        {{"plinko3d", "--no-prompt", "--pegs", "8"}, false, "piramide sin separador"},
        {{"plinko3d", "--no-prompt", "--pegs", "11x22"}, true, "piramide valida"},
        {{"plinko3d", "--no-prompt", "--board-radius", "0.05"}, false, "radio del tablero muy pequeno"},
        {{"plinko3d", "--no-prompt", "--rotation", "900"}, false, "rotacion fuera de rango"},
        {{"plinko3d", "--no-prompt", "--pitch", "120"}, false, "inclinacion fuera de rango"},
        {{"plinko3d", "--no-prompt", "--rotation", "25", "--pitch", "20"}, true, "camara valida"},
        {{"plinko3d", "--no-prompt", "--substeps", "0"}, false, "sub-pasos en cero"},
        {{"plinko3d", "--no-prompt", "--radius", "0.0"}, false, "radio nulo"},
        {{"plinko3d", "--no-prompt", "--benchmark", "--bench-reps", "3"}, false,
         "menos de 10 repeticiones"},
        {{"plinko3d", "--no-prompt", "--bench-balls", "100,doscientos"}, false,
         "lista de N mal formada"},
        {{"plinko3d", "--no-prompt", "--opcion-inventada"}, false, "argumento desconocido"},
    };

    int failures = 0;
    for (const Case& testCase : cases) {
        std::vector<char*> argv;
        argv.reserve(testCase.arguments.size());
        for (const std::string& argument : testCase.arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }

        AppConfig config;
        std::string errorMessage;
        const bool accepted =
            parseCommandLine(static_cast<int>(argv.size()), argv.data(), config, errorMessage);

        if (accepted != testCase.shouldSucceed) {
            std::cerr << "FALLA [" << testCase.description << "]: se esperaba "
                      << (testCase.shouldSucceed ? "aceptar" : "rechazar")
                      << " y ocurrio lo contrario. Mensaje: '" << errorMessage << "'\n";
            ++failures;
            continue;
        }
        if (!accepted && errorMessage.empty()) {
            std::cerr << "FALLA [" << testCase.description
                      << "]: se rechazo sin explicar el motivo.\n";
            ++failures;
            continue;
        }
        std::cout << "  OK [" << testCase.description << "]"
                  << (accepted ? " aceptado" : " rechazado: " + errorMessage) << '\n';
    }

    if (failures == 0) {
        std::cout << "Programacion defensiva verificada en " << cases.size()
                  << " casos de argumentos.\n";
    }
    return failures == 0 ? 0 : 1;
}

// -------------------------------------------------------------------------
// Prueba 4: los conteos por casilla suman exactamente las pelotas recicladas.
// Detectaria una perdida de incrementos por una carrera en los contadores
// compartidos (que es justo lo que protegen "atomic" y "critical").
// -------------------------------------------------------------------------
int testConteos() {
    constexpr int BALLS = 300;
    constexpr int STEPS = 300;
    const SimulationParams params = testParams(BALLS);

    struct Case {
        ExecutionMode mode;
        int threads;
        const char* label;
    };
    const std::vector<Case> cases {
        {ExecutionMode::Sequential,   1, "secuencial"},
        {ExecutionMode::OpenMpStatic, 8, "openmp-static x8"},
        {ExecutionMode::OpenMpTuned,  8, "openmp-tuned x8"},
    };

    int failures = 0;
    for (const Case& testCase : cases) {
        Simulation simulation(params);
        advance(simulation, testCase.mode, testCase.threads, STEPS);

        long long total = 0;
        for (const long long count : simulation.binCounts()) {
            total += count;
        }
        if (total != simulation.recycledBalls()) {
            std::cerr << "FALLA [" << testCase.label << "]: la suma de casillas ("
                      << total << ") no coincide con las pelotas recicladas ("
                      << simulation.recycledBalls() << ").\n";
            ++failures;
            continue;
        }
        if (total <= 0) {
            std::cerr << "FALLA [" << testCase.label
                      << "]: ninguna pelota llego al fondo; la prueba no midio nada.\n";
            ++failures;
            continue;
        }
        std::cout << "  OK [" << testCase.label << "] " << total
                  << " pelotas contadas sin perdidas.\n";
    }

    if (failures == 0) {
        std::cout << "Contadores compartidos verificados: no se pierde ningun incremento.\n";
    }
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string suite = argc > 1 ? argv[1] : "todas";

    if (suite == "equivalencia") { return testEquivalencia(); }
    if (suite == "determinismo") { return testDeterminismo(); }
    if (suite == "argumentos")   { return testArgumentos(); }
    if (suite == "conteos")      { return testConteos(); }

    if (suite == "todas") {
        int failures = 0;
        std::cout << "--- equivalencia ---\n"; failures += testEquivalencia();
        std::cout << "--- determinismo ---\n"; failures += testDeterminismo();
        std::cout << "--- argumentos ---\n";   failures += testArgumentos();
        std::cout << "--- conteos ---\n";      failures += testConteos();
        std::cout << (failures == 0 ? "\nTODAS LAS PRUEBAS APROBADAS\n"
                                    : "\nHAY PRUEBAS FALLIDAS\n");
        return failures == 0 ? 0 : 1;
    }

    std::cerr << "Suite desconocida: '" << suite
              << "'. Use equivalencia, determinismo, argumentos, conteos o todas.\n";
    return 2;
}
