# Plinko 3D Paralelo

Screensaver escrito en C++17 que simula una **pirámide de Plinko en 3D**: miles
de pelotas llueven sobre un cono de clavijas oscilantes, chocan entre sí, se
reparten hacia afuera rebotando de nivel en nivel, atraviesan zonas que alteran
su física y terminan contadas en sectores angulares alrededor de la base. La
cámara orbita alrededor de la pirámide. Arranca a pantalla completa, se dibuja
con SDL2 y OpenGL y muestra los FPS en pantalla.

El proyecto compara **cuatro estrategias de ejecución** sobre exactamente el
mismo trabajo: una secuencial, una con un `std::thread` por pelota y dos con
**OpenMP**. Se puede alternar entre ellas con una tecla mientras el programa
corre.

> Proyecto No. 1 — Computación Paralela y Distribuida (CC3086), Universidad del
> Valle de Guatemala, Semestre 2, 2026.
> Ian Cumes (23236) · Javier Valladares (23045) · Nery Molina (23218)

![Plinko 3D Paralelo a pantalla completa](docs/capturas/piramide_frontal.png)

Con menos pelotas se aprecia la estructura de la pirámide:

![La pirámide de clavijas](docs/capturas/piramide_estructura.png)

## Resultados

Medido en un Apple M1 Pro (6 núcleos de rendimiento + 2 de eficiencia), 12
repeticiones por configuración. Se reporta el **mejor** tiempo de las doce, no el
promedio: la interferencia del sistema operativo solo puede añadir tiempo, nunca
quitarlo. La comprobación de que el criterio es sano es que el speedup con un
solo hilo da 1.00 en las seis cargas (con el promedio se dispersaba entre 0.90 y
1.06). La sección 5.3 del informe lo desarrolla.

| N | Secuencial (ms/paso) | `std::thread` | OMP estático 8h | OMP ajustado 8h | Mejor speedup | Eficiencia |
|---|---|---|---|---|---|---|
| 250 | 0.407 | 0.28x | 1.53x | 1.08x | **2.48x** (4 h) | 0.62 |
| 500 | 1.053 | 0.25x | 2.38x | 2.36x | **3.06x** (4 h) | 0.77 |
| 1000 | 3.106 | 0.23x | 3.39x | 3.16x | **4.08x** (6 h) | 0.68 |
| 2000 | 10.301 | no viable | 3.68x | 4.15x | **4.46x** (6 h) | 0.74 |
| 4000 | 36.682 | no viable | 3.48x | 4.44x | **4.44x** (8 h) | 0.56 |
| 8000 | 136.021 | no viable | 4.26x | 4.74x | **4.74x** (8 h) | 0.59 |

Con la configuración por omisión (N = 3 000) el cambio de modo se ve de
inmediato en el HUD:

| Modo | FPS | Física |
|---|---|---|
| Secuencial | 42 (ámbar) | 22.1 ms |
| OpenMP estático, 8 hilos | 140 (verde) | 5.3 ms |
| OpenMP ajustado, 8 hilos | 153 (verde) | 4.9 ms |

Tres cosas que vale la pena señalar:

- **Un hilo por pelota es contraproducente.** Esa versión resulta unas cuatro
  veces *más lenta* que la secuencial, y por encima de 1 024 pelotas el sistema
  operativo ya no permite crearla. El costo de despertar y dormir N hilos crece
  con N mientras el trabajo por hilo se mantiene constante.
- **`schedule(guided)` no siempre gana.** Con dos hilos las dos versiones son
  indistinguibles (0.03x de diferencia). Con cargas pequeñas el reparto estático
  gana en todas las cantidades de hilos, porque el costo de que los hilos vuelvan
  a pedir tarea no se amortiza. La ventaja de la versión ajustada aparece cuando
  coinciden carga alta y seis u ocho hilos, y llega al 28 %: ahí entran al equipo
  los dos núcleos de eficiencia y el reparto estático se desbalancea. No existe
  una política de reparto que gane siempre.
- **Paralelizar solo rinde si el problema es grande.** Con N = 250 la sobrecarga
  de la región paralela se come la ganancia; a partir de N ≈ 1 000 la eficiencia
  crece de forma sostenida.

El análisis completo está en **[docs/Informe_Proyecto1.pdf](docs/Informe_Proyecto1.pdf)**.

## Requisitos

| Dependencia | Notas |
|---|---|
| CMake 3.16 o posterior | |
| Compilador con C++17 | Probado con AppleClang 17 y GCC 13 |
| SDL2 | `brew install sdl2` · `sudo apt install libsdl2-dev` |
| OpenGL | Incluido en el sistema |
| OpenMP | En macOS: `brew install libomp` |

En macOS, AppleClang no trae el runtime de OpenMP. `CMakeLists.txt` lo detecta
solo: si `find_package(OpenMP)` falla, busca la instalación de Homebrew. Si aun
así no lo encuentra, el programa **compila igual** y avisa que los modos de
OpenMP correrán en un solo hilo.

## Compilación

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
```

## Ejecución

```bash
./build/plinko3d -n 800
```

Sin argumentos, el programa pide N por consola (Enter conserva el valor por
omisión). Con `--no-prompt` usa los valores por omisión sin preguntar.

### Opciones de línea de comandos

No hay ninguna variable fija en el código que afecte la carga de trabajo o el
lienzo: todo se lee de aquí.

**Escena**

| Opción | Descripción | Rango |
|---|---|---|
| `-n`, `--balls <entero>` | Cantidad N de pelotas | 1 – 200 000 |
| `--pegs <niv>x<anillo>` | Pirámide: niveles × clavijas de la base, p. ej. `12x46` | 0–64 y 0–256 |
| `--modifiers <entero>` | Zonas modificadoras de física | 0 – 256 |
| `--bins <entero>` | Sectores angulares contadores en la base | 1 – 64 |
| `--radius <decimal>` | Radio de cada pelota | 0.01 – 1.5 |
| `--gravity <decimal>` | Gravedad en unidades/s² | debe ser negativa |
| `--restitution <decimal>` | Coeficiente de restitución | 0 – 1 |
| `--substeps <entero>` | Sub-pasos de integración por cuadro | 1 – 16 |
| `--no-interaction` | Desactiva las colisiones pelota-pelota O(N²) | |
| `--board-radius <dec>` | Radio del cilindro que contiene la escena | > 3× radio pelota |
| `--board-height <dec>` | Altura útil de la escena | > 6× radio pelota |
| `--seed <entero>` | Semilla del generador pseudoaleatorio | ≥ 0 |

**Ventana**

| Opción | Descripción | Rango |
|---|---|---|
| `-w`, `--width <entero>` | Ancho del lienzo en píxeles | ≥ 640 |
| `-h`, `--height <entero>` | Alto del lienzo en píxeles | ≥ 480 |
| `--windowed` | Arranca en ventana en lugar de pantalla completa | |
| `--no-vsync` | Desactiva la sincronía vertical | |
| `--rotation <dec>` | Grados por segundo que gira la cámara (0 la detiene) | −360 – 360 |
| `--pitch <dec>` | Inclinación de la cámara en grados | −80 – 80 |

**Ejecución**

| Opción | Descripción |
|---|---|
| `-m`, `--mode <modo>` | `seq`, `threads`, `omp-static` u `omp-tuned` |
| `-t`, `--threads <entero>` | Hilos de OpenMP (0 = todos los núcleos) |
| `--no-prompt` | No solicitar datos por consola si faltan |

**Medición**

| Opción | Descripción |
|---|---|
| `--benchmark` | Ejecuta el banco de pruebas sin abrir ventana |
| `--bench-balls <lista>` | Valores de N separados por coma |
| `--bench-threads <lista>` | Cantidades de hilos separadas por coma |
| `--bench-reps <entero>` | Repeticiones por medición (mínimo 10) |
| `--bench-steps <entero>` | Pasos de física por repetición |
| `--bench-out <ruta>` | Archivo CSV de salida |
| `--screenshot <ruta>` | Guarda una captura BMP y termina |
| `--warmup <entero>` | Cuadros simulados antes de la captura |

`--help` imprime todo lo anterior. Cualquier argumento inválido se rechaza con
un mensaje que explica el problema, seguido de la ayuda, y el programa termina
con código distinto de cero.

### Controles

| Tecla | Acción |
|---|---|
| `0` `1` `2` `3` | Secuencial / `std::thread` / OpenMP estático / OpenMP ajustado |
| `ESPACIO` | Rota entre los cuatro modos |
| `+` / `-` | Aumenta o reduce los hilos de OpenMP |
| `,` / `.` | Reduce o aumenta la velocidad de giro de la cámara |
| `F` | Alterna entre pantalla completa y ventana |
| `R` | Reinicia la escena con una semilla nueva |
| `ESC` o `Q` | Cierra la aplicación |

El HUD muestra en vivo los FPS (verde sobre 58, ámbar entre 30 y 58, rojo por
debajo), el modo activo, N, los hilos, el tiempo de física y el tiempo total por
cuadro, el speedup estimado contra el modo secuencial y el total de pelotas
recicladas.

## Los cuatro modos

Los cuatro comparten el mismo núcleo de física, la función `advanceBall()`, y se
diferencian solo en cómo recorren el arreglo de pelotas y cómo acumulan los
contadores compartidos. Así la comparación mide la estrategia de paralelización,
no dos implementaciones distintas de la física.

| Modo | Estrategia | Sincronía |
|---|---|---|
| `seq` | Ciclo simple sobre las N pelotas | ninguna |
| `threads` | Un `std::thread` persistente por pelota | Barrera de dos fases con mutex y dos `condition_variable` |
| `omp-static` | `#pragma omp parallel for schedule(static)` | `#pragma omp atomic` + barrera implícita del `omp for` |
| `omp-tuned` | Región paralela única, `schedule(guided)` | Contadores privados fusionados con `#pragma omp critical`; `#pragma omp barrier` + `single` para la métrica de desbalance |

### Por qué no hay condiciones de carrera

La simulación mantiene **dos** arreglos de pelotas: `current_` es de solo lectura
durante todo el paso y `next_` tiene un único escritor por índice. Al terminar el
paso se intercambian.

De ahí se siguen tres cosas: ningún hilo escribe una dirección que otro pueda
leer, así que no hace falta ningún candado sobre el estado de las pelotas; el
resultado no depende del orden de ejecución, así que la simulación es
**determinista** y produce los mismos bits con 1 hilo que con 16; y ese
determinismo se puede verificar automáticamente.

Además, cada pelota lleva su propia semilla pseudoaleatoria dentro de su
estructura, en lugar de compartir un generador global que habría que proteger con
un mutex y que haría el resultado dependiente del calendario de los hilos.

## Banco de pruebas

Ejecuta la misma física que el screensaver pero sin ventana ni dibujado, de modo
que el tiempo medido corresponde solo al trabajo que se paraleliza.

```bash
./build/plinko3d --benchmark --bench-balls 250,500,1000,2000,4000,8000 --bench-threads 1,2,4,6,8 --bench-reps 12
```

Escribe `docs/resultados/benchmark.csv` con las cifras agregadas y
`docs/resultados/benchmark_muestras.csv` con cada repetición individual. Usa un
paso de tiempo fijo de 1/60 s y ejecuta tres pasos de calentamiento antes de
cronometrar, para que la creación del equipo de hilos no contamine la primera
toma.

Para regenerar las gráficas y el informe a partir de esos CSV:

```bash
python3 scripts/generar_graficas.py && python3 scripts/generar_informe.py
```

## Pruebas

```bash
ctest --test-dir build --output-on-failure
```

| Suite | Qué verifica |
|---|---|
| `equivalencia_modos` | Los cuatro modos producen un estado final **idéntico bit a bit** tras 180 pasos con 220 pelotas |
| `determinismo_hilos` | El resultado no cambia con 1, 2, 3, 5, 8 o 16 hilos |
| `validacion_argumentos` | 21 casos de línea de comandos, válidos e inválidos |
| `conservacion_conteos` | La suma de las casillas coincide con el total de pelotas recicladas |

La comparación de estados es exacta, sin tolerancia, a propósito: como el diseño
garantiza que las operaciones de punto flotante se ejecutan en el mismo orden en
todos los modos, cualquier diferencia indicaría una condición de carrera real.

**Estado en la versión entregada: 4 de 4 pruebas aprobadas.**

## Estructura del proyecto

```text
include/
  Vec3.h                 Vector de tres componentes y operaciones
  Random.h               Generador SplitMix32 sin estado compartido
  Entities.h             Ball, Peg, Modifier y la oscilación de las clavijas
  SimulationConfig.h     Parámetros, modos y análisis de la línea de comandos
  Simulation.h           Estado del tablero y las cuatro estrategias de avance
  BallThreadSystem.h     Un std::thread persistente por pelota
  Renderer.h             Interfaz del renderizador y datos del HUD
  Benchmark.h            Banco de pruebas sin ventana
  Font5x7.h              Tipo de letra de mapa de bits (generado)
src/
  Simulation.cpp         advanceBall() y las cuatro versiones del paso
  BallThreadSystem.cpp   Barrera de dos fases con mutex y variables de condición
  SimulationConfig.cpp   Validación y programación defensiva
  Benchmark.cpp          Medición, estadísticas y escritura de los CSV
  Renderer.cpp           Tablero, billboards texturizados y HUD
  main.cpp               Ventana, eventos, ciclo principal y liberación
tests/
  SimulationTests.cpp    Las cuatro suites registradas en CTest
scripts/
  generate_font.py       Genera include/Font5x7.h
  generar_graficas.py    Gráficas del informe a partir de los CSV
  generar_informe.py     Informe PDF
  informe_datos.py       Catálogo de funciones y bibliografía
docs/
  Informe_Proyecto1.pdf  Informe completo con los tres anexos
  capturas/              Capturas del programa
  graficas/              Figuras y diagrama de flujo
  resultados/            CSV y bitácora del banco de pruebas
```

## Notas de implementación

- **Renderizado.** Las pelotas y las clavijas se dibujan como billboards
  texturizados (un cuadrilátero con una textura de esfera preiluminada) en lugar
  de mallas de esfera. Con N grande esa decisión es la diferencia entre 6 y 60
  FPS, y deja el costo del cuadro dominado por la física, que es lo que interesa
  medir. Las texturas se generan por código, sin archivos externos.
- **Texto en pantalla.** El HUD usa un tipo de letra de mapa de bits 5×7 incluido
  en el binario, para no depender de SDL_ttf ni de un archivo de fuente.
- **OpenGL en un solo hilo.** El contexto no es seguro para múltiples hilos, así
  que el dibujado ocurre siempre en el hilo principal y solo después de que la
  barrera liberó a todos los hilos de la física.
- **La cámara no toca la física.** El giro es puramente visual, de modo que los
  tiempos y los speedup son comparables entre corridas y no dependen del ángulo.
- **Clavijas y pelotas se distinguen por cuatro rasgos a la vez**, no solo por
  color: las clavijas son más de tres veces más grandes, usan tonos fríos poco
  saturados (ámbar las que oscilan), llevan una textura con contorno oscuro y no
  emiten halo. Las pelotas usan tonos libres muy saturados y sí lo emiten.
- **El encuadre se calcula muestreando el cilindro.** Al inclinar la cámara, el
  borde cercano de la corona de sectores queda mucho más próximo que el centro;
  ajustar la distancia con el tamaño aparente del plano central dejaba la base
  cortada fuera del cuadro.
