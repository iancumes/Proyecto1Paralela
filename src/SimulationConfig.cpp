#include "SimulationConfig.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace {

// Limites duros. Existen para que un valor absurdo (por ejemplo N = 10^9) no
// agote la memoria del equipo ni congele la ventana antes de mostrar el error.
constexpr int MIN_BALLS = 1;
constexpr int MAX_BALLS = 200000;
constexpr int MIN_WINDOW_WIDTH = 640;   // Exigencia del enunciado.
constexpr int MIN_WINDOW_HEIGHT = 480;  // Exigencia del enunciado.
constexpr int MAX_WINDOW_SIDE = 7680;
constexpr int MAX_THREADS = 512;

// Convierte una cadena a entero validando que se consuma por completo.
// Entradas: "text" cadena a convertir.
// Salidas: "value" entero resultante. Devuelve false si la cadena no es un
// entero valido o si desborda el rango de "int".
bool parseInteger(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

// Convierte una cadena a flotante validando que se consuma por completo.
bool parseFloatValue(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return false;
    }
    value = static_cast<float>(parsed);
    return true;
}

// Convierte "8,16,32" en una lista de enteros positivos.
// Devuelve false si algun elemento no es un entero valido o no es positivo.
bool parseIntegerList(const std::string& text, std::vector<int>& values) {
    values.clear();
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        // Elimina espacios accidentales alrededor de cada elemento.
        const auto first = item.find_first_not_of(" \t");
        const auto last = item.find_last_not_of(" \t");
        if (first == std::string::npos) {
            return false;
        }
        item = item.substr(first, last - first + 1);
        int parsed = 0;
        if (!parseInteger(item, parsed) || parsed <= 0) {
            return false;
        }
        values.push_back(parsed);
    }
    return !values.empty();
}

// Indica si el siguiente argumento existe; evita leer fuera de "argv".
bool hasValue(int index, int argc) {
    return index + 1 < argc;
}

}  // namespace

const char* executionModeName(ExecutionMode mode) {
    switch (mode) {
        case ExecutionMode::Sequential:   return "secuencial";
        case ExecutionMode::StdThreads:   return "std::thread";
        case ExecutionMode::OpenMpStatic: return "openmp-static";
        case ExecutionMode::OpenMpTuned:  return "openmp-tuned";
    }
    return "desconocido";
}

bool parseExecutionMode(const std::string& text, ExecutionMode& mode) {
    if (text == "seq" || text == "secuencial" || text == "sequential") {
        mode = ExecutionMode::Sequential;
        return true;
    }
    if (text == "threads" || text == "stdthreads" || text == "hilos") {
        mode = ExecutionMode::StdThreads;
        return true;
    }
    if (text == "omp-static" || text == "static" || text == "omp") {
        mode = ExecutionMode::OpenMpStatic;
        return true;
    }
    if (text == "omp-tuned" || text == "tuned" || text == "omp2") {
        mode = ExecutionMode::OpenMpTuned;
        return true;
    }
    return false;
}

std::string usageText(const char* programName) {
    std::ostringstream text;
    text <<
        "Plinko 3D Paralelo - screensaver con fisica y OpenMP\n"
        "\n"
        "Uso: " << programName << " [opciones]\n"
        "\n"
        "Opciones de la escena:\n"
        "  -n, --balls <entero>       Cantidad N de pelotas a simular (1..200000).\n"
        "      --pegs <filas>x<cols>  Rejilla de clavijas, por ejemplo 8x11.\n"
        "      --modifiers <entero>   Cantidad K de zonas modificadoras (0..256).\n"
        "      --bins <entero>        Casillas contadoras en la base (1..64).\n"
        "      --radius <decimal>     Radio de cada pelota (0.01..1.5).\n"
        "      --gravity <decimal>    Gravedad en unidades/s^2 (debe ser negativa).\n"
        "      --restitution <dec>    Coeficiente de restitucion (0..1).\n"
        "      --substeps <entero>    Sub-pasos de integracion por cuadro (1..16).\n"
        "      --no-interaction       Desactiva las colisiones pelota-pelota O(N^2).\n"
        "      --seed <entero>        Semilla del generador pseudoaleatorio.\n"
        "\n"
        "Opciones de la ventana:\n"
        "  -w, --width <entero>       Ancho del lienzo en pixeles (minimo 640).\n"
        "  -h, --height <entero>      Alto del lienzo en pixeles (minimo 480).\n"
        "      --no-vsync             Desactiva la sincronia vertical.\n"
        "\n"
        "Opciones de ejecucion:\n"
        "  -m, --mode <modo>          seq | threads | omp-static | omp-tuned.\n"
        "  -t, --threads <entero>     Hilos de OpenMP (0 = todos los nucleos).\n"
        "      --no-prompt            No solicitar datos por consola si faltan.\n"
        "\n"
        "Opciones de medicion:\n"
        "      --benchmark            Ejecuta el banco de pruebas sin abrir ventana.\n"
        "      --bench-balls <lista>  Valores de N separados por coma.\n"
        "      --bench-threads <lst>  Cantidades de hilos separadas por coma.\n"
        "      --bench-reps <entero>  Repeticiones por medicion (minimo 10).\n"
        "      --bench-steps <entero> Pasos de fisica por repeticion (minimo 10).\n"
        "      --bench-out <ruta>     Archivo CSV donde se escriben los resultados.\n"
        "      --screenshot <ruta>    Guarda una captura BMP y termina.\n"
        "      --warmup <entero>      Cuadros simulados antes de la captura.\n"
        "\n"
        "  --help                     Muestra esta ayuda.\n"
        "\n"
        "Controles en ejecucion:\n"
        "  0/1/2/3  cambia a secuencial / std::thread / omp-static / omp-tuned\n"
        "  ESPACIO  rota entre los modos disponibles\n"
        "  + / -    aumenta o reduce la cantidad de hilos de OpenMP\n"
        "  R        reinicia la escena con una semilla nueva\n"
        "  ESC      cierra la aplicacion\n";
    return text.str();
}

bool parseCommandLine(int argc, char** argv, AppConfig& config, std::string& errorMessage) {
    errorMessage.clear();
    bool ballCountProvided = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        auto requireValue = [&](std::string& out) -> bool {
            if (!hasValue(index, argc)) {
                errorMessage = "La opcion '" + argument + "' requiere un valor.";
                return false;
            }
            out = argv[++index];
            return true;
        };

        if (argument == "--help" || argument == "-?") {
            config.showHelp = true;
            return true;
        }

        if (argument == "-n" || argument == "--balls") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.simulation.ballCount)) {
                errorMessage = "El valor de '" + argument + "' debe ser un entero: se recibio '" + value + "'.";
                return false;
            }
            ballCountProvided = true;
        } else if (argument == "--pegs") {
            std::string value;
            if (!requireValue(value)) { return false; }
            const std::size_t separator = value.find_first_of("xX");
            if (separator == std::string::npos ||
                !parseInteger(value.substr(0, separator), config.simulation.pegRows) ||
                !parseInteger(value.substr(separator + 1), config.simulation.pegColumns)) {
                errorMessage = "El formato de '--pegs' debe ser <filas>x<columnas>, por ejemplo 8x11.";
                return false;
            }
        } else if (argument == "--modifiers") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.simulation.modifierCount)) {
                errorMessage = "El valor de '--modifiers' debe ser un entero.";
                return false;
            }
        } else if (argument == "--bins") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.simulation.binCount)) {
                errorMessage = "El valor de '--bins' debe ser un entero.";
                return false;
            }
        } else if (argument == "--radius") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseFloatValue(value, config.simulation.ballRadius)) {
                errorMessage = "El valor de '--radius' debe ser un numero decimal.";
                return false;
            }
        } else if (argument == "--gravity") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseFloatValue(value, config.simulation.gravity)) {
                errorMessage = "El valor de '--gravity' debe ser un numero decimal.";
                return false;
            }
        } else if (argument == "--restitution") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseFloatValue(value, config.simulation.restitution)) {
                errorMessage = "El valor de '--restitution' debe ser un numero decimal.";
                return false;
            }
        } else if (argument == "--substeps") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.simulation.substeps)) {
                errorMessage = "El valor de '--substeps' debe ser un entero.";
                return false;
            }
        } else if (argument == "--no-interaction") {
            config.simulation.ballInteraction = false;
        } else if (argument == "--seed") {
            std::string value;
            if (!requireValue(value)) { return false; }
            int parsed = 0;
            if (!parseInteger(value, parsed) || parsed < 0) {
                errorMessage = "El valor de '--seed' debe ser un entero no negativo.";
                return false;
            }
            config.simulation.seed = static_cast<std::uint32_t>(parsed);
        } else if (argument == "-w" || argument == "--width") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.windowWidth)) {
                errorMessage = "El valor de '" + argument + "' debe ser un entero.";
                return false;
            }
        } else if (argument == "-h" || argument == "--height") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.windowHeight)) {
                errorMessage = "El valor de '" + argument + "' debe ser un entero.";
                return false;
            }
        } else if (argument == "--no-vsync") {
            config.vsync = false;
        } else if (argument == "-m" || argument == "--mode") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseExecutionMode(value, config.mode)) {
                errorMessage = "Modo desconocido '" + value +
                    "'. Use seq, threads, omp-static u omp-tuned.";
                return false;
            }
        } else if (argument == "-t" || argument == "--threads") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.threadCount)) {
                errorMessage = "El valor de '" + argument + "' debe ser un entero.";
                return false;
            }
        } else if (argument == "--no-prompt") {
            config.allowPrompt = false;
        } else if (argument == "--benchmark") {
            config.runBenchmark = true;
            config.allowPrompt = false;
        } else if (argument == "--bench-balls") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseIntegerList(value, config.benchmarkBallCounts)) {
                errorMessage = "'--bench-balls' espera enteros positivos separados por coma.";
                return false;
            }
        } else if (argument == "--bench-threads") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseIntegerList(value, config.benchmarkThreadCounts)) {
                errorMessage = "'--bench-threads' espera enteros positivos separados por coma.";
                return false;
            }
        } else if (argument == "--bench-reps") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.benchmarkRepetitions)) {
                errorMessage = "El valor de '--bench-reps' debe ser un entero.";
                return false;
            }
        } else if (argument == "--bench-steps") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.benchmarkSteps)) {
                errorMessage = "El valor de '--bench-steps' debe ser un entero.";
                return false;
            }
        } else if (argument == "--bench-out") {
            if (!requireValue(config.benchmarkOutput)) { return false; }
        } else if (argument == "--screenshot") {
            if (!requireValue(config.screenshotPath)) { return false; }
        } else if (argument == "--warmup") {
            std::string value;
            if (!requireValue(value)) { return false; }
            if (!parseInteger(value, config.screenshotWarmupFrames) ||
                config.screenshotWarmupFrames < 0) {
                errorMessage = "El valor de '--warmup' debe ser un entero no negativo.";
                return false;
            }
        } else {
            errorMessage = "Argumento no reconocido: '" + argument + "'.";
            return false;
        }
    }

    // Solo se pregunta por consola cuando el usuario no indico N explicitamente.
    promptForMissingValues(config, ballCountProvided);
    return validateConfig(config, errorMessage);
}

bool validateConfig(const AppConfig& config, std::string& errorMessage) {
    const SimulationParams& simulation = config.simulation;

    if (simulation.ballCount < MIN_BALLS || simulation.ballCount > MAX_BALLS) {
        errorMessage = "La cantidad de pelotas debe estar entre " +
            std::to_string(MIN_BALLS) + " y " + std::to_string(MAX_BALLS) + ".";
        return false;
    }
    if (simulation.pegRows < 0 || simulation.pegRows > 64 ||
        simulation.pegColumns < 0 || simulation.pegColumns > 64) {
        errorMessage = "La rejilla de clavijas debe tener entre 0 y 64 filas y columnas.";
        return false;
    }
    if (simulation.modifierCount < 0 || simulation.modifierCount > 256) {
        errorMessage = "La cantidad de modificadores debe estar entre 0 y 256.";
        return false;
    }
    if (simulation.binCount < 1 || simulation.binCount > 64) {
        errorMessage = "La cantidad de casillas debe estar entre 1 y 64.";
        return false;
    }
    if (simulation.substeps < 1 || simulation.substeps > 16) {
        errorMessage = "Los sub-pasos de integracion deben estar entre 1 y 16.";
        return false;
    }
    if (!(simulation.ballRadius > 0.01F) || simulation.ballRadius > 1.5F) {
        errorMessage = "El radio de la pelota debe estar entre 0.01 y 1.5.";
        return false;
    }
    if (!(simulation.gravity < 0.0F)) {
        errorMessage = "La gravedad debe ser negativa para que las pelotas caigan.";
        return false;
    }
    if (simulation.restitution < 0.0F || simulation.restitution > 1.0F) {
        errorMessage = "El coeficiente de restitucion debe estar entre 0 y 1.";
        return false;
    }
    if (config.windowWidth < MIN_WINDOW_WIDTH || config.windowHeight < MIN_WINDOW_HEIGHT) {
        errorMessage = "El lienzo minimo permitido es de " +
            std::to_string(MIN_WINDOW_WIDTH) + "x" + std::to_string(MIN_WINDOW_HEIGHT) + " pixeles.";
        return false;
    }
    if (config.windowWidth > MAX_WINDOW_SIDE || config.windowHeight > MAX_WINDOW_SIDE) {
        errorMessage = "El lienzo no puede exceder " + std::to_string(MAX_WINDOW_SIDE) + " pixeles por lado.";
        return false;
    }
    if (config.threadCount < 0 || config.threadCount > MAX_THREADS) {
        errorMessage = "La cantidad de hilos debe estar entre 0 y " + std::to_string(MAX_THREADS) + ".";
        return false;
    }
    if (config.runBenchmark) {
        if (config.benchmarkRepetitions < 10) {
            errorMessage = "El banco de pruebas exige al menos 10 repeticiones por medicion.";
            return false;
        }
        if (config.benchmarkSteps < 10) {
            errorMessage = "El banco de pruebas exige al menos 10 pasos de fisica por repeticion.";
            return false;
        }
        for (const int value : config.benchmarkBallCounts) {
            if (value < MIN_BALLS || value > MAX_BALLS) {
                errorMessage = "Un valor de '--bench-balls' esta fuera del rango permitido.";
                return false;
            }
        }
        for (const int value : config.benchmarkThreadCounts) {
            if (value < 1 || value > MAX_THREADS) {
                errorMessage = "Un valor de '--bench-threads' esta fuera del rango permitido.";
                return false;
            }
        }
    }
    return true;
}

void promptForMissingValues(AppConfig& config, bool ballCountProvided) {
    if (!config.allowPrompt || ballCountProvided) {
        return;
    }
    // Sin terminal interactiva (por ejemplo dentro de CTest o de un script) se
    // conservan los valores por omision en lugar de bloquear la ejecucion.
    if (ISATTY(FILENO(stdin)) == 0) {
        return;
    }

    std::cout << "No se indico la cantidad de pelotas con '-n'.\n"
              << "Ingrese N [" << config.simulation.ballCount << "]: " << std::flush;

    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\nEntrada no disponible; se usara N = "
                  << config.simulation.ballCount << ".\n";
        return;
    }

    // Se reintenta hasta tres veces antes de rendirse y usar el valor por omision.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            std::cout << "Se usara el valor por omision N = "
                      << config.simulation.ballCount << ".\n";
            return;
        }
        const auto last = line.find_last_not_of(" \t\r\n");
        const std::string trimmed = line.substr(first, last - first + 1);

        int parsed = 0;
        if (parseInteger(trimmed, parsed) && parsed >= MIN_BALLS && parsed <= MAX_BALLS) {
            config.simulation.ballCount = parsed;
            return;
        }

        std::cout << "Valor invalido. Ingrese un entero entre " << MIN_BALLS
                  << " y " << MAX_BALLS << " (Enter para usar "
                  << config.simulation.ballCount << "): " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            return;
        }
    }
    std::cout << "Demasiados intentos invalidos; se usara N = "
              << config.simulation.ballCount << ".\n";
}
