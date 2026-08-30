#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Configuracion de ejecucion y analisis de argumentos de linea de comandos.
//
// El proyecto exige evitar variables "hard-coded": todos los parametros que
// afectan la carga de trabajo o la ventana se leen de la linea de comandos y,
// si faltan, se solicitan de forma interactiva. Cada valor se valida antes de
// usarse (programacion defensiva).
// ---------------------------------------------------------------------------

// Estrategia de actualizacion de la fisica.
enum class ExecutionMode {
    Sequential,   // Version secuencial de referencia (un solo hilo).
    StdThreads,   // Primera version paralela: un std::thread persistente por pelota.
    OpenMpStatic, // Segunda version paralela: "omp parallel for" con reparto estatico.
    OpenMpTuned   // Tercera version paralela: region unica, reparto guiado, contadores privados.
};

// Nombre corto del modo, usado en la interfaz y en los reportes.
const char* executionModeName(ExecutionMode mode);

// Convierte una cadena de la linea de comandos en un modo.
// Entradas: "text" cadena ingresada por el usuario.
// Salidas: "mode" modo reconocido. Devuelve false si la cadena no es valida.
bool parseExecutionMode(const std::string& text, ExecutionMode& mode);

// Parametros fisicos y de escena. Se agrupan aparte porque el motor de fisica
// los necesita completos y el banco de pruebas los reutiliza sin ventana.
struct SimulationParams {
    int ballCount        = 600;    // N: cantidad de pelotas a simular y renderizar.
    int pegRows          = 9;      // Filas de clavijas del tablero.
    int pegColumns       = 14;     // Columnas de clavijas por fila.
    int modifierCount    = 5;      // K: zonas modificadoras de fisica.
    int binCount         = 15;     // Casillas contadoras en la base del tablero.
    int substeps         = 2;      // Sub-pasos de integracion por cuadro.
    float ballRadius     = 0.11F;  // Radio base de cada pelota.
    float gravity        = -9.81F; // Aceleracion de la gravedad (unidades/s^2).
    float restitution    = 0.72F;  // Coeficiente de restitucion de los rebotes.
    float boardWidth     = 17.0F;  // Ancho util del tablero.
    float boardHeight    = 10.5F;  // Alto util del tablero.
    float boardDepth     = 1.7F;   // Profundidad util del tablero.
    bool ballInteraction = true;   // Habilita las colisiones pelota-pelota O(N^2).
    std::uint32_t seed   = 20260829U; // Semilla global del generador pseudoaleatorio.

    // Coordenada del piso del tablero (derivada del alto).
    float floorY() const { return -boardHeight * 0.5F; }
    // Coordenada del techo del tablero.
    float ceilingY() const { return boardHeight * 0.5F; }
    // Semiancho del tablero.
    float halfWidth() const { return boardWidth * 0.5F; }
    // Semiprofundidad del tablero.
    float halfDepth() const { return boardDepth * 0.5F; }
};

// Configuracion completa de la aplicacion.
struct AppConfig {
    SimulationParams simulation;          // Parametros de la escena y la fisica.
    ExecutionMode mode = ExecutionMode::OpenMpTuned; // Modo inicial de actualizacion.
    int windowWidth  = 1280;              // Ancho del lienzo en pixeles (minimo 640).
    int windowHeight = 720;               // Alto del lienzo en pixeles (minimo 480).
    int threadCount  = 0;                 // 0 = usar todos los nucleos disponibles.
    bool vsync       = true;              // Sincroniza el intercambio de buffers.
    bool showHelp    = false;             // Solicito la ayuda y no debe simularse.
    bool runBenchmark = false;            // Ejecuta el banco de pruebas sin ventana.
    bool allowPrompt = true;              // Permite pedir datos por consola si faltan.
    int benchmarkRepetitions = 12;        // Repeticiones por medicion (minimo exigido: 10).
    int benchmarkSteps = 240;             // Pasos de fisica cronometrados por repeticion.
    std::vector<int> benchmarkBallCounts; // Valores de N a evaluar en el banco.
    std::vector<int> benchmarkThreadCounts; // Cantidades de hilos a evaluar.
    std::string benchmarkOutput = "docs/resultados/benchmark.csv"; // Archivo CSV de salida.
    std::string screenshotPath;           // Si no esta vacio, guarda un BMP y termina.
    int screenshotWarmupFrames = 240;     // Cuadros simulados antes de la captura.
};

// Texto de ayuda mostrado con "--help" o ante un argumento invalido.
std::string usageText(const char* programName);

// Analiza los argumentos de la linea de comandos.
// Entradas: "argc"/"argv" tal como los recibe main.
// Salidas: "config" configuracion resultante; "errorMessage" descripcion del
//          primer problema encontrado.
// Devuelve true si los argumentos son validos y coherentes.
bool parseCommandLine(int argc, char** argv, AppConfig& config, std::string& errorMessage);

// Revisa que los parametros esten dentro de rangos utiles.
// Devuelve false y llena "errorMessage" cuando algun valor es inaceptable.
bool validateConfig(const AppConfig& config, std::string& errorMessage);

// Solicita por consola los datos que no se recibieron como argumentos.
// Solo actua si "allowPrompt" es verdadero y la entrada estandar es interactiva.
// Entradas/Salidas: "config" se completa con lo ingresado por el usuario.
void promptForMissingValues(AppConfig& config, bool ballCountProvided);
