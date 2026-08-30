#!/usr/bin/env python3
"""Datos textuales del informe: catalogo de funciones y bibliografia.

Se mantienen aparte del generador para que el archivo del informe quede
legible y para poder revisar el catalogo contra el codigo fuente.
"""

# --- Anexo 2: catalogo de funciones ----------------------------------------
# Cada entrada: (archivo, firma, entradas, salidas, descripcion)
CATALOGO = [
    # ---------------------------------------------------------------- Vec3.h
    ("include/Vec3.h", "struct Vec3",
     "x, y, z : float — componentes de ancho, alto y profundidad del tablero.",
     "No aplica (tipo de dato).",
     "Vector de tres componentes definido como agregado POD para que el arreglo de pelotas "
     "quede contiguo en memoria y los ciclos puedan repartirse entre hilos sin indirecciones."),
    ("include/Vec3.h", "Vec3& operator+=(const Vec3& other)",
     "other : const Vec3& — vector que se suma.",
     "Vec3& — referencia al propio vector ya modificado.",
     "Suma componente a componente en el lugar. Se usa al integrar la posicion y al acumular impulsos."),
    ("include/Vec3.h", "Vec3 operator*(const Vec3& vector, float scalar)",
     "vector : const Vec3& — vector a escalar; scalar : float — factor.",
     "Vec3 — vector escalado.",
     "Multiplicacion por escalar. Aparece en cada paso de integracion (velocidad por delta time)."),
    ("include/Vec3.h", "float dot(const Vec3& left, const Vec3& right)",
     "left, right : const Vec3& — vectores a proyectar.",
     "float — producto punto.",
     "Producto punto. Base de las proyecciones que resuelven los rebotes: proyecta la velocidad "
     "relativa sobre la normal de contacto."),
    ("include/Vec3.h", "float lengthSquared(const Vec3& vector)",
     "vector : const Vec3& — vector medido.",
     "float — magnitud al cuadrado.",
     "Magnitud al cuadrado. Se prefiere sobre length() dentro del ciclo O(N^2) porque evita la "
     "raiz cuadrada cuando solo hay que comparar distancias."),
    ("include/Vec3.h", "float length(const Vec3& vector)",
     "vector : const Vec3& — vector medido.",
     "float — magnitud euclidiana.",
     "Magnitud del vector. Solo se invoca cuando la distancia real es necesaria."),
    ("include/Vec3.h", "Vec3 normalized(const Vec3& vector)",
     "vector : const Vec3& — vector a normalizar.",
     "Vec3 — vector unitario; si la magnitud es despreciable devuelve (0,1,0).",
     "Normalizacion con proteccion contra division entre cero: ante un vector nulo devuelve un "
     "eje estable en lugar de producir NaN (programacion defensiva)."),
    # -------------------------------------------------------------- Random.h
    ("include/Random.h", "std::uint32_t nextRandomUint(std::uint32_t& state)",
     "state : std::uint32_t& — semilla, se modifica en el lugar.",
     "std::uint32_t — entero pseudoaleatorio de 32 bits.",
     "Generador SplitMix32. Cada pelota lleva su propia semilla, de modo que la secuencia que "
     "consume no depende del numero de hilos ni de su calendarizacion y no requiere candados."),
    ("include/Random.h", "float nextRandomFloat(std::uint32_t& state)",
     "state : std::uint32_t& — semilla del generador.",
     "float — valor uniforme en [0, 1).",
     "Convierte 24 bits del generador en un flotante. Se usa para colores y decisiones de escena."),
    ("include/Random.h", "float nextRandomInRange(std::uint32_t& state, float minimum, float maximum)",
     "state : std::uint32_t& — semilla; minimum, maximum : float — extremos del intervalo.",
     "float — valor uniforme en [minimum, maximum).",
     "Escala el valor uniforme al rango pedido. Genera posiciones y velocidades de reaparicion."),
    ("include/Random.h", "std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t index)",
     "seed : std::uint32_t — semilla global; index : std::uint32_t — indice de la pelota.",
     "std::uint32_t — semilla derivada.",
     "Deriva la semilla de cada pelota a partir de la global y de su indice sin recorrer el "
     "generador secuencialmente, lo que permite inicializar en paralelo de forma reproducible."),
    # ------------------------------------------------------------ Entities.h
    ("include/Entities.h", "struct Ball",
     "position, velocity, color : Vec3; radius, mass : float; seed : std::uint32_t; "
     "bin : std::int32_t; active : bool.",
     "No aplica (tipo de dato).",
     "Pelota del tablero. Es la unidad de trabajo que se reparte entre hilos: el arreglo de "
     "Ball es el unico que crece con N."),
    ("include/Entities.h", "struct Peg",
     "basePosition, color : Vec3; radius, amplitude, angularSpeed, phase : float.",
     "No aplica (tipo de dato).",
     "Clavija del tablero. Puede oscilar de forma sinusoidal para que el recorrido de las "
     "pelotas no sea repetitivo."),
    ("include/Entities.h", "struct Modifier",
     "center, color : Vec3; radius, strength : float; kind : ModifierKind.",
     "No aplica (tipo de dato).",
     "Zona esferica que altera la fisica local: pozo gravitatorio, impulso de velocidad o "
     "turbulencia lateral dependiente del tiempo."),
    ("include/Entities.h", "Vec3 pegPositionAt(const Peg& peg, float time)",
     "peg : const Peg& — clavija consultada; time : float — reloj de simulacion en segundos.",
     "Vec3 — centro de la clavija desplazado por su oscilacion.",
     "Evalua la posicion de la clavija en el instante dado mediante una funcion seno. Es una "
     "funcion pura: cualquier hilo puede llamarla a la vez sin sincronizacion."),
    # ----------------------------------------------------- SimulationConfig.h
    ("include/SimulationConfig.h", "enum class ExecutionMode",
     "Sequential, StdThreads, OpenMpStatic, OpenMpTuned.",
     "No aplica (tipo de dato).",
     "Identifica cual de las cuatro estrategias de actualizacion se ejecuta."),
    ("include/SimulationConfig.h", "struct SimulationParams",
     "ballCount, pegRows, pegColumns, modifierCount, binCount, substeps : int; "
     "ballRadius, gravity, restitution, boardWidth, boardHeight, boardDepth : float; "
     "ballInteraction : bool; seed : std::uint32_t.",
     "floorY(), ceilingY(), halfWidth(), halfDepth() : float — limites derivados del tablero.",
     "Agrupa todos los parametros fisicos y de escena. El banco de pruebas los reutiliza sin "
     "necesidad de abrir una ventana."),
    ("include/SimulationConfig.h", "struct AppConfig",
     "simulation : SimulationParams; mode : ExecutionMode; windowWidth, windowHeight, "
     "threadCount, benchmarkRepetitions, benchmarkSteps, screenshotWarmupFrames : int; "
     "vsync, showHelp, runBenchmark, allowPrompt : bool; benchmarkBallCounts, "
     "benchmarkThreadCounts : std::vector<int>; benchmarkOutput, screenshotPath : std::string.",
     "No aplica (tipo de dato).",
     "Configuracion completa de la aplicacion tal como queda despues de leer la linea de "
     "comandos y de validarla."),
    ("src/SimulationConfig.cpp", "bool parseInteger(const std::string& text, int& value)",
     "text : const std::string& — cadena recibida en la linea de comandos.",
     "value : int& — entero convertido. Retorno bool: true si la conversion fue total.",
     "Convierte a entero exigiendo que se consuma toda la cadena y detectando desbordamiento. "
     "Rechaza entradas como '12abc' que strtol aceptaria parcialmente."),
    ("src/SimulationConfig.cpp", "bool parseFloatValue(const std::string& text, float& value)",
     "text : const std::string& — cadena recibida.",
     "value : float& — decimal convertido. Retorno bool: true si la conversion fue total.",
     "Equivalente de parseInteger para valores decimales como la gravedad o la restitucion."),
    ("src/SimulationConfig.cpp", "bool parseIntegerList(const std::string& text, std::vector<int>& values)",
     "text : const std::string& — lista separada por comas, por ejemplo '250,500,1000'.",
     "values : std::vector<int>& — enteros positivos. Retorno bool: true si todos son validos.",
     "Analiza las listas de --bench-balls y --bench-threads, recortando espacios y exigiendo "
     "que cada elemento sea un entero positivo."),
    ("include/SimulationConfig.h", "const char* executionModeName(ExecutionMode mode)",
     "mode : ExecutionMode — modo consultado.",
     "const char* — nombre corto del modo.",
     "Devuelve la etiqueta que aparece en el HUD, en el titulo de la ventana y en los CSV."),
    ("include/SimulationConfig.h", "bool parseExecutionMode(const std::string& text, ExecutionMode& mode)",
     "text : const std::string& — cadena escrita por el usuario.",
     "mode : ExecutionMode& — modo reconocido. Retorno bool: false si la cadena no corresponde.",
     "Acepta varios alias por modo (seq/secuencial, omp-tuned/tuned/omp2) y rechaza el resto."),
    ("include/SimulationConfig.h", "std::string usageText(const char* programName)",
     "programName : const char* — nombre con el que se invoco el ejecutable.",
     "std::string — texto de ayuda completo.",
     "Construye la ayuda que se imprime con --help y tambien despues de cualquier error de "
     "argumentos, para que el usuario vea de inmediato la forma correcta de invocacion."),
    ("include/SimulationConfig.h",
     "bool parseCommandLine(int argc, char** argv, AppConfig& config, std::string& errorMessage)",
     "argc : int, argv : char** — argumentos tal como los recibe main.",
     "config : AppConfig& — configuracion resultante; errorMessage : std::string& — descripcion "
     "del primer problema. Retorno bool: true si los argumentos son validos.",
     "Recorre argv reconociendo cada opcion, verifica que las que llevan valor lo reciban, "
     "convierte los tipos, invoca la solicitud interactiva si falta N y termina llamando a "
     "validateConfig. Es el punto unico de entrada de la programacion defensiva."),
    ("include/SimulationConfig.h",
     "bool validateConfig(const AppConfig& config, std::string& errorMessage)",
     "config : const AppConfig& — configuracion a revisar.",
     "errorMessage : std::string& — motivo del rechazo. Retorno bool: true si todo esta en rango.",
     "Comprueba N, la rejilla de clavijas, modificadores, casillas, sub-pasos, radio, gravedad, "
     "restitucion, tamano minimo del lienzo (640x480), cantidad de hilos y los parametros del "
     "banco de pruebas."),
    ("include/SimulationConfig.h",
     "void promptForMissingValues(AppConfig& config, bool ballCountProvided)",
     "config : AppConfig& — configuracion en construccion; ballCountProvided : bool — si el "
     "usuario ya indico N con -n.",
     "config : AppConfig& — se completa con el valor ingresado.",
     "Solicita N por consola cuando no vino por argumento. Solo actua si la entrada estandar es "
     "interactiva, admite Enter para conservar el valor por omision y reintenta hasta tres veces "
     "ante entradas invalidas."),
    # ---------------------------------------------------------- Simulation.h
    ("include/Simulation.h",
     "Ball advanceBall(std::size_t index, const Ball* balls, std::size_t ballCount, "
     "const Peg* pegs, std::size_t pegCount, const Modifier* modifiers, "
     "std::size_t modifierCount, const SimulationParams& params, float time, float dt, "
     "int& binHit)",
     "index : std::size_t — pelota a actualizar; balls, pegs, modifiers : punteros de solo "
     "lectura al estado del cuadro anterior; ballCount, pegCount, modifierCount : std::size_t; "
     "params : const SimulationParams&; time : float — reloj de simulacion; dt : float — paso.",
     "binHit : int& — casilla alcanzada, o -1 si la pelota no llego al fondo. "
     "Retorno Ball: el nuevo estado de la pelota.",
     "NUCLEO DE LA PARALELIZACION. Aplica gravedad y zonas modificadoras O(K), colisiones "
     "pelota-pelota O(N), integracion semi-implicita de Euler, colisiones contra clavijas O(M), "
     "rebotes contra paredes y techo, y reaparicion al llegar al fondo. Es una funcion pura "
     "sobre entradas de solo lectura, de modo que varios hilos pueden ejecutarla simultaneamente "
     "sin candados y el resultado no depende del orden de ejecucion."),
    ("include/Simulation.h", "Simulation::Simulation(const SimulationParams& params)",
     "params : const SimulationParams& — parametros ya validados.",
     "No aplica (constructor).",
     "Construye la escena completa invocando reset() con la semilla indicada."),
    ("include/Simulation.h", "Simulation::~Simulation()",
     "No aplica.", "No aplica (destructor).",
     "Libera el sistema de hilos persistentes, que a su vez los detiene y los une ordenadamente "
     "antes de destruir cualquier memoria que esten leyendo."),
    ("include/Simulation.h", "void Simulation::reset(std::uint32_t seed)",
     "seed : std::uint32_t — semilla global del generador.",
     "Modifica el estado interno: clavijas, modificadores, pelotas y contadores.",
     "Regenera la escena completa de forma reproducible. Tambien libera los hilos persistentes, "
     "porque su cantidad depende de N."),
    ("include/Simulation.h", "void Simulation::resizeBallCount(int ballCount)",
     "ballCount : int — nuevo valor de N (se recorta al rango valido).",
     "Modifica el estado interno.",
     "Cambia la cantidad de pelotas conservando la semilla, para poder variar la carga sin "
     "reiniciar el programa."),
    ("include/Simulation.h",
     "void Simulation::step(float dt, ExecutionMode mode, int threadCount)",
     "dt : float — segundos a avanzar; mode : ExecutionMode — estrategia; threadCount : int — "
     "hilos de OpenMP (0 usa todos los disponibles).",
     "Modifica el estado interno de la simulacion.",
     "Despachador: encamina el paso a una de las cuatro implementaciones."),
    ("include/Simulation.h", "void Simulation::stepSequential(float dt)",
     "dt : float — segundos a avanzar.",
     "Modifica current_, next_, binCounts_ y recycledBalls_.",
     "Version secuencial de referencia. Recorre las N pelotas en un solo hilo y acumula las "
     "casillas en el mismo ciclo. Es el denominador de todos los speedup reportados."),
    ("include/Simulation.h", "void Simulation::stepStdThreads(float dt)",
     "dt : float — segundos a avanzar.",
     "Modifica el estado interno; usa binHits_ como area de escritura por indice.",
     "Primera version paralela: delega cada pelota a un std::thread persistente y espera en una "
     "barrera de dos fases. Si el sistema operativo no permite crear los hilos, degrada de forma "
     "controlada a la version secuencial en lugar de abortar."),
    ("include/Simulation.h",
     "void Simulation::stepOpenMpStatic(float dt, int threadCount)",
     "dt : float — segundos a avanzar; threadCount : int — hilos solicitados.",
     "Modifica el estado interno.",
     "Segunda version paralela: '#pragma omp parallel for schedule(static)' sobre el arreglo de "
     "pelotas, con '#pragma omp atomic' sobre los contadores de casilla y 'reduction(+)' sobre "
     "el total de recicladas. La barrera implicita al cerrar el omp for es la sincronia que "
     "necesita el sub-paso siguiente."),
    ("include/Simulation.h",
     "void Simulation::stepOpenMpTuned(float dt, int threadCount)",
     "dt : float — segundos a avanzar; threadCount : int — hilos solicitados.",
     "Modifica el estado interno y actualiza lastLoadImbalance_.",
     "Tercera version paralela: abre una sola region paralela para todos los sub-pasos, reparte "
     "con 'schedule(guided)', acumula las casillas en contadores privados por hilo que se "
     "fusionan con '#pragma omp critical', y calcula el desbalance de carga con "
     "'#pragma omp barrier' seguido de '#pragma omp single'. La alternancia de buffers se "
     "resuelve por la paridad del sub-paso, porque dentro de la region no se puede intercambiar "
     "los vectores."),
    ("src/Simulation.cpp", "void Simulation::buildPegs(std::uint32_t& seedState)",
     "seedState : std::uint32_t& — generador de la escena.",
     "Llena pegs_.",
     "Construye la rejilla triangular de clavijas: las filas impares se desplazan medio paso, "
     "como en un tablero Plinko clasico. Alrededor del 35 % de las clavijas recibe amplitud de "
     "oscilacion y se pinta en violeta; el resto queda fija y se pinta en cian."),
    ("src/Simulation.cpp", "void Simulation::buildModifiers(std::uint32_t& seedState)",
     "seedState : std::uint32_t& — generador de la escena.",
     "Llena modifiers_.",
     "Coloca K zonas de influencia con centro, radio, tipo e intensidad pseudoaleatorios."),
    ("src/Simulation.cpp",
     "void Simulation::spawnBall(Ball& ball, std::uint32_t index, std::uint32_t& seedState) const",
     "ball : Ball& — pelota a inicializar; index : std::uint32_t — su indice; "
     "seedState : std::uint32_t& — generador de la escena.",
     "ball : Ball& — queda con semilla, radio, masa, color, posicion y velocidad.",
     "Inicializa una pelota derivando su semilla propia con mixSeed y repartiendola por toda la "
     "altura del tablero para que la escena se vea poblada desde el primer cuadro."),
    ("src/Simulation.cpp", "void Simulation::ensureStdThreads()",
     "No aplica.",
     "Crea o reemplaza stdThreads_; en caso de fallo llena stdThreadsError_.",
     "Crea el sistema de un hilo por pelota solo cuando hace falta y captura la excepcion si el "
     "sistema operativo rechaza la creacion, marcando el modo como no disponible."),
    ("src/Simulation.cpp", "Vec3 hsvToRgb(float hue, float saturation, float value)",
     "hue : float en [0,1); saturation, value : float en [0,1].",
     "Vec3 — componentes rojo, verde y azul en [0,1].",
     "Convierte HSV a RGB. Sortear solo el tono, con saturacion y valor altos fijos, produce "
     "colores vistosos y evita los tonos apagados que daria sortear las tres componentes RGB "
     "por separado."),
    ("src/Simulation.cpp", "void respawnAtTop(Ball& ball, const SimulationParams& params)",
     "ball : Ball& — pelota reciclada; params : const SimulationParams& — limites del tablero.",
     "ball : Ball& — con posicion, velocidad y color nuevos.",
     "Recoloca la pelota en la parte alta del tablero. Consume unicamente el generador propio de "
     "la pelota, por lo que puede ejecutarse dentro de un ciclo paralelo sin sincronizacion."),
    ("src/Simulation.cpp", "void clampSpeed(Vec3& velocity)",
     "velocity : Vec3& — velocidad a limitar.",
     "velocity : Vec3& — con magnitud recortada al maximo permitido.",
     "Limita la rapidez sin alterar la direccion, para que un choque multiple no dispare una "
     "pelota fuera del tablero."),
    # ---------------------------------------------------- BallThreadSystem.h
    ("include/BallThreadSystem.h", "BallThreadSystem::BallThreadSystem(std::size_t ballCount)",
     "ballCount : std::size_t — cantidad de hilos a crear (uno por pelota).",
     "No aplica (constructor). Lanza std::system_error si el sistema operativo los rechaza.",
     "Crea los hilos persistentes una sola vez. Si falla a mitad de camino, detiene y une los "
     "que ya existen antes de propagar la excepcion, de modo que no quedan hilos huerfanos."),
    ("include/BallThreadSystem.h", "BallThreadSystem::~BallThreadSystem()",
     "No aplica.", "No aplica (destructor).",
     "Levanta la bandera de parada bajo el mutex, despierta a todos los workers y los une. "
     "Ningun hilo sobrevive al objeto."),
    ("include/BallThreadSystem.h",
     "void BallThreadSystem::runRound(const Ball* readBalls, Ball* writeBalls, "
     "std::size_t ballCount, const Peg* pegs, std::size_t pegCount, const Modifier* modifiers, "
     "std::size_t modifierCount, const SimulationParams& params, float time, float dt, "
     "int* binHits)",
     "readBalls, pegs, modifiers : punteros de solo lectura al cuadro anterior; ballCount, "
     "pegCount, modifierCount : std::size_t; params : const SimulationParams&; time, dt : float.",
     "writeBalls : Ball* — estado nuevo; binHits : int* — casilla alcanzada por cada pelota.",
     "Ejecuta una ronda completa. Primera fase de la barrera: publica los datos, incrementa el "
     "numero de generacion y despierta a todos los workers. Segunda fase: espera a que el "
     "contador de pendientes llegue a cero, lo que garantiza que nadie lea writeBalls mientras "
     "otro hilo todavia lo escribe."),
    ("src/BallThreadSystem.cpp", "void BallThreadSystem::workerLoop(std::size_t ballIndex)",
     "ballIndex : std::size_t — indice fijo de la pelota que atiende este hilo.",
     "Escribe writeBalls[ballIndex] y binHits[ballIndex].",
     "Ciclo de vida de cada worker: espera su turno comparando el numero de generacion (lo que "
     "descarta los despertares espurios), copia los datos de la ronda, suelta el mutex, calcula "
     "su pelota fuera de la seccion critica y decrementa el contador de pendientes."),
    # ------------------------------------------------------------ Renderer.h
    ("include/Renderer.h", "struct HudInfo",
     "framesPerSecond, physicsMilliseconds, frameMilliseconds, speedupEstimate, "
     "loadImbalance : double; threads, ballCount : int; recycled : long long; "
     "modeName, notice : std::string.",
     "No aplica (tipo de dato).",
     "Cifras que el HUD despliega sobre la escena."),
    ("include/Renderer.h", "void initializeRenderer(int width, int height)",
     "width, height : int — tamano del lienzo en pixeles.",
     "Modifica el estado de OpenGL y crea dos texturas.",
     "Prepara profundidad, mezcla y prueba de alfa, y genera por codigo las texturas de esfera "
     "iluminada y de halo. No lee ningun archivo externo, de modo que el proyecto se entrega "
     "solo como codigo fuente."),
    ("include/Renderer.h", "void shutdownRenderer()",
     "No aplica.", "Libera las texturas de OpenGL.",
     "Destruccion explicita de los recursos graficos antes de eliminar el contexto."),
    ("include/Renderer.h", "void resizeRenderer(int width, int height)",
     "width, height : int — nuevo tamano del lienzo.",
     "Modifica el viewport y la matriz de proyeccion.",
     "Recalcula la perspectiva cuando el usuario cambia el tamano de la ventana."),
    ("include/Renderer.h",
     "void renderScene(const Simulation& simulation, const HudInfo& hud)",
     "simulation : const Simulation& — estado de solo lectura; hud : const HudInfo& — cifras.",
     "Dibuja el cuadro completo en el framebuffer activo.",
     "Dibuja el panel de fondo, el marco tridimensional, las casillas con su histograma, las "
     "zonas modificadoras, las clavijas, el halo aditivo, las pelotas y el HUD. Se invoca solo "
     "despues de que la fisica termino y la barrera libero a todos los hilos."),
    ("include/Renderer.h",
     "void drawText(const std::string& text, float x, float y, float scale, "
     "float r, float g, float b, float a)",
     "text : const std::string& — cadena; x, y : float — posicion en pixeles desde la esquina "
     "superior izquierda; scale : float — tamano del pixel del tipo de letra; r, g, b, a : float "
     "— color.",
     "Dibuja cuadrilateros en el framebuffer.",
     "Dibuja texto con el tipo de letra de mapa de bits 5x7 incluido en el programa. Todos los "
     "glifos de una llamada se emiten dentro de un solo par glBegin/glEnd."),
    ("src/Renderer.cpp", "GLuint createSphereTexture()",
     "No aplica.", "GLuint — identificador de la textura creada.",
     "Genera por codigo una esfera preiluminada: reconstruye la normal a partir de la posicion "
     "dentro del circulo y evalua un termino difuso mas un realce especular. Multiplicada por el "
     "color de la pelota, produce una esfera creible con un solo cuadrilatero."),
    ("src/Renderer.cpp", "float cameraDistanceFor(const SimulationParams& params, float aspect)",
     "params : const SimulationParams& — dimensiones del tablero; aspect : float — relacion de "
     "aspecto del lienzo.",
     "float — distancia de la camara sobre el eje Z.",
     "Calcula a que distancia colocar la camara para que el tablero completo quepa sin importar "
     "la relacion de aspecto de la ventana."),
    ("src/Renderer.cpp", "void emitBillboard(float x, float y, float z, float radius)",
     "x, y, z : float — centro; radius : float — semilado del cuadrilatero.",
     "Emite cuatro vertices texturizados.",
     "Dibuja un cuadrilatero orientado a la camara. Como la camara no rota, un cuadrilatero en "
     "el plano XY siempre queda de frente y no hace falta reconstruir la base de la vista."),
    # ----------------------------------------------------------- Benchmark.h
    ("include/Benchmark.h", "struct BenchmarkRecord",
     "ballCount, threads, steps, repetitions : int; mode : ExecutionMode; averageStepMs, "
     "minimumStepMs, maximumStepMs, stdDevStepMs, speedupAverage, speedupBest, efficiency, "
     "maxFps : double; available : bool; note : std::string; samples : std::vector<double>.",
     "No aplica (tipo de dato).",
     "Resultado agregado de una combinacion de N, modo y cantidad de hilos, junto con las "
     "muestras individuales de cada repeticion."),
    ("include/Benchmark.h", "int runBenchmark(const AppConfig& config)",
     "config : const AppConfig& — configuracion validada con runBenchmark activo.",
     "Escribe benchmark.csv y benchmark_muestras.csv; imprime la tabla por consola. "
     "Retorno int: 0 si todo fue bien.",
     "Recorre todas las combinaciones de N, modo y hilos; para cada una ejecuta pasos de "
     "calentamiento y luego las repeticiones cronometradas, calcula las estadisticas y los "
     "speedup respecto de la version secuencial con el mismo N, y guarda los resultados."),
    ("src/Benchmark.cpp",
     "BenchmarkRecord measure(const SimulationParams& params, ExecutionMode mode, int threads, "
     "int repetitions, int steps)",
     "params : const SimulationParams&; mode : ExecutionMode; threads, repetitions, steps : int.",
     "BenchmarkRecord — estadisticas y muestras individuales.",
     "Mide una combinacion concreta. Antes de cronometrar ejecuta tres pasos de calentamiento "
     "para llenar las caches y para que OpenMP cree su equipo de hilos, de modo que ese costo no "
     "contamine la primera toma. Usa un paso de tiempo fijo de 1/60 s para eliminar el ruido del "
     "reloj real."),
    ("src/Benchmark.cpp", "int stepsForBallCount(int ballCount, int requested, int substeps)",
     "ballCount : int — N evaluado; requested : int — tope indicado por el usuario; "
     "substeps : int — sub-pasos por cuadro.",
     "int — pasos a cronometrar, siempre en el rango [4, requested].",
     "Reduce la cantidad de pasos de forma inversamente proporcional a N^2, que es la "
     "complejidad del nucleo, para que N grande no dispare el tiempo total del banco."),
    ("src/Benchmark.cpp", "void ensureParentDirectory(const std::string& path)",
     "path : const std::string& — ruta del archivo de salida.",
     "Crea los directorios intermedios que falten.",
     "Programacion defensiva: evita que el banco termine sin poder guardar el CSV despues de "
     "varios minutos de medicion."),
    # ------------------------------------------------------------ Font5x7.h
    ("include/Font5x7.h", "const std::uint8_t* glyphFor(char character)",
     "character : char — caracter buscado.",
     "const std::uint8_t* — cinco bytes, uno por columna del glifo.",
     "Devuelve el glifo del tipo de letra 5x7, convirtiendo minusculas a mayusculas y "
     "sustituyendo por espacio cualquier caracter fuera de la tabla."),
    # -------------------------------------------------------------- main.cpp
    ("src/main.cpp", "int main(int argc, char** argv)",
     "argc : int, argv : char** — argumentos de la linea de comandos.",
     "int — 0 si la ejecucion fue correcta, 1 ante cualquier error.",
     "Punto de entrada. Captura y valida argumentos, decide entre banco de pruebas y "
     "screensaver, crea la ventana y el contexto, ejecuta el ciclo principal y libera todos los "
     "recursos en orden inverso a su creacion."),
    ("src/main.cpp", "void processEvents(RuntimeState& state)",
     "state : RuntimeState& — estado del ciclo principal.",
     "state : RuntimeState& — modificado segun las teclas pulsadas.",
     "Procesa la cola de SDL: cierre de ventana, cambio de tamano, seleccion de modo con 0/1/2/3, "
     "rotacion con ESPACIO, ajuste de hilos con + y -, reinicio con R y salida con ESC."),
    ("src/main.cpp", "ExecutionMode nextMode(ExecutionMode mode)",
     "mode : ExecutionMode — modo actual.",
     "ExecutionMode — siguiente modo del ciclo.",
     "Rota entre las cuatro estrategias, para poder compararlas en vivo sin reiniciar."),
    ("src/main.cpp", "bool saveScreenshot(const std::string& path, int width, int height)",
     "path : const std::string& — ruta destino; width, height : int — tamano del lienzo.",
     "Escribe un archivo BMP. Retorno bool: true si se guardo correctamente.",
     "Lee el framebuffer con glReadPixels e invierte las filas, porque OpenGL las entrega de "
     "abajo hacia arriba. Se usa para documentar el proyecto de forma reproducible."),
]

# --- Bibliografia -----------------------------------------------------------
BIBLIOGRAFIA = [
    "OpenMP Architecture Review Board. (2021). <i>OpenMP Application Programming Interface, "
    "Version 5.2</i>. OpenMP ARB. Recuperado de https://www.openmp.org/specifications/",

    "Chapman, B., Jost, G., y van der Pas, R. (2007). <i>Using OpenMP: Portable Shared Memory "
    "Parallel Programming</i>. Cambridge, MA: The MIT Press.",

    "Pacheco, P. S., y Malensek, M. (2022). <i>An Introduction to Parallel Programming</i> "
    "(2a ed.). Cambridge, MA: Morgan Kaufmann.",

    "Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale "
    "computing capabilities. En <i>Proceedings of the AFIPS Spring Joint Computer Conference</i> "
    "(pp. 483-485). Nueva York: ACM.",

    "Gustafson, J. L. (1988). Reevaluating Amdahl's Law. <i>Communications of the ACM, 31</i>(5), "
    "532-533.",

    "Foster, I. (1995). <i>Designing and Building Parallel Programs: Concepts and Tools for "
    "Parallel Software Engineering</i>. Reading, MA: Addison-Wesley.",

    "SDL Community. (2024). <i>SDL2 Wiki: API Reference</i>. Simple DirectMedia Layer. "
    "Recuperado de https://wiki.libsdl.org/SDL2/",

    "Apple Inc. (2021). <i>Apple M1 Pro and M1 Max: Technical Overview</i>. Apple Newsroom. "
    "Recuperado de https://www.apple.com/newsroom/",
]
