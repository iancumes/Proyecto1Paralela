#!/usr/bin/env python3
"""Genera las graficas del informe a partir de los CSV del banco de pruebas.

Entrada : docs/resultados/benchmark.csv y docs/resultados/benchmark_muestras.csv
Salida  : archivos PNG en docs/graficas/

Convenciones de color (superficie clara, pensadas para el PDF impreso):
  * Series categoricas (modos de ejecucion) -> paleta categorica en orden fijo.
  * Magnitud continua (eficiencia en el mapa de calor) -> rampa de un solo tono.
  * El texto nunca lleva el color de la serie: se queda en gris oscuro.
"""
import csv
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.lines import Line2D

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTADOS = os.path.join(BASE, "docs", "resultados")
SALIDA = os.path.join(BASE, "docs", "graficas")
os.makedirs(SALIDA, exist_ok=True)

# --- Paleta -----------------------------------------------------------------
# Slots categoricos en orden fijo (validados para superficie clara).
C_AZUL, C_NARANJA, C_AQUA, C_AMARILLO = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"
TEXTO_1, TEXTO_2, TEXTO_3 = "#0b0b0b", "#52514e", "#8a8880"
REJILLA = "#e3e2de"
SUPERFICIE = "#ffffff"
# Rampa secuencial de un solo tono para magnitud continua.
RAMPA_AZUL = LinearSegmentedColormap.from_list(
    "azul", ["#eaf2fd", "#9ec5f4", "#3987e5", "#256abf", "#0d366b"])

COLOR_MODO = {
    "secuencial":    C_AZUL,
    "std::thread":   C_NARANJA,
    "openmp-static": C_AQUA,
    "openmp-tuned":  C_AMARILLO,
}
ETIQUETA_MODO = {
    "secuencial":    "Secuencial",
    "std::thread":   "std::thread (1 por pelota)",
    "openmp-static": "OpenMP estatico",
    "openmp-tuned":  "OpenMP ajustado",
}

plt.rcParams.update({
    "figure.facecolor": SUPERFICIE,
    "axes.facecolor": SUPERFICIE,
    "savefig.facecolor": SUPERFICIE,
    "font.family": "DejaVu Sans",
    "font.size": 9,
    "axes.edgecolor": REJILLA,
    "axes.labelcolor": TEXTO_2,
    "axes.titlecolor": TEXTO_1,
    "xtick.color": TEXTO_2,
    "ytick.color": TEXTO_2,
    "grid.color": REJILLA,
    "grid.linewidth": 0.8,
    "legend.frameon": False,
    "axes.spines.top": False,
    "axes.spines.right": False,
})


def limpiar(ax, ejes_y=True):
    """Deja la rejilla discreta y elimina el marco superior y derecho."""
    ax.grid(True, axis="y" if ejes_y else "both", alpha=0.9, zorder=0)
    ax.set_axisbelow(True)
    for spine in ("left", "bottom"):
        ax.spines[spine].set_color(REJILLA)


# --- Lectura de datos -------------------------------------------------------
def leer_resumen():
    with open(os.path.join(RESULTADOS, "benchmark.csv")) as archivo:
        filas = []
        for fila in csv.DictReader(archivo):
            filas.append({
                "n": int(fila["n_balls"]),
                "modo": fila["mode"],
                "hilos": int(fila["threads"]),
                "avg": float(fila["avg_ms"]),
                "min": float(fila["min_ms"]),
                "max": float(fila["max_ms"]),
                "sd": float(fila["stddev_ms"]),
                "speedup": float(fila["speedup_avg"]),
                "speedup_best": float(fila["speedup_best"]),
                "eficiencia": float(fila["efficiency"]),
                "fps": float(fila["max_fps"]),
                "ok": fila["available"] == "1",
            })
        return filas


def leer_muestras():
    datos = defaultdict(list)
    with open(os.path.join(RESULTADOS, "benchmark_muestras.csv")) as archivo:
        for fila in csv.DictReader(archivo):
            clave = (int(fila["n_balls"]), fila["mode"], int(fila["threads"]))
            datos[clave].append(float(fila["ms_per_step"]))
    return datos


RESUMEN = leer_resumen()
MUESTRAS = leer_muestras()
VALORES_N = sorted({f["n"] for f in RESUMEN})
HILOS_OMP = sorted({f["hilos"] for f in RESUMEN if f["modo"].startswith("openmp")})


def serie(modo, n):
    """Devuelve (hilos, speedup, eficiencia) ordenados para un modo y un N."""
    filas = sorted((f for f in RESUMEN if f["modo"] == modo and f["n"] == n and f["ok"]),
                   key=lambda f: f["hilos"])
    return ([f["hilos"] for f in filas],
            [f["speedup"] for f in filas],
            [f["eficiencia"] for f in filas])


# ---------------------------------------------------------------------------
# Figura 1: speedup contra numero de hilos, un panel por N (multiples pequenos)
# ---------------------------------------------------------------------------
def figura_speedup():
    fig, ejes = plt.subplots(2, 3, figsize=(10.2, 6.0), sharex=True, sharey=True)
    for indice, n in enumerate(VALORES_N):
        ax = ejes[indice // 3][indice % 3]
        limpiar(ax)
        # Referencia de speedup ideal (lineal).
        ax.plot(HILOS_OMP, HILOS_OMP, color=TEXTO_3, linewidth=1.2,
                linestyle=(0, (4, 3)), zorder=1)
        for modo in ("openmp-static", "openmp-tuned"):
            hilos, speedup, _ = serie(modo, n)
            ax.plot(hilos, speedup, color=COLOR_MODO[modo], linewidth=2.0,
                    marker="o", markersize=5.5, zorder=3,
                    markeredgecolor=SUPERFICIE, markeredgewidth=1.2)
        ax.set_title(f"N = {n:,}".replace(",", " "), fontsize=10, pad=6)
        ax.set_xticks(HILOS_OMP)
        ax.set_ylim(0, 8.6)
        if indice % 3 == 0:
            ax.set_ylabel("Speedup", color=TEXTO_2)
        if indice // 3 == 1:
            ax.set_xlabel("Hilos", color=TEXTO_2)

    # Etiqueta directa dentro del ultimo panel: identidad sin depender del color.
    ax = ejes[1][2]
    hilos, sp_est, _ = serie("openmp-static", VALORES_N[-1])
    hilos_a, sp_aju, _ = serie("openmp-tuned", VALORES_N[-1])
    ax.annotate("ajustado", (hilos_a[-1], sp_aju[-1]), xytext=(-6, 10),
                textcoords="offset points", color=TEXTO_1, fontsize=8.5, ha="right")
    ax.annotate("estatico", (hilos[-1], sp_est[-1]), xytext=(-6, -14),
                textcoords="offset points", color=TEXTO_1, fontsize=8.5, ha="right")

    leyenda = [
        Line2D([], [], color=COLOR_MODO["openmp-static"], linewidth=2.0, marker="o",
               markersize=5.5, label="OpenMP estatico  (schedule static + atomic)"),
        Line2D([], [], color=COLOR_MODO["openmp-tuned"], linewidth=2.0, marker="o",
               markersize=5.5, label="OpenMP ajustado  (region unica + guided + critical)"),
        Line2D([], [], color=TEXTO_3, linewidth=1.2, linestyle=(0, (4, 3)),
               label="Speedup ideal (lineal)"),
    ]
    fig.legend(handles=leyenda, loc="lower center", ncol=3, bbox_to_anchor=(0.5, -0.015),
               fontsize=8.5, labelcolor=TEXTO_2)
    fig.suptitle("Speedup contra la version secuencial, por cantidad de pelotas",
                 fontsize=12, color=TEXTO_1, y=0.985)
    fig.tight_layout(rect=(0, 0.05, 1, 0.96))
    ruta = os.path.join(SALIDA, "fig1_speedup.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 2: eficiencia contra numero de hilos, un panel por N
# ---------------------------------------------------------------------------
def figura_eficiencia():
    fig, ejes = plt.subplots(2, 3, figsize=(10.2, 6.0), sharex=True, sharey=True)
    for indice, n in enumerate(VALORES_N):
        ax = ejes[indice // 3][indice % 3]
        limpiar(ax)
        ax.axhline(1.0, color=TEXTO_3, linewidth=1.2, linestyle=(0, (4, 3)), zorder=1)
        for modo in ("openmp-static", "openmp-tuned"):
            hilos, _, eficiencia = serie(modo, n)
            ax.plot(hilos, eficiencia, color=COLOR_MODO[modo], linewidth=2.0,
                    marker="o", markersize=5.5, zorder=3,
                    markeredgecolor=SUPERFICIE, markeredgewidth=1.2)
        ax.set_title(f"N = {n:,}".replace(",", " "), fontsize=10, pad=6)
        ax.set_xticks(HILOS_OMP)
        ax.set_ylim(0, 1.15)
        if indice % 3 == 0:
            ax.set_ylabel("Eficiencia", color=TEXTO_2)
        if indice // 3 == 1:
            ax.set_xlabel("Hilos", color=TEXTO_2)

    ax = ejes[1][2]
    _, _, ef_aju = serie("openmp-tuned", VALORES_N[-1])
    hilos_a, _, _ = serie("openmp-tuned", VALORES_N[-1])
    ax.annotate("ajustado", (hilos_a[-1], ef_aju[-1]), xytext=(-6, 9),
                textcoords="offset points", color=TEXTO_1, fontsize=8.5, ha="right")
    hilos_e, _, ef_est = serie("openmp-static", VALORES_N[-1])
    ax.annotate("estatico", (hilos_e[-1], ef_est[-1]), xytext=(-6, -14),
                textcoords="offset points", color=TEXTO_1, fontsize=8.5, ha="right")

    leyenda = [
        Line2D([], [], color=COLOR_MODO["openmp-static"], linewidth=2.0, marker="o",
               markersize=5.5, label="OpenMP estatico"),
        Line2D([], [], color=COLOR_MODO["openmp-tuned"], linewidth=2.0, marker="o",
               markersize=5.5, label="OpenMP ajustado"),
        Line2D([], [], color=TEXTO_3, linewidth=1.2, linestyle=(0, (4, 3)),
               label="Eficiencia ideal = 1.0"),
    ]
    fig.legend(handles=leyenda, loc="lower center", ncol=3, bbox_to_anchor=(0.5, -0.015),
               fontsize=8.5, labelcolor=TEXTO_2)
    fig.suptitle("Eficiencia (speedup / hilos), por cantidad de pelotas",
                 fontsize=12, color=TEXTO_1, y=0.985)
    fig.tight_layout(rect=(0, 0.05, 1, 0.96))
    ruta = os.path.join(SALIDA, "fig2_eficiencia.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 3: tiempo por paso contra N, en escala logaritmica, por modo
# ---------------------------------------------------------------------------
def figura_escalamiento():
    fig, ax = plt.subplots(figsize=(8.4, 5.0))
    limpiar(ax, ejes_y=False)

    series = [
        ("secuencial", 1, "Secuencial"),
        ("std::thread", None, "std::thread (1 por pelota)"),
        ("openmp-static", 8, "OpenMP estatico, 8 hilos"),
        ("openmp-tuned", 8, "OpenMP ajustado, 8 hilos"),
    ]
    for modo, hilos, etiqueta in series:
        puntos = [f for f in RESUMEN
                  if f["modo"] == modo and f["ok"] and (hilos is None or f["hilos"] == hilos)]
        puntos.sort(key=lambda f: f["n"])
        xs = [f["n"] for f in puntos]
        ys = [f["avg"] for f in puntos]
        ax.plot(xs, ys, color=COLOR_MODO[modo], linewidth=2.0, marker="o",
                markersize=6, markeredgecolor=SUPERFICIE, markeredgewidth=1.2, zorder=3)
        # Etiqueta directa: con cuatro series la identidad no depende del color.
        desplazamiento = {"secuencial": (8, 2), "std::thread": (8, -2),
                          "openmp-static": (8, 2), "openmp-tuned": (8, -2)}[modo]
        ax.annotate(etiqueta, (xs[-1], ys[-1]), xytext=desplazamiento,
                    textcoords="offset points", color=TEXTO_1, fontsize=8.5, va="center")

    # Presupuesto de un cuadro a 60 FPS.
    ax.axhline(1000 / 60, color=TEXTO_3, linewidth=1.2, linestyle=(0, (4, 3)), zorder=1)
    ax.annotate("presupuesto de 60 FPS (16.7 ms)", (VALORES_N[0], 1000 / 60),
                xytext=(0, 6), textcoords="offset points", color=TEXTO_2, fontsize=8)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xticks(VALORES_N)
    ax.set_xticklabels([f"{n:,}".replace(",", " ") for n in VALORES_N])
    ax.set_xlabel("Cantidad de pelotas N (escala logaritmica)", color=TEXTO_2)
    ax.set_ylabel("Tiempo por paso de fisica [ms]", color=TEXTO_2)
    ax.set_title("Escalamiento del costo por paso: pendiente 2 en log-log = coste O(N²)",
                 fontsize=11.5, color=TEXTO_1, pad=10)
    ax.set_xlim(200, 22000)
    ax.grid(True, which="both", alpha=0.6)
    fig.tight_layout()
    ruta = os.path.join(SALIDA, "fig3_escalamiento.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 4: comparacion directa entre las dos versiones de OpenMP con N = 8000
# ---------------------------------------------------------------------------
def figura_comparacion(n=8000):
    fig, ax = plt.subplots(figsize=(8.0, 4.6))
    limpiar(ax)

    ancho = 0.38
    posiciones = range(len(HILOS_OMP))
    for desplazamiento, modo in ((-ancho / 2, "openmp-static"), (ancho / 2, "openmp-tuned")):
        valores = []
        for hilos in HILOS_OMP:
            fila = next((f for f in RESUMEN if f["modo"] == modo and f["n"] == n
                         and f["hilos"] == hilos), None)
            valores.append(fila["speedup"] if fila else 0.0)
        barras = ax.bar([p + desplazamiento for p in posiciones], valores, ancho * 0.94,
                        color=COLOR_MODO[modo], zorder=3, edgecolor=SUPERFICIE, linewidth=1.5)
        for barra, valor in zip(barras, valores):
            ax.annotate(f"{valor:.2f}×", (barra.get_x() + barra.get_width() / 2, valor),
                        xytext=(0, 3), textcoords="offset points", ha="center",
                        color=TEXTO_1, fontsize=8.5)

    ax.set_xticks(list(posiciones))
    ax.set_xticklabels([str(h) for h in HILOS_OMP])
    ax.set_xlabel("Hilos", color=TEXTO_2)
    ax.set_ylabel("Speedup", color=TEXTO_2)
    ax.set_ylim(0, 7.0)
    ax.set_title(f"Estatico contra ajustado con N = {n:,}".replace(",", " "),
                 fontsize=11.5, color=TEXTO_1, pad=10)
    leyenda = [
        Line2D([], [], color=COLOR_MODO["openmp-static"], linewidth=8,
               label="OpenMP estatico"),
        Line2D([], [], color=COLOR_MODO["openmp-tuned"], linewidth=8,
               label="OpenMP ajustado"),
    ]
    ax.legend(handles=leyenda, loc="upper left", fontsize=9, labelcolor=TEXTO_2)
    fig.tight_layout()
    ruta = os.path.join(SALIDA, "fig4_static_vs_tuned.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 5: dispersion de las 12 repeticiones (captura de las mediciones)
# ---------------------------------------------------------------------------
def figura_dispersion(n=4000):
    configuraciones = [
        ("secuencial", 1, "Secuencial"),
        ("openmp-static", 2, "Estatico\n2 hilos"),
        ("openmp-static", 4, "Estatico\n4 hilos"),
        ("openmp-static", 8, "Estatico\n8 hilos"),
        ("openmp-tuned", 2, "Ajustado\n2 hilos"),
        ("openmp-tuned", 4, "Ajustado\n4 hilos"),
        ("openmp-tuned", 8, "Ajustado\n8 hilos"),
    ]
    fig, ax = plt.subplots(figsize=(8.8, 4.8))
    limpiar(ax)

    for indice, (modo, hilos, etiqueta) in enumerate(configuraciones):
        valores = MUESTRAS.get((n, modo, hilos), [])
        if not valores:
            continue
        # Cada repeticion como un punto; la barra horizontal marca el promedio.
        xs = [indice + (i - len(valores) / 2) * 0.035 for i in range(len(valores))]
        ax.scatter(xs, valores, s=26, color=COLOR_MODO[modo], alpha=0.85, zorder=3,
                   edgecolor=SUPERFICIE, linewidth=0.8)
        promedio = sum(valores) / len(valores)
        ax.plot([indice - 0.26, indice + 0.26], [promedio, promedio],
                color=TEXTO_1, linewidth=2.0, zorder=4)
        ax.annotate(f"{promedio:.1f}", (indice, promedio), xytext=(0, 9),
                    textcoords="offset points", ha="center", color=TEXTO_1, fontsize=8.5)

    ax.set_xticks(range(len(configuraciones)))
    ax.set_xticklabels([c[2] for c in configuraciones], fontsize=8.5)
    ax.set_ylabel("Tiempo por paso [ms]", color=TEXTO_2)
    ax.set_title(f"Las 12 repeticiones de cada medicion con N = {n:,}".replace(",", " ")
                 + "  (la barra negra es el promedio)",
                 fontsize=11.5, color=TEXTO_1, pad=10)
    fig.tight_layout()
    ruta = os.path.join(SALIDA, "fig5_dispersion.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 6: cuadros por segundo que sostiene la fisica, por modo
# ---------------------------------------------------------------------------
def figura_fps():
    fig, ax = plt.subplots(figsize=(8.4, 4.8))
    limpiar(ax, ejes_y=False)

    series = [
        ("secuencial", 1, "Secuencial"),
        ("std::thread", None, "std::thread"),
        ("openmp-static", 8, "OpenMP estatico, 8 hilos"),
        ("openmp-tuned", 8, "OpenMP ajustado, 8 hilos"),
    ]
    for modo, hilos, etiqueta in series:
        puntos = [f for f in RESUMEN
                  if f["modo"] == modo and f["ok"] and (hilos is None or f["hilos"] == hilos)]
        puntos.sort(key=lambda f: f["n"])
        xs = [f["n"] for f in puntos]
        ys = [f["fps"] for f in puntos]
        ax.plot(xs, ys, color=COLOR_MODO[modo], linewidth=2.0, marker="o", markersize=6,
                markeredgecolor=SUPERFICIE, markeredgewidth=1.2, zorder=3)
        desplazamiento = {"secuencial": (8, 0), "std::thread": (10, 12),
                          "openmp-static": (8, 0), "openmp-tuned": (8, 0)}[modo]
        ax.annotate(etiqueta, (xs[-1], ys[-1]), xytext=desplazamiento,
                    textcoords="offset points", color=TEXTO_1, fontsize=8.5, va="center")

    ax.axhline(60, color=TEXTO_3, linewidth=1.4, linestyle=(0, (4, 3)), zorder=1)
    ax.annotate("60 FPS", (VALORES_N[0], 60), xytext=(0, 6),
                textcoords="offset points", color=TEXTO_2, fontsize=8.5)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xticks(VALORES_N)
    ax.set_xticklabels([f"{n:,}".replace(",", " ") for n in VALORES_N])
    ax.set_xlabel("Cantidad de pelotas N (escala logaritmica)", color=TEXTO_2)
    ax.set_ylabel("Cuadros por segundo que permite la fisica", color=TEXTO_2)
    ax.set_title("Cuadros por segundo sostenibles segun la carga",
                 fontsize=11.5, color=TEXTO_1, pad=10)
    ax.set_xlim(200, 26000)
    ax.grid(True, which="both", alpha=0.6)
    fig.tight_layout()
    ruta = os.path.join(SALIDA, "fig6_fps.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


# ---------------------------------------------------------------------------
# Figura 7: mapa de calor de la eficiencia (magnitud continua -> rampa de un tono)
# ---------------------------------------------------------------------------
def figura_mapa_eficiencia(modo="openmp-tuned"):
    matriz = []
    for n in VALORES_N:
        fila = []
        for hilos in HILOS_OMP:
            registro = next((f for f in RESUMEN if f["modo"] == modo and f["n"] == n
                             and f["hilos"] == hilos), None)
            fila.append(registro["eficiencia"] if registro else float("nan"))
        matriz.append(fila)

    fig, ax = plt.subplots(figsize=(7.2, 4.4))
    imagen = ax.imshow(matriz, cmap=RAMPA_AZUL, vmin=0, vmax=1.05, aspect="auto")
    ax.set_xticks(range(len(HILOS_OMP)))
    ax.set_xticklabels([str(h) for h in HILOS_OMP])
    ax.set_yticks(range(len(VALORES_N)))
    ax.set_yticklabels([f"{n:,}".replace(",", " ") for n in VALORES_N])
    ax.set_xlabel("Hilos", color=TEXTO_2)
    ax.set_ylabel("Pelotas N", color=TEXTO_2)
    ax.set_title("Eficiencia de la version OpenMP ajustada", fontsize=11.5,
                 color=TEXTO_1, pad=10)

    # Valor escrito en cada celda: el color codifica, el numero confirma.
    for f, n in enumerate(VALORES_N):
        for c, _ in enumerate(HILOS_OMP):
            valor = matriz[f][c]
            if valor != valor:
                continue
            ax.text(c, f, f"{valor:.2f}", ha="center", va="center", fontsize=8.5,
                    color="#ffffff" if valor > 0.62 else TEXTO_1)

    ax.set_xticks([x - 0.5 for x in range(1, len(HILOS_OMP))], minor=True)
    ax.set_yticks([y - 0.5 for y in range(1, len(VALORES_N))], minor=True)
    ax.grid(which="minor", color=SUPERFICIE, linewidth=2)
    ax.tick_params(which="minor", length=0)
    for spine in ax.spines.values():
        spine.set_visible(False)

    barra = fig.colorbar(imagen, ax=ax, fraction=0.045, pad=0.03)
    barra.set_label("Eficiencia", color=TEXTO_2)
    barra.outline.set_visible(False)
    fig.tight_layout()
    ruta = os.path.join(SALIDA, "fig7_mapa_eficiencia.png")
    fig.savefig(ruta, dpi=190, bbox_inches="tight")
    plt.close(fig)
    return ruta


if __name__ == "__main__":
    for generador in (figura_speedup, figura_eficiencia, figura_escalamiento,
                      figura_comparacion, figura_dispersion, figura_fps,
                      figura_mapa_eficiencia):
        print("generada:", os.path.relpath(generador(), BASE))
