#pragma once

#include "Simulation.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Renderer: dibujo de la escena con OpenGL en modo compatibilidad.
//
// El renderizado ocurre siempre en el hilo principal y solo despues de que la
// fisica termino: el contexto de OpenGL no es seguro para multiples hilos y,
// ademas, leer las posiciones mientras se escriben produciria parpadeos.
//
// Las pelotas y las clavijas se dibujan como "billboards" texturizados (un
// cuadrilatero por elemento con una textura de esfera preiluminada) en lugar de
// mallas de esfera. Con N grande esa decision es la diferencia entre 6 y 60
// cuadros por segundo, y deja el costo del cuadro dominado por la fisica, que
// es lo que el proyecto quiere medir.
// ---------------------------------------------------------------------------

// Orientacion de la camara en orbita alrededor de la piramide.
struct CameraState {
    float yawDegrees   = 0.0F;   // Giro alrededor del eje vertical.
    float pitchDegrees = 16.0F;  // Inclinacion sobre la horizontal.
};

// Datos que el HUD muestra sobre la escena.
struct HudInfo {
    double framesPerSecond = 0.0;   // Cuadros por segundo medidos en la ultima ventana.
    double physicsMilliseconds = 0.0; // Tiempo promedio de fisica por cuadro.
    double frameMilliseconds = 0.0; // Tiempo promedio total por cuadro.
    double speedupEstimate = 0.0;   // Aceleracion estimada contra el modo secuencial.
    double loadImbalance = 1.0;     // Desbalance de carga del ultimo paso paralelo.
    int threads = 1;                // Hilos activos.
    int ballCount = 0;              // N vigente.
    long long recycled = 0;         // Pelotas recicladas desde el inicio.
    std::string modeName = "";      // Nombre del modo de ejecucion.
    std::string notice = "";        // Mensaje temporal (por ejemplo, un error).
};

// Prepara el estado de OpenGL y genera las texturas procedurales.
// Entradas: "width" y "height" tamano del lienzo en pixeles.
void initializeRenderer(int width, int height);

// Libera las texturas creadas por initializeRenderer.
void shutdownRenderer();

// Ajusta el viewport y la proyeccion tras un cambio de tamano de ventana.
void resizeRenderer(int width, int height);

// Dibuja un cuadro completo: piramide, clavijas, modificadores, pelotas y HUD.
// Entradas: "simulation" estado de solo lectura; "hud" cifras a desplegar;
//           "camera" orientacion de la camara en orbita.
void renderScene(const Simulation& simulation, const HudInfo& hud, const CameraState& camera);

// Dibuja una cadena de texto sobre el lienzo, en coordenadas de pixel medidas
// desde la esquina superior izquierda.
// Entradas: "text" cadena; "x"/"y" posicion; "scale" tamano del pixel del
//           tipo de letra; "r"/"g"/"b"/"a" color.
void drawText(const std::string& text, float x, float y, float scale,
              float r, float g, float b, float a);
