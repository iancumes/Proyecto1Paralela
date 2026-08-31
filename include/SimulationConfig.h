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
    int ballCount        = 3000;   // N: cantidad de pelotas a simular y renderizar.
    int pegLevels        = 12;     // Niveles de la piramide de clavijas.
    int pegsPerBaseRing  = 46;     // Clavijas del anillo mas ancho (la base).
    int modifierCount    = 8;      // K: zonas modificadoras de fisica.
    int binCount         = 24;     // Sectores contadores alrededor del eje.
    int substeps         = 2;      // Sub-pasos de integracion por cuadro.
    float ballRadius     = 0.085F;  // Radio base de cada pelota.
    float gravity        = -9.81F; // Aceleracion de la gravedad (unidades/s^2).
    float restitution    = 0.68F;  // Coeficiente de restitucion de los rebotes.
    float boardRadius    = 13.5F;  // Radio del cilindro que contiene la escena.
    float boardHeight    = 10.5F;   // Altura util de la escena.
    bool ballInteraction = true;   // Habilita las colisiones pelota-pelota O(N^2).
    std::uint32_t seed   = 20260829U; // Semilla global del generador pseudoaleatorio.

    // Coordenada del piso, donde se cuentan las pelotas.
    float floorY() const { return -boardHeight * 0.5F; }
    // Coordenada del techo, por donde reaparecen.
    float ceilingY() const { return boardHeight * 0.5F; }
    // Radio maximo que puede alcanzar el centro de una pelota.
    float usableRadius(float ballRadiusValue) const { return boardRadius - ballRadiusValue; }
    // Radio del anillo mas ancho de la piramide.
    float pyramidRadius() const { return boardRadius * 0.62F; }
};

// Configuracion completa de la aplicacion.
struct AppConfig {
    SimulationParams simulation;          // Parametros de la escena y la fisica.
    ExecutionMode mode = ExecutionMode::OpenMpTuned; // Modo inicial de actualizacion.
    int windowWidth  = 1280;              // Ancho del lienzo en pixeles (minimo 640).
    int windowHeight = 720;               // Alto del lienzo en pixeles (minimo 480).
    int threadCount  = 0;                 // 0 = usar todos los nucleos disponibles.
    bool vsync       = true;              // Sincroniza el intercambio de buffers.
    bool fullscreen  = true;              // Abre a pantalla completa; --windowed lo desactiva.
    float rotationSpeed = 11.0F;          // Grados por segundo que gira la camara.
    float cameraPitch   = 9.0F;          // Inclinacion de la camara sobre la horizontal.
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
