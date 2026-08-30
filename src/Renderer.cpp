#include "Renderer.h"

#include "Font5x7.h"

#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr float PI = 3.14159265358979323846F;
constexpr int SPRITE_TEXTURE_SIZE = 64;   // Lado de la textura procedural.
constexpr float FIELD_OF_VIEW = 45.0F;    // Campo de vision vertical en grados.
constexpr float FIT_MARGIN = 1.10F;       // Holgura alrededor del tablero.

GLuint g_sphereTexture = 0;  // Textura de esfera preiluminada (pelotas y clavijas).
GLuint g_glowTexture = 0;    // Halo aditivo que da el aspecto de neon.
int g_viewportWidth = 1280;  // Ancho actual del lienzo.
int g_viewportHeight = 720;  // Alto actual del lienzo.

// Construye una perspectiva sin depender de GLU, que no siempre esta presente.
// Entradas: campo de vision vertical en grados, relacion de aspecto y planos.
void setPerspective(float fieldOfViewDegrees, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(fieldOfViewDegrees * PI / 360.0F);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

// Genera la textura de una esfera iluminada desde arriba a la izquierda.
// El canal alfa define el recorte circular; los canales RGB llevan el termino
// difuso mas un realce especular. Al multiplicarla por el color de la pelota se
// obtiene una esfera creible con un solo cuadrilatero.
GLuint createSphereTexture() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(SPRITE_TEXTURE_SIZE) * SPRITE_TEXTURE_SIZE * 4, 0);

    const float half = SPRITE_TEXTURE_SIZE * 0.5F;
    for (int y = 0; y < SPRITE_TEXTURE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_TEXTURE_SIZE; ++x) {
            const float nx = (static_cast<float>(x) + 0.5F - half) / half;
            const float ny = (static_cast<float>(y) + 0.5F - half) / half;
            const float radiusSquared = nx * nx + ny * ny;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * SPRITE_TEXTURE_SIZE + static_cast<std::size_t>(x)) * 4;

            if (radiusSquared > 1.0F) {
                pixels[offset + 3] = 0;  // Fuera del circulo: totalmente transparente.
                continue;
            }

            // Normal de la esfera reconstruida a partir de la posicion.
            const float nz = std::sqrt(std::max(0.0F, 1.0F - radiusSquared));
            const float lightX = -0.45F;
            const float lightY = -0.55F;
            const float lightZ = 0.70F;
            const float diffuse = std::max(0.0F, nx * lightX + ny * lightY + nz * lightZ);
            // Realce especular estrecho.
            const float specular = std::pow(diffuse, 22.0F);
            const float intensity = std::min(1.0F, 0.24F + 0.86F * diffuse);

            const float red = intensity + specular * 0.85F;
            const float green = intensity + specular * 0.85F;
            const float blue = intensity + specular * 0.85F;

            // Suavizado del borde para evitar el aliasing del circulo.
            const float edge = std::min(1.0F, (1.0F - std::sqrt(radiusSquared)) * 12.0F);

            pixels[offset + 0] = static_cast<std::uint8_t>(std::min(255.0F, red * 255.0F));
            pixels[offset + 1] = static_cast<std::uint8_t>(std::min(255.0F, green * 255.0F));
            pixels[offset + 2] = static_cast<std::uint8_t>(std::min(255.0F, blue * 255.0F));
            pixels[offset + 3] = static_cast<std::uint8_t>(std::max(0.0F, edge) * 255.0F);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Genera un halo radial suave que se dibuja en modo aditivo alrededor de cada
// pelota. Es lo que produce la estetica de neon del screensaver.
GLuint createGlowTexture() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(SPRITE_TEXTURE_SIZE) * SPRITE_TEXTURE_SIZE * 4, 0);

    const float half = SPRITE_TEXTURE_SIZE * 0.5F;
    for (int y = 0; y < SPRITE_TEXTURE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_TEXTURE_SIZE; ++x) {
            const float nx = (static_cast<float>(x) + 0.5F - half) / half;
            const float ny = (static_cast<float>(y) + 0.5F - half) / half;
            const float distance = std::sqrt(nx * nx + ny * ny);
            const float falloff = std::max(0.0F, 1.0F - distance);
            const float alpha = falloff * falloff * falloff;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * SPRITE_TEXTURE_SIZE + static_cast<std::size_t>(x)) * 4;
            pixels[offset + 0] = 255;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 255;
            pixels[offset + 3] = static_cast<std::uint8_t>(alpha * 255.0F);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Emite los cuatro vertices de un billboard centrado en (x, y, z).
// La camara no rota, asi que un cuadrilatero en el plano XY siempre queda de
// frente y no hace falta reconstruir la base de la vista.
inline void emitBillboard(float x, float y, float z, float radius) {
    glTexCoord2f(0.0F, 0.0F); glVertex3f(x - radius, y + radius, z);
    glTexCoord2f(1.0F, 0.0F); glVertex3f(x + radius, y + radius, z);
    glTexCoord2f(1.0F, 1.0F); glVertex3f(x + radius, y - radius, z);
    glTexCoord2f(0.0F, 1.0F); glVertex3f(x - radius, y - radius, z);
}

// Calcula a que distancia debe colocarse la camara para que el tablero completo
// quepa en el lienzo, sin importar la relacion de aspecto de la ventana.
// Entradas: dimensiones del tablero y relacion de aspecto actual.
// Salida: distancia en unidades de mundo sobre el eje Z.
float cameraDistanceFor(const SimulationParams& params, float aspect) {
    const float halfFov = FIELD_OF_VIEW * PI / 360.0F;
    const float tangent = std::tan(halfFov);
    // Distancia minima para que entre la altura y para que entre el ancho.
    const float forHeight = (params.boardHeight * 0.5F * FIT_MARGIN) / tangent;
    const float forWidth = (params.boardWidth * 0.5F * FIT_MARGIN) / (tangent * std::max(aspect, 0.1F));
    return std::max(forHeight, forWidth) + params.halfDepth() + 1.0F;
}

// Dibuja el panel de fondo con un degradado vertical y el marco del tablero.
void renderBoard(const SimulationParams& params) {
    const float left = -params.halfWidth();
    const float right = params.halfWidth();
    const float bottom = params.floorY();
    const float top = params.ceilingY();
    const float back = -params.halfDepth() - 0.35F;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor4f(0.03F, 0.05F, 0.14F, 1.0F);
    glVertex3f(left, top, back);
    glVertex3f(right, top, back);
    glColor4f(0.07F, 0.03F, 0.16F, 1.0F);
    glVertex3f(right, bottom, back);
    glVertex3f(left, bottom, back);
    glEnd();

    // Rejilla tenue sobre el panel: aporta sensacion de profundidad sin
    // competir visualmente con las pelotas.
    glColor4f(0.16F, 0.24F, 0.45F, 0.35F);
    glBegin(GL_LINES);
    constexpr int GRID_LINES = 14;
    for (int line = 1; line < GRID_LINES; ++line) {
        const float t = static_cast<float>(line) / GRID_LINES;
        glVertex3f(left + (right - left) * t, bottom, back + 0.005F);
        glVertex3f(left + (right - left) * t, top, back + 0.005F);
    }
    for (int line = 1; line < GRID_LINES / 2; ++line) {
        const float t = static_cast<float>(line) / (GRID_LINES / 2);
        glVertex3f(left, bottom + (top - bottom) * t, back + 0.005F);
        glVertex3f(right, bottom + (top - bottom) * t, back + 0.005F);
    }
    glEnd();

    // Marco trasero y marco delantero: entre ambos queda encerrado el volumen
    // por el que se mueven las pelotas, de modo que ninguna parece escaparse.
    const float front = params.halfDepth() + 0.15F;
    glColor4f(0.20F, 0.50F, 0.85F, 0.55F);
    glBegin(GL_LINE_LOOP);
    glVertex3f(left, bottom, back + 0.01F);
    glVertex3f(right, bottom, back + 0.01F);
    glVertex3f(right, top, back + 0.01F);
    glVertex3f(left, top, back + 0.01F);
    glEnd();

    glColor4f(0.35F, 0.80F, 1.0F, 0.90F);
    glBegin(GL_LINE_LOOP);
    glVertex3f(left, bottom, front);
    glVertex3f(right, bottom, front);
    glVertex3f(right, top, front);
    glVertex3f(left, top, front);
    glEnd();

    // Aristas que unen ambos marcos: dan volumen a la caja del tablero.
    glColor4f(0.22F, 0.45F, 0.75F, 0.45F);
    glBegin(GL_LINES);
    glVertex3f(left, bottom, back + 0.01F);  glVertex3f(left, bottom, front);
    glVertex3f(right, bottom, back + 0.01F); glVertex3f(right, bottom, front);
    glVertex3f(right, top, back + 0.01F);    glVertex3f(right, top, front);
    glVertex3f(left, top, back + 0.01F);     glVertex3f(left, top, front);
    glEnd();
}

// Dibuja las divisiones de las casillas y un histograma con el conteo de cada
// una. El histograma es la salida de resultados de la simulacion.
void renderBins(const SimulationParams& params, const std::vector<long long>& counts) {
    if (counts.empty()) {
        return;
    }
    const float left = -params.halfWidth();
    const float binWidth = params.boardWidth / static_cast<float>(counts.size());
    const float bottom = params.floorY();
    const float back = -params.halfDepth() - 0.3F;
    const long long maximum = std::max(1LL, *std::max_element(counts.begin(), counts.end()));
    const float maximumHeight = params.boardHeight * 0.13F;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (std::size_t index = 0; index < counts.size(); ++index) {
        const float x0 = left + binWidth * static_cast<float>(index) + binWidth * 0.12F;
        const float x1 = left + binWidth * static_cast<float>(index + 1) - binWidth * 0.12F;
        const float fraction = static_cast<float>(counts[index]) / static_cast<float>(maximum);
        const float height = maximumHeight * fraction;
        // El color pasa de azul (poco) a magenta (mucho).
        glColor4f(0.25F + 0.70F * fraction, 0.30F - 0.18F * fraction, 0.95F - 0.25F * fraction, 0.80F);
        glVertex3f(x0, bottom, back + 0.02F);
        glVertex3f(x1, bottom, back + 0.02F);
        glVertex3f(x1, bottom + height, back + 0.02F);
        glVertex3f(x0, bottom + height, back + 0.02F);
    }
    glEnd();

    // Separadores entre casillas.
    glColor4f(0.35F, 0.55F, 0.85F, 0.55F);
    glBegin(GL_LINES);
    for (std::size_t index = 0; index <= counts.size(); ++index) {
        const float x = left + binWidth * static_cast<float>(index);
        glVertex3f(x, bottom, back + 0.03F);
        glVertex3f(x, bottom + maximumHeight * 1.05F, back + 0.03F);
    }
    glEnd();
}

// Dibuja las zonas modificadoras como anillos translucidos.
void renderModifiers(const std::vector<Modifier>& modifiers, float time) {
    if (modifiers.empty()) {
        return;
    }
    glDisable(GL_TEXTURE_2D);
    constexpr int SEGMENTS = 28;
    for (const Modifier& modifier : modifiers) {
        // La pulsacion sinusoidal deja claro cual zona esta activa.
        const float pulse = 0.85F + 0.15F * std::sin(time * 2.4F + modifier.center.x);
        glColor4f(modifier.color.x, modifier.color.y, modifier.color.z, 0.30F);
        glBegin(GL_LINE_LOOP);
        for (int segment = 0; segment < SEGMENTS; ++segment) {
            const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
            glVertex3f(modifier.center.x + std::cos(angle) * modifier.radius * pulse,
                       modifier.center.y + std::sin(angle) * modifier.radius * pulse,
                       modifier.center.z);
        }
        glEnd();
    }
}

}  // namespace

void initializeRenderer(int width, int height) {
    g_sphereTexture = createSphereTexture();
    g_glowTexture = createGlowTexture();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // La prueba de alfa descarta el exterior del circulo de cada sprite, de
    // modo que el buffer de profundidad sigue ordenando correctamente.
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.35F);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glClearColor(0.010F, 0.014F, 0.045F, 1.0F);
    resizeRenderer(width, height);
}

void shutdownRenderer() {
    // Liberacion explicita de los recursos de OpenGL antes de destruir el
    // contexto: es parte del manejo ordenado de objetos que pide el proyecto.
    if (g_sphereTexture != 0) {
        glDeleteTextures(1, &g_sphereTexture);
        g_sphereTexture = 0;
    }
    if (g_glowTexture != 0) {
        glDeleteTextures(1, &g_glowTexture);
        g_glowTexture = 0;
    }
}

void resizeRenderer(int width, int height) {
    g_viewportWidth = std::max(width, 1);
    g_viewportHeight = std::max(height, 1);
    glViewport(0, 0, g_viewportWidth, g_viewportHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(FIELD_OF_VIEW,
                   static_cast<float>(g_viewportWidth) / static_cast<float>(g_viewportHeight),
                   0.1F, 200.0F);
    glMatrixMode(GL_MODELVIEW);
}

void drawText(const std::string& text, float x, float y, float scale,
              float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    float cursorX = x;
    for (const char character : text) {
        if (character == ' ') {
            cursorX += (font5x7::GLYPH_WIDTH + 1) * scale;
            continue;
        }
        const std::uint8_t* glyph = font5x7::glyphFor(character);
        for (int column = 0; column < font5x7::GLYPH_WIDTH; ++column) {
            const std::uint8_t bits = glyph[column];
            for (int row = 0; row < font5x7::GLYPH_HEIGHT; ++row) {
                if ((bits & (1u << row)) == 0u) {
                    continue;
                }
                const float px = cursorX + static_cast<float>(column) * scale;
                const float py = y + static_cast<float>(row) * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
        cursorX += (font5x7::GLYPH_WIDTH + 1) * scale;
    }
    glEnd();
}

void renderScene(const Simulation& simulation, const HudInfo& hud) {
    const SimulationParams& params = simulation.params();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const float aspect =
        static_cast<float>(g_viewportWidth) / static_cast<float>(g_viewportHeight);
    glTranslatef(0.0F, 0.0F, -cameraDistanceFor(params, aspect));

    renderBoard(params);
    renderBins(params, simulation.binCounts());
    renderModifiers(simulation.modifiers(), simulation.time());

    // --- Clavijas ----------------------------------------------------------
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_sphereTexture);
    glBegin(GL_QUADS);
    for (const Peg& peg : simulation.pegs()) {
        const Vec3 center = pegPositionAt(peg, simulation.time());
        glColor4f(peg.color.x, peg.color.y, peg.color.z, 1.0F);
        emitBillboard(center.x, center.y, center.z, peg.radius);
    }
    glEnd();

    // --- Halo aditivo de las pelotas ---------------------------------------
    const std::vector<Ball>& balls = simulation.balls();
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, g_glowTexture);
    glBegin(GL_QUADS);
    for (const Ball& ball : balls) {
        if (!ball.active) {
            continue;
        }
        glColor4f(ball.color.x, ball.color.y, ball.color.z, 0.42F);
        emitBillboard(ball.position.x, ball.position.y, ball.position.z, ball.radius * 2.6F);
    }
    glEnd();

    // --- Pelotas -----------------------------------------------------------
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, g_sphereTexture);
    glBegin(GL_QUADS);
    for (const Ball& ball : balls) {
        if (!ball.active) {
            continue;
        }
        glColor4f(ball.color.x, ball.color.y, ball.color.z, 1.0F);
        emitBillboard(ball.position.x, ball.position.y, ball.position.z, ball.radius);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // --- HUD en proyeccion ortografica -------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(g_viewportWidth),
            static_cast<double>(g_viewportHeight), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);

    const float scale = std::max(2.0F, static_cast<float>(g_viewportWidth) / 640.0F);
    const float lineHeight = (font5x7::GLYPH_HEIGHT + 3) * scale;
    const float marginX = 12.0F * scale * 0.5F;
    float cursorY = 10.0F * scale * 0.5F;

    // Panel semitransparente detras del texto para que el HUD sea legible
    // aunque una pelota clara pase justo por debajo. El ancho se calcula a
    // partir del avance del tipo de letra, no con un numero fijo.
    const float glyphAdvance = (font5x7::GLYPH_WIDTH + 1) * scale;
    const float panelWidth = marginX * 2.0F + glyphAdvance * 20.5F;
    const float panelHeight = lineHeight * 7.6F + marginX;
    glColor4f(0.02F, 0.03F, 0.09F, 0.72F);
    glBegin(GL_QUADS);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F + panelHeight);
    glVertex2f(4.0F, 4.0F + panelHeight);
    glEnd();
    glColor4f(0.25F, 0.55F, 0.90F, 0.55F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F + panelHeight);
    glVertex2f(4.0F, 4.0F + panelHeight);
    glEnd();

    // Los FPS se muestran en grande y cambian de color segun la fluidez: verde
    // sobre 60, ambar entre 30 y 60, rojo por debajo.
    const bool smooth = hud.framesPerSecond >= 58.0;
    const bool acceptable = hud.framesPerSecond >= 30.0;
    const float fpsRed = smooth ? 0.35F : (acceptable ? 1.0F : 1.0F);
    const float fpsGreen = smooth ? 1.0F : (acceptable ? 0.75F : 0.30F);
    const float fpsBlue = smooth ? 0.55F : (acceptable ? 0.20F : 0.30F);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "FPS %5.1f", hud.framesPerSecond);
    drawText(buffer, marginX, cursorY, scale * 1.6F, fpsRed, fpsGreen, fpsBlue, 1.0F);
    cursorY += lineHeight * 1.7F;

    std::snprintf(buffer, sizeof(buffer), "MODO %s", hud.modeName.c_str());
    drawText(buffer, marginX, cursorY, scale, 0.85F, 0.92F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "N %d  HILOS %d", hud.ballCount, hud.threads);
    drawText(buffer, marginX, cursorY, scale, 0.85F, 0.92F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "FISICA %6.2f MS", hud.physicsMilliseconds);
    drawText(buffer, marginX, cursorY, scale, 0.75F, 0.85F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "CUADRO %6.2f MS", hud.frameMilliseconds);
    drawText(buffer, marginX, cursorY, scale, 0.75F, 0.85F, 1.0F, 1.0F);
    cursorY += lineHeight;

    if (hud.speedupEstimate > 0.0) {
        std::snprintf(buffer, sizeof(buffer), "SPEEDUP %4.2fX", hud.speedupEstimate);
    } else {
        std::snprintf(buffer, sizeof(buffer), "SPEEDUP  --");
    }
    drawText(buffer, marginX, cursorY, scale, 1.0F, 0.85F, 0.45F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "RECICLADAS %lld", hud.recycled);
    drawText(buffer, marginX, cursorY, scale, 0.65F, 0.78F, 0.95F, 1.0F);

    // Ayuda de teclado en la esquina inferior izquierda.
    drawText("0-3 MODO   ESPACIO ROTA   +/- HILOS   R REINICIA   ESC SALE",
             marginX, static_cast<float>(g_viewportHeight) - lineHeight - 6.0F,
             scale * 0.75F, 0.55F, 0.68F, 0.85F, 0.95F);

    // Aviso temporal (por ejemplo, cuando un modo no esta disponible).
    if (!hud.notice.empty()) {
        drawText(hud.notice, marginX,
                 static_cast<float>(g_viewportHeight) - lineHeight * 2.2F - 6.0F,
                 scale * 0.85F, 1.0F, 0.55F, 0.45F, 1.0F);
    }

    glEnable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
