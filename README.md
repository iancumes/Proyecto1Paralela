# PLINKO 3D PARALELO

Simulación de pelotas en un tablero Plinko tridimensional, preparada para
comparar la actualización física secuencial con implementaciones paralelas.

> Estado: avance parcial. El tablero todavía no resuelve colisiones contra las
> clavijas ni calcula casillas/puntajes.

## Segundo avance: un hilo por pelota

Se agregó una primera comparación ejecutable entre dos modos de actualización:

- **Secuencial:** el hilo principal actualiza todas las pelotas una por una.
- **Paralelo:** `BallThreadSystem` mantiene un `std::thread` persistente por
  pelota. Cada worker escribe únicamente la pelota que tiene asignada.

Los hilos no se crean de nuevo en cada frame. El hilo principal publica el
`delta time`, despierta los workers y espera a que todos terminen antes de
renderizar. De esta manera, OpenGL continúa utilizándose solo desde el hilo
principal y el render nunca lee posiciones mientras están cambiando.

### Controles y medición preliminar

- `1`: modo secuencial.
- `2`: modo paralelo.
- `ESPACIO`: alternar entre ambos modos.
- `ESC`: cerrar.

El título de la ventana muestra los FPS, el promedio de microsegundos empleado
por la física y la cantidad de hilos del modo actual. Con solo 12 pelotas es
esperable que la coordinación de 12 hilos cueste más que el cálculo físico;
esta medición servirá como línea base para el siguiente avance.

### Prueba de equivalencia

`tests/BallThreadSystemTests.cpp` ejecuta 600 pasos sobre copias idénticas de
las pelotas y comprueba que los modos secuencial y paralelo producen el mismo
estado. También verifica que se cree exactamente un hilo por pelota.

```bash
ctest --test-dir build --output-on-failure
```

### Pendiente para próximos avances

- Detectar y resolver rebotes contra las clavijas del tablero.
- Reaparecer pelotas para lograr el comportamiento continuo de screensaver.
- Permitir cargas de prueba distintas a las 12 pelotas iniciales.
- Registrar mediciones repetibles y calcular speedup/eficiencia.
- Comparar este diseño con una cantidad acotada de workers u OpenMP.

## Primer avance

### API gráfica

SDL2 crea la ventana y procesa eventos; OpenGL renderiza la escena 3D con double buffering.

### Elementos almacenados en memoria

La estructura `Ball` está declarada en `include/Ball.h`. La colección principal `std::vector<Ball> balls`, ubicada en `src/main.cpp`, almacena las 12 pelotas iniciales. La cantidad se controla con `INITIAL_BALL_COUNT`.

### Actualización

`updateBall(...)` y `updateBallsSerial(...)` están en `src/Ball.cpp`. Aplican gravedad, calculan la siguiente posición usando delta time y resuelven rebotes simples contra el piso y los límites laterales.

### Renderizado

`renderBall(...)` y `renderBalls(...)` están en `src/Renderer.cpp`. Cada pelota se dibuja como una esfera sencilla en su posición 3D almacenada.

### Ejecución

Requisitos: CMake 3.16 o posterior, compilador compatible con C++17, SDL2 y OpenGL.

```bash
cmake -S . -B build
cmake --build build
./build/plinko3d
```

Cierra la ventana con el botón del sistema o con la tecla `ESC`.
