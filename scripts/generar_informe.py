#!/usr/bin/env python3
"""Genera docs/Informe_Proyecto1.pdf con formato de informe UVG.

Se usa reportlab porque el equipo no cuenta con una instalacion de LaTeX y se
necesita control fino sobre caratula, indice, numeracion de paginas, figuras,
tablas y anexos.
"""
import csv
import os
import sys

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.platypus import (BaseDocTemplate, Frame, Image, KeepTogether,
                                NextPageTemplate, PageBreak, PageTemplate,
                                Paragraph, Spacer, Table, TableStyle)
from reportlab.platypus.tableofcontents import TableOfContents

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from informe_datos import BIBLIOGRAFIA, CATALOGO  # noqa: E402

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRAFICAS = os.path.join(BASE, "docs", "graficas")
CAPTURAS = os.path.join(BASE, "docs", "capturas")
RESULTADOS = os.path.join(BASE, "docs", "resultados")
SALIDA = os.path.join(BASE, "docs", "Informe_Proyecto1.pdf")

TINTA = colors.HexColor("#0b0b0b")
TINTA_2 = colors.HexColor("#3d3c3a")
ACENTO = colors.HexColor("#1c5cab")
LINEA = colors.HexColor("#c9cdd4")
FONDO_TABLA = colors.HexColor("#eef4fd")
FONDO_CITA = colors.HexColor("#f5f7fa")

ANCHO_UTIL = letter[0] - 2 * 2.6 * cm

# --- Estilos ----------------------------------------------------------------
_base = getSampleStyleSheet()
E = {}
E["cuerpo"] = ParagraphStyle("cuerpo", parent=_base["BodyText"], fontName="Times-Roman",
                             fontSize=10.5, leading=15.2, alignment=TA_JUSTIFY,
                             spaceAfter=7, textColor=TINTA)
E["cuerpo_sin"] = ParagraphStyle("cuerpo_sin", parent=E["cuerpo"], spaceAfter=2)
E["h1"] = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=15, leading=19,
                         spaceBefore=16, spaceAfter=9, textColor=ACENTO)
E["h2"] = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=11.5, leading=15,
                         spaceBefore=11, spaceAfter=5, textColor=TINTA)
E["h3"] = ParagraphStyle("h3", fontName="Helvetica-BoldOblique", fontSize=10.5, leading=14,
                         spaceBefore=8, spaceAfter=3, textColor=TINTA_2)
E["pie_figura"] = ParagraphStyle("pie_figura", fontName="Helvetica", fontSize=8.6, leading=11.5,
                                 alignment=TA_CENTER, textColor=TINTA_2,
                                 spaceBefore=4, spaceAfter=12)
E["pie_tabla"] = ParagraphStyle("pie_tabla", fontName="Helvetica", fontSize=8.6, leading=11.5,
                                alignment=TA_LEFT, textColor=TINTA_2,
                                spaceBefore=2, spaceAfter=10)
E["titulo_tabla"] = ParagraphStyle("titulo_tabla", fontName="Helvetica-Bold", fontSize=9,
                                   leading=12, textColor=TINTA, spaceBefore=10, spaceAfter=4)
E["cita"] = ParagraphStyle("cita", parent=E["cuerpo"], fontName="Times-Italic",
                           leftIndent=18, rightIndent=18, spaceBefore=8, spaceAfter=8,
                           borderPadding=(8, 8, 8, 8), backColor=FONDO_CITA,
                           borderColor=LINEA, borderWidth=0.6)
E["nota"] = ParagraphStyle("nota", fontName="Times-Roman", fontSize=8.2, leading=10.6,
                           alignment=TA_JUSTIFY, textColor=TINTA_2, spaceAfter=2)
E["celda"] = ParagraphStyle("celda", fontName="Times-Roman", fontSize=8, leading=10.2,
                            alignment=TA_LEFT, textColor=TINTA)
E["celda_c"] = ParagraphStyle("celda_c", parent=E["celda"], alignment=TA_CENTER)
E["celda_neg"] = ParagraphStyle("celda_neg", parent=E["celda"], fontName="Times-Bold")
E["celda_min"] = ParagraphStyle("celda_min", parent=E["celda"], fontSize=6.3, leading=8,
                                alignment=TA_CENTER)
E["celda_min_izq"] = ParagraphStyle("celda_min_izq", parent=E["celda_min"], alignment=TA_LEFT)
E["encabezado"] = ParagraphStyle("encabezado", fontName="Helvetica-Bold", fontSize=8,
                                 leading=10.4, textColor=colors.white, alignment=TA_CENTER)
E["mono"] = ParagraphStyle("mono", fontName="Courier", fontSize=7.6, leading=9.6,
                           textColor=TINTA)
E["lista"] = ParagraphStyle("lista", parent=E["cuerpo"], leftIndent=16, bulletIndent=5,
                            spaceAfter=3)
E["portada_u"] = ParagraphStyle("portada_u", fontName="Helvetica-Bold", fontSize=15, leading=20,
                                alignment=TA_CENTER, textColor=ACENTO)
E["portada_s"] = ParagraphStyle("portada_s", fontName="Helvetica", fontSize=11.5, leading=16,
                                alignment=TA_CENTER, textColor=TINTA_2)
E["portada_t"] = ParagraphStyle("portada_t", fontName="Helvetica-Bold", fontSize=23, leading=29,
                                alignment=TA_CENTER, textColor=TINTA)
E["portada_st"] = ParagraphStyle("portada_st", fontName="Helvetica-Oblique", fontSize=12.5,
                                 leading=17, alignment=TA_CENTER, textColor=TINTA_2)
E["portada_n"] = ParagraphStyle("portada_n", fontName="Times-Roman", fontSize=12, leading=18,
                                alignment=TA_CENTER, textColor=TINTA)

# --- Contadores de figuras y tablas ----------------------------------------
_contador = {"figura": 0, "tabla": 0}


def figura(ruta, pie, ancho=None, maximo_alto=19.5 * cm):
    """Inserta una imagen escalada al ancho util con su pie numerado."""
    _contador["figura"] += 1
    from PIL import Image as PILImage
    with PILImage.open(ruta) as imagen:
        px_w, px_h = imagen.size
    ancho = ancho or ANCHO_UTIL
    alto = ancho * px_h / px_w
    if alto > maximo_alto:                    # No debe desbordar la caja de texto.
        alto = maximo_alto
        ancho = alto * px_w / px_h
    imagen_flow = Image(ruta, width=ancho, height=alto)
    imagen_flow.hAlign = "CENTER"
    return [imagen_flow,
            Paragraph(f"<b>Figura {_contador['figura']}.</b> {pie}", E["pie_figura"])]


def titulo_tabla(texto):
    _contador["tabla"] += 1
    return Paragraph(f"Tabla {_contador['tabla']}. {texto}", E["titulo_tabla"])


def tabla(datos, anchos, alineacion_centro=True, tamano=8):
    """Construye una tabla con el mismo aspecto en todo el informe."""
    estilo = [
        ("BACKGROUND", (0, 0), (-1, 0), ACENTO),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("GRID", (0, 0), (-1, -1), 0.4, LINEA),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, FONDO_TABLA]),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
    ]
    if alineacion_centro:
        estilo.append(("ALIGN", (0, 0), (-1, -1), "CENTER"))
    t = Table(datos, colWidths=anchos, repeatRows=1, style=TableStyle(estilo))
    t.hAlign = "CENTER"
    return t


def parrafo(texto, estilo="cuerpo"):
    return Paragraph(texto, E[estilo])


def vinetas(elementos):
    return [Paragraph(f"•&nbsp;&nbsp;{t}", E["lista"]) for t in elementos]


def notas_al_pie(items):
    """Bloque de notas al pie separado por una linea, al cierre de la seccion."""
    salida = [Spacer(1, 4),
              Table([[""]], colWidths=[ANCHO_UTIL],
                    style=TableStyle([("LINEABOVE", (0, 0), (-1, 0), 0.5, LINEA),
                                      ("TOPPADDING", (0, 0), (-1, -1), 0),
                                      ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))]
    for numero, texto in items:
        salida.append(Paragraph(f"<super>{numero}</super>&nbsp;{texto}", E["nota"]))
    salida.append(Spacer(1, 6))
    return salida


# --- Lectura de resultados --------------------------------------------------
def leer_resumen():
    """Lee el CSV agregado usando el tiempo minimo como estimador principal.

    Ver la seccion 5.2 del informe: la interferencia del sistema operativo solo
    puede anadir tiempo, de modo que el minimo de las 12 repeticiones es la
    mejor estimacion del costo real. El promedio, el maximo y la desviacion se
    conservan y se reportan integros en el Anexo 3.
    """
    with open(os.path.join(RESULTADOS, "benchmark.csv")) as archivo:
        registros = []
        for f in csv.DictReader(archivo):
            minimo = float(f["min_ms"])
            hilos = int(f["threads"])
            speedup = float(f["speedup_best"])
            registros.append({
                "n": int(f["n_balls"]), "modo": f["mode"], "hilos": hilos,
                "pasos": int(f["steps"]), "reps": int(f["repetitions"]),
                "avg": float(f["avg_ms"]), "min": minimo, "max": float(f["max_ms"]),
                "sd": float(f["stddev_ms"]), "speedup": speedup,
                "speedup_avg": float(f["speedup_avg"]),
                "ef": speedup / max(hilos, 1),
                "fps": 1000.0 / minimo if minimo > 0 else 0.0,
                "ok": f["available"] == "1", "nota": f["note"],
            })
        return registros


def leer_muestras():
    datos = {}
    with open(os.path.join(RESULTADOS, "benchmark_muestras.csv")) as archivo:
        for f in csv.DictReader(archivo):
            datos.setdefault((int(f["n_balls"]), f["mode"], int(f["threads"])), []) \
                 .append(float(f["ms_per_step"]))
    return datos


RESUMEN = leer_resumen()
MUESTRAS = leer_muestras()
VALORES_N = sorted({f["n"] for f in RESUMEN})
NOMBRE_MODO = {"secuencial": "Secuencial", "std::thread": "std::thread",
               "openmp-static": "OpenMP estatico", "openmp-tuned": "OpenMP ajustado"}


def buscar(modo, n, hilos=None):
    for f in RESUMEN:
        if f["modo"] == modo and f["n"] == n and (hilos is None or f["hilos"] == hilos):
            return f
    return None


MEJOR = max((f for f in RESUMEN if f["ok"] and f["modo"].startswith("openmp")),
            key=lambda f: f["speedup"])
# Con carga alta y ocho hilos es donde se compara mejor el reparto guiado contra
# el estatico, asi que varias secciones se apoyan en estos dos registros.
AJU8_MAX = next(f for f in RESUMEN if f["modo"] == "openmp-tuned" and f["n"] == 8000 and f["hilos"] == 8)
EST8_MAX = next(f for f in RESUMEN if f["modo"] == "openmp-static" and f["n"] == 8000 and f["hilos"] == 8)
PEOR_HILOS = min((f for f in RESUMEN if f["ok"] and f["modo"] == "std::thread"),
                 key=lambda f: f["speedup"])


# --- Plantilla del documento ------------------------------------------------
class InformeDocTemplate(BaseDocTemplate):
    """Plantilla con caratula sin numerar, indice y numeracion en el pie."""

    def __init__(self, ruta, **kwargs):
        super().__init__(ruta, **kwargs)
        margen = 2.6 * cm
        cuadro = Frame(margen, margen, letter[0] - 2 * margen,
                       letter[1] - 2 * margen - 0.7 * cm, id="normal")
        self.addPageTemplates([
            PageTemplate(id="portada", frames=[cuadro]),
            PageTemplate(id="contenido", frames=[cuadro], onPage=self._pie_de_pagina),
        ])

    def _pie_de_pagina(self, canvas, doc):
        canvas.saveState()
        canvas.setStrokeColor(LINEA)
        canvas.setLineWidth(0.5)
        canvas.line(2.6 * cm, 1.75 * cm, letter[0] - 2.6 * cm, 1.75 * cm)
        canvas.setFont("Helvetica", 7.6)
        canvas.setFillColor(TINTA_2)
        canvas.drawString(2.6 * cm, 1.35 * cm,
                          "Universidad del Valle de Guatemala  |  Computacion Paralela y "
                          "Distribuida  |  Proyecto 1")
        canvas.drawRightString(letter[0] - 2.6 * cm, 1.35 * cm, str(canvas.getPageNumber() - 1))
        canvas.restoreState()

    def afterFlowable(self, flowable):
        """Registra los encabezados en el indice."""
        if not isinstance(flowable, Paragraph):
            return
        estilo = flowable.style.name
        texto = flowable.getPlainText()
        if estilo == "h1" and texto != "Indice":
            self.notify("TOCEntry", (0, texto, self.page - 1))
        elif estilo == "h2":
            self.notify("TOCEntry", (1, texto, self.page - 1))


def encabezado(texto, nivel="h1"):
    return Paragraph(texto, E[nivel])


# ===========================================================================
# Contenido del informe
# ===========================================================================
def construir_portada():
    return [
        Spacer(1, 0.9 * cm),
        parrafo("UNIVERSIDAD DEL VALLE DE GUATEMALA", "portada_u"),
        parrafo("Facultad de Ingenieria<br/>Departamento de Ciencia de la Computacion",
                "portada_s"),
        Spacer(1, 0.35 * cm),
        Table([[""]], colWidths=[6 * cm],
              style=TableStyle([("LINEBELOW", (0, 0), (-1, -1), 1.2, ACENTO)]), hAlign="CENTER"),
        Spacer(1, 1.8 * cm),
        parrafo("CC3086 &mdash; Programacion de Microprocesadores<br/>"
                "Computacion Paralela y Distribuida<br/>Semestre 2, 2026", "portada_s"),
        Spacer(1, 1.2 * cm),
        parrafo("Proyecto No. 1", "portada_st"),
        Spacer(1, 0.4 * cm),
        parrafo("Plinko 3D Paralelo", "portada_t"),
        Spacer(1, 0.3 * cm),
        parrafo("Screensaver con simulacion fisica en C++ y paralelizacion "
                "de memoria compartida con OpenMP", "portada_st"),
        Spacer(1, 1.9 * cm),
        parrafo("<b>Integrantes</b>", "portada_n"),
        Spacer(1, 0.2 * cm),
        parrafo("Ian Rodrigo Cumes &mdash; 23236<br/>"
                "Javier Valladares &mdash; 23045<br/>"
                "Nery Molina &mdash; 23218", "portada_n"),
        Spacer(1, 1.7 * cm),
        parrafo("Guatemala, 30 de agosto de 2026", "portada_n"),
    ]


def construir_indice():
    indice = TableOfContents()
    indice.levelStyles = [
        ParagraphStyle("toc1", fontName="Helvetica-Bold", fontSize=10.5, leading=17,
                       textColor=TINTA, leftIndent=6, firstLineIndent=-6),
        ParagraphStyle("toc2", fontName="Times-Roman", fontSize=9.8, leading=14,
                       textColor=TINTA_2, leftIndent=24, firstLineIndent=-6),
    ]
    return [encabezado("Indice"), Spacer(1, 4), indice]


def seccion_introduccion():
    return [
        encabezado("1. Introduccion"),
        parrafo(
            "Este informe documenta el diseno, la implementacion y la evaluacion de "
            "<b>Plinko 3D Paralelo</b>, un screensaver escrito en C++17 que simula una piramide de "
            "Plinko tridimensional: N pelotas caen sobre el vertice de un cono de clavijas "
            "oscilantes, chocan entre si, se reparten hacia afuera rebotando de nivel en nivel, "
            "atraviesan zonas que alteran su fisica y terminan contadas en sectores angulares "
            "alrededor de la base. La camara orbita alrededor de la piramide, de modo que la "
            "estructura se percibe en tres dimensiones. El programa corre a pantalla completa, "
            "se dibuja con SDL2 y OpenGL, y despliega en pantalla los cuadros por segundo."),
        parrafo(
            "El objetivo del proyecto no es el juego en si, sino usarlo como banco de trabajo "
            "para un ejercicio de paralelizacion. Partimos de una version secuencial funcional y "
            "la fuimos transformando en versiones paralelas de memoria compartida, midiendo en "
            "cada iteracion el speedup y la eficiencia obtenidos. El resultado es una comparacion "
            "de cuatro estrategias sobre exactamente el mismo trabajo: una secuencial, una con un "
            "<font face='Courier'>std::thread</font> por pelota, y dos con OpenMP."),
        parrafo(
            "Lo que hace interesante al problema es que la parte cara de cada cuadro son las "
            "colisiones entre pelotas, cuyo costo crece con el cuadrado de N. Eso da suficiente "
            "trabajo por cuadro como para que la paralelizacion tenga algo que ganar, y al mismo "
            "tiempo obliga a resolver el problema central de la memoria compartida: como dejar "
            "que varios hilos lean el estado de todas las pelotas mientras cada uno escribe la "
            "suya, sin condiciones de carrera y sin pagar el costo de un candado por pelota."),
        parrafo(
            f"El mejor resultado obtenido fue un speedup de <b>{MEJOR['speedup']:.2f}x</b> con "
            f"{MEJOR['hilos']} hilos y N = {MEJOR['n']:,} pelotas".replace(",", " ") +
            f" (eficiencia {MEJOR['ef']:.2f}). El hallazgo mas util del proyecto, sin embargo, "
            "fue negativo: la primera version paralela, la que asignaba un hilo por pelota, "
            f"resulto mas de cinco veces <i>mas lenta</i> que la secuencial "
            f"({PEOR_HILOS['speedup']:.2f}x en el peor caso medido). Entender por que es lo que "
            "motivo el cambio a OpenMP."),
    ]


def seccion_antecedentes():
    return [
        encabezado("2. Antecedentes"),
        parrafo(
            "La programacion paralela de memoria compartida existe desde que los procesadores "
            "dejaron de ganar velocidad por reloj y empezaron a ganarla por cantidad de nucleos. "
            "Antes de OpenMP, aprovechar esos nucleos significaba escribir codigo explicito de "
            "hilos: crearlos, repartirles trabajo, sincronizarlos y destruirlos a mano, con una "
            "biblioteca distinta segun el sistema operativo."),
        parrafo(
            "OpenMP aparecio en 1997 como un estandar de directivas de compilador que permite "
            "expresar el paralelismo sin reescribir el programa. Su propuesta es incremental: se "
            "parte de un programa secuencial correcto y se le agregan directivas "
            "<font face='Courier'>#pragma omp</font> donde conviene, de modo que el mismo codigo "
            "sigue compilando y funcionando en un compilador que no soporte OpenMP "
            "(Chapman, Jost y van der Pas, 2007). Esa propiedad es exactamente la que este "
            "proyecto explota: las cuatro versiones conviven en el mismo binario y se pueden "
            "alternar con una tecla mientras el programa corre."),
        parrafo(
            "Para decidir <i>que</i> paralelizar, el proyecto sigue el metodo PCAM propuesto por "
            "Foster (1995): particionar el problema en tareas, analizar la comunicacion entre "
            "ellas, aglomerarlas para reducir el costo de esa comunicacion y finalmente mapearlas "
            "a procesadores. Es un metodo pensado para memoria distribuida, pero sus cuatro "
            "etapas se aplican igual de bien a memoria compartida, donde la etapa de comunicacion "
            "se traduce en accesos a memoria y en sincronizacion."),
        parrafo(
            "Como antecedente directo, este proyecto continua la primera entrega del curso, en la "
            "que el equipo construyo un tablero con doce pelotas, gravedad y rebotes contra los "
            "bordes, y una version paralela con un <font face='Courier'>std::thread</font> "
            "persistente por pelota. Aquella entrega ya dejaba anotado que, con solo doce pelotas "
            "y una fisica sencilla, era posible que el modo paralelo resultara mas lento que el "
            "secuencial por el costo de sincronizacion. Este informe cuantifica esa sospecha y la "
            "resuelve."),
    ]


def seccion_objetivos():
    return [
        encabezado("3. Objetivos"),
        encabezado("3.1 Objetivo general", "h2"),
        parrafo(
            "Disenar, implementar y evaluar un screensaver con simulacion fisica cuyo nucleo de "
            "computo se paralelice con OpenMP sobre memoria compartida, midiendo la mejora "
            "obtenida en terminos de speedup y eficiencia."),
        encabezado("3.2 Objetivos especificos", "h2"),
        *vinetas([
            "Implementar una version secuencial correcta y funcional que sirva como referencia "
            "de resultado y como denominador de todas las mediciones de speedup.",
            "Aplicar el metodo PCAM para identificar la descomposicion del problema y el patron "
            "de programacion paralela apropiado.",
            "Construir versiones paralelas sucesivas, cada una corrigiendo una limitacion "
            "medida en la anterior, y documentar la mejora que aporta cada iteracion.",
            "Emplear mecanismos de proteccion de memoria compartida y de sincronia adecuados a "
            "cada caso, evitando tanto las condiciones de carrera como la serializacion "
            "innecesaria.",
            "Parametrizar el programa mediante argumentos de linea de comandos con validacion "
            "defensiva, sin variables fijas en el codigo.",
            "Medir el tiempo de ejecucion con al menos diez repeticiones por configuracion y "
            "calcular el speedup y la eficiencia de cada version paralela.",
            "Determinar hasta que cantidad de elementos cada version sostiene 60 cuadros por "
            "segundo.",
        ]),
    ]


def seccion_marco_teorico():
    seq_8000 = buscar("secuencial", 8000)
    return [
        encabezado("4. Marco teorico"),
        encabezado("4.1 Memoria compartida y OpenMP", "h2"),
        parrafo(
            "En un sistema de memoria compartida todos los nucleos ven el mismo espacio de "
            "direcciones. Eso elimina el costo de enviar mensajes, pero introduce el problema "
            "opuesto: si dos hilos escriben la misma direccion sin coordinarse, el resultado "
            "depende del orden en que el planificador los ejecute. A eso se le llama condicion de "
            "carrera, y su sintoma tipico es un programa que da resultados distintos en cada "
            "corrida."),
        parrafo(
            "OpenMP resuelve la coordinacion con un modelo de bifurcacion y union: al encontrar "
            "una region <font face='Courier'>#pragma omp parallel</font> el hilo maestro crea un "
            "equipo de hilos, todos ejecutan la region, y al cerrarla se sincronizan en una "
            "barrera implicita antes de que el maestro continue solo. Dentro de la region, "
            "<font face='Courier'>#pragma omp for</font> reparte las iteraciones de un ciclo "
            "entre los hilos del equipo, tambien con barrera implicita al final "
            "(OpenMP Architecture Review Board, 2021)."),
        parrafo(
            "Las clausulas de reparto determinan como se dividen las iteraciones. Con "
            "<font face='Courier'>schedule(static)</font> cada hilo recibe de antemano un bloque "
            "contiguo del mismo tamano, lo que no cuesta nada en tiempo de ejecucion pero exige "
            "que todas las iteraciones cuesten lo mismo. Con "
            "<font face='Courier'>schedule(guided)</font> los hilos toman bloques que empiezan "
            "grandes y se van reduciendo, de modo que el reparto se ajusta solo cuando unas "
            "iteraciones resultan mas caras que otras, a cambio de un pequeno costo de "
            "planificacion."),
        encabezado("4.2 Metodo PCAM", "h2"),
        parrafo(
            "El metodo PCAM (Foster, 1995) descompone el diseno de un programa paralelo en cuatro "
            "etapas sucesivas:"),
        *vinetas([
            "<b>Particion.</b> Dividir el problema en la mayor cantidad de tareas independientes "
            "posible, sin pensar todavia en cuantos procesadores hay.",
            "<b>Comunicacion.</b> Identificar que datos necesita cada tarea de las demas y con "
            "que frecuencia.",
            "<b>Aglomeracion.</b> Agrupar tareas pequenas en tareas mayores para reducir el costo "
            "de comunicacion y de creacion de tareas.",
            "<b>Mapeo.</b> Asignar las tareas aglomeradas a los procesadores disponibles, "
            "buscando equilibrar la carga.",
        ]),
        parrafo(
            "La etapa de aglomeracion es la que explica el fracaso de nuestra primera version "
            "paralela: repartir una tarea por pelota es una particion correcta, pero saltarse la "
            "aglomeracion y mapear cada tarea a su propio hilo del sistema operativo hace que el "
            "costo de gestionar las tareas supere al trabajo que contienen."),
        encabezado("4.3 Speedup, eficiencia y sus limites", "h2"),
        parrafo(
            "El <b>speedup</b> compara el tiempo de la version secuencial contra el de la "
            "paralela sobre el mismo problema: S(p) = T(1) / T(p), donde p es la cantidad de "
            "hilos. La <b>eficiencia</b> normaliza ese numero por la cantidad de recursos "
            "empleados: E(p) = S(p) / p. Una eficiencia de 1.0 significa que cada hilo agregado "
            "aporta exactamente el trabajo de un hilo; en la practica siempre es menor, porque "
            "una parte del programa no se paraleliza y porque la sincronizacion cuesta."),
        parrafo(
            "La <b>ley de Amdahl</b> (Amdahl, 1967) pone el techo: si una fraccion f del programa "
            "es inherentemente secuencial, el speedup no puede pasar de 1 / (f + (1-f)/p), y "
            "cuando p tiende a infinito el limite es 1/f. Con f = 5&nbsp;% el techo es 20x, sin "
            "importar cuantos nucleos se agreguen."),
        parrafo(
            "La <b>ley de Gustafson</b> (Gustafson, 1988) matiza ese pesimismo observando que, en "
            "la practica, cuando se dispone de mas nucleos no se resuelve el mismo problema mas "
            "rapido sino un problema mas grande en el mismo tiempo. Esa observacion describe bien "
            "lo que ocurre en este proyecto: la parte secuencial de nuestro paso de fisica es "
            "practicamente constante, mientras que la parte paralela crece con N&sup2;, de modo "
            "que la eficiencia mejora conforme aumenta la carga. Es exactamente lo que muestran "
            f"las mediciones: con N = 250 el mejor speedup apenas llega a 1.93x, mientras que con "
            f"N = 8&nbsp;000, donde un solo paso secuencial cuesta {seq_8000['avg']:.0f}&nbsp;ms, "
            f"alcanza {MEJOR['speedup']:.2f}x."),
    ]


def seccion_metodologia():
    return [
        encabezado("5. Metodologia"),
        parrafo(
            "El enunciado del proyecto define el alcance de manera compacta. Se pide, "
            "textualmente:"),
        Paragraph(
            "&ldquo;realizar un programa que dibuje en pantalla un &lsquo;screensaver&rsquo; y "
            "que corra de forma paralela. Comenzaran disenando y programando la version "
            "secuencial. Una vez su version secuencial este lista (funcional y corriendo), "
            "procederan a buscar acelerarla y mejorarla utilizando OpenMP&rdquo;.<super>1</super>",
            E["cita"]),
        parrafo(
            "El trabajo siguio ese orden. Primero se construyo la simulacion secuencial completa "
            "y se verifico que produjera una escena estable; solo despues se introdujeron las "
            "directivas de OpenMP. Cada version paralela se acepto unicamente cuando dos "
            "condiciones se cumplian: que produjera exactamente el mismo estado que la "
            "secuencial, y que las pruebas automatizadas de determinismo pasaran con distintas "
            "cantidades de hilos."),
        encabezado("5.1 Entorno de medicion", "h2"),
        parrafo(
            "Todas las mediciones reportadas se tomaron en la misma maquina y en la misma sesion, "
            "con el binario compilado en modo <font face='Courier'>Release</font>:"),
        titulo_tabla("Entorno de las mediciones."),
        tabla([
            [Paragraph("Componente", E["encabezado"]), Paragraph("Detalle", E["encabezado"])],
            [Paragraph("Procesador", E["celda"]),
             Paragraph("Apple M1 Pro, 8 nucleos (6 de rendimiento y 2 de eficiencia)", E["celda"])],
            [Paragraph("Memoria", E["celda"]), Paragraph("16 GB unificados", E["celda"])],
            [Paragraph("Sistema operativo", E["celda"]), Paragraph("macOS 26.5.1 (25F80)", E["celda"])],
            [Paragraph("Compilador", E["celda"]),
             Paragraph("Apple clang 17.0.0, C++17, -O3 (CMAKE_BUILD_TYPE=Release)", E["celda"])],
            [Paragraph("OpenMP", E["celda"]),
             Paragraph("libomp 5.1 (Homebrew), enlazada mediante -Xpreprocessor -fopenmp", E["celda"])],
            [Paragraph("Graficos", E["celda"]), Paragraph("SDL2 y OpenGL 2.1 en perfil de compatibilidad", E["celda"])],
        ], [4.6 * cm, ANCHO_UTIL - 4.6 * cm], alineacion_centro=False),
        parrafo(
            "El detalle del procesador no es un dato de relleno: el M1 Pro es un procesador "
            "<i>heterogeneo</i>, con seis nucleos rapidos y dos lentos. Esa asimetria explica "
            "buena parte de los resultados de la seccion 7 y es la razon por la que la eficiencia "
            "cae al pasar de seis a ocho hilos."),
        encabezado("5.2 Protocolo de medicion", "h2"),
        parrafo(
            "El programa incluye un modo de banco de pruebas "
            "(<font face='Courier'>--benchmark</font>) que ejecuta exactamente la misma fisica "
            "que el screensaver pero sin abrir ventana ni dibujar nada. Eso es deliberado: si se "
            "midiera el cuadro completo, el tiempo estaria dominado por el renderizado y por la "
            "sincronia vertical, y el efecto de la paralelizacion quedaria oculto. El protocolo "
            "de cada medicion es el siguiente:"),
        *vinetas([
            "Se usa un paso de tiempo fijo de 1/60 de segundo, en lugar del reloj real, para que "
            "dos corridas simulen exactamente el mismo trabajo.",
            "Antes de cronometrar se ejecutan tres pasos de calentamiento, que llenan las caches "
            "y obligan a OpenMP a crear su equipo de hilos, de modo que ese costo no contamine la "
            "primera toma.",
            "Se cronometran 12 repeticiones por configuracion, dos mas que el minimo exigido, y "
            "se reportan promedio, minimo, maximo y desviacion estandar.",
            "La cantidad de pasos por repeticion se ajusta de forma inversamente proporcional a "
            "N&sup2;, para que las cargas grandes no dominen el tiempo total del banco.",
            "El speedup se calcula siempre contra la version secuencial medida con el mismo N en "
            "la misma corrida, no contra un valor de referencia guardado.",
        ]),
        encabezado("5.3 Por que se reporta el tiempo minimo y no el promedio", "h2"),
        parrafo(
            "Las cifras de este informe usan el <b>menor</b> de los doce tiempos de cada "
            "configuracion, no el promedio. La decision merece explicarse porque no es la mas "
            "intuitiva."),
        parrafo(
            "El equipo no dispone de una maquina dedicada: las mediciones se tomaron en un "
            "equipo de trabajo con macOS, que mantiene procesos de fondo fuera del control del "
            "programa. Esa interferencia tiene una propiedad importante: solo puede <i>anadir</i> "
            "tiempo a una medicion, nunca quitarlo. El minimo es, por lo tanto, la mejor "
            "estimacion disponible del costo real del codigo, mientras que el promedio mezcla el "
            "costo del programa con el de lo que haya estado corriendo al lado."),
        parrafo(
            "No es una eleccion de conveniencia, y hay una comprobacion que lo demuestra. La "
            "version de OpenMP ejecutada con <b>un solo hilo</b> hace exactamente el mismo "
            "trabajo que la secuencial, de modo que su speedup tiene que dar 1.00 por "
            "construccion; cualquier desviacion es ruido de medicion y nada mas. Con el "
            "estimador del minimo, esa fila da 1.00 o 1.01 en las seis cargas y en las dos "
            "versiones. Con el promedio se dispersaba entre 0.90 y 1.06, e incluso llegaba a "
            "producir eficiencias superiores a 1.0, que serian fisicamente imposibles en este "
            "programa."),
        parrafo(
            "En una primera corrida el efecto fue extremo: las primeras cuatro repeticiones de "
            "la version secuencial con N = 2&nbsp;000 dieron entre 15 y 24&nbsp;ms mientras las "
            "ocho restantes se estabilizaron en 10.5&nbsp;ms. Ese unico bloque contaminado "
            "inflaba el promedio de la referencia y, con el, todos los speedup calculados contra "
            "ella. El minimo no se vio afectado."),
        parrafo(
            "Nada se oculta con esto: el Anexo 3 reproduce las doce repeticiones individuales de "
            "cada configuracion, junto con el promedio, el maximo y la desviacion estandar, de "
            "modo que cualquiera puede recalcular las cifras con el estimador que prefiera. La "
            "seccion 7.6 discute justamente la dispersion observada."),
        parrafo(
            "Se barrieron seis valores de N (250, 500, 1&nbsp;000, 2&nbsp;000, 4&nbsp;000 y "
            "8&nbsp;000) y cinco cantidades de hilos (1, 2, 4, 6 y 8), lo que da 72 "
            "configuraciones y 828 mediciones individuales. Todas estan en el Anexo 3 y en los "
            "archivos <font face='Courier'>docs/resultados/</font> del repositorio."),
        *notas_al_pie([
            (1, "Universidad del Valle de Guatemala, Departamento de Ciencia de la Computacion. "
                "<i>Proyecto No. 1</i>, Computacion Paralela y Distribuida, Semestre 2, 2026, "
                "seccion &ldquo;Contenido&rdquo;."),
        ]),
    ]


def seccion_diseno():
    return [
        encabezado("6. Diseno e implementacion"),
        encabezado("6.1 El problema y por que es paralelizable", "h2"),
        parrafo(
            "Cada cuadro de la simulacion consiste en avanzar N pelotas un intervalo de tiempo. "
            "Para una pelota concreta ese avance requiere: aplicar gravedad y el efecto de las K "
            "zonas modificadoras que la contienen, resolver las colisiones contra las otras N-1 "
            "pelotas, integrar velocidad y posicion, resolver las colisiones contra las M "
            "clavijas de la piramide, rebotar contra la pared cilindrica y el techo, y finalmente "
            "comprobar si llego al piso para anotarla en un sector angular y hacerla reaparecer "
            "sobre el vertice."),
        parrafo(
            "El costo total por paso es entonces O(N&sup2; + N&middot;M + N&middot;K). El termino "
            "dominante para N grande es el cuadratico de las colisiones entre pelotas, y es "
            "precisamente el que interesa paralelizar: es trabajo aritmetico puro, sin entrada ni "
            "salida, y crece rapido con el parametro que el usuario controla."),
        encabezado("6.2 Aplicacion del metodo PCAM", "h2"),
        titulo_tabla("Descomposicion del problema segun el metodo PCAM."),
        tabla([
            [Paragraph("Etapa", E["encabezado"]), Paragraph("Decision tomada", E["encabezado"])],
            [Paragraph("Particion", E["celda_neg"]),
             Paragraph("Una tarea por pelota. Es la descomposicion de dominio natural: el arreglo "
                       "de pelotas es el unico que crece con el parametro N que el usuario "
                       "controla, y cada pelota se actualiza con la misma secuencia de "
                       "operaciones.", E["celda"])],
            [Paragraph("Comunicacion", E["celda_neg"]),
             Paragraph("Cada tarea necesita leer el estado del cuadro anterior de todas las demas "
                       "pelotas, de las clavijas y de los modificadores, pero solo escribe su "
                       "propia posicion. Es un patron de recoleccion (gather): muchas lecturas "
                       "compartidas, una sola escritura privada.", E["celda"])],
            [Paragraph("Aglomeracion", E["celda_neg"]),
             Paragraph("Las tareas se agrupan en bloques de indices contiguos. Como el arreglo de "
                       "pelotas esta contiguo en memoria, un bloque de indices es tambien un "
                       "bloque de lineas de cache, lo que aprovecha la localidad espacial y "
                       "amortiza el costo de planificacion entre muchas pelotas.", E["celda"])],
            [Paragraph("Mapeo", E["celda_neg"]),
             Paragraph("Los bloques se asignan a los hilos del equipo de OpenMP. Se probaron dos "
                       "politicas: reparto estatico, que no cuesta nada pero supone carga "
                       "uniforme, y reparto guiado, que se adapta cuando unas pelotas colisionan "
                       "mas que otras o cuando los nucleos no son igual de rapidos.", E["celda"])],
        ], [3.2 * cm, ANCHO_UTIL - 3.2 * cm], alineacion_centro=False),
        encabezado("6.3 Doble buffer: por que no hay condiciones de carrera", "h2"),
        parrafo(
            "La decision de diseno mas importante del proyecto es la que elimina las carreras por "
            "construccion en lugar de protegerlas con candados. La simulacion mantiene "
            "<i>dos</i> arreglos de pelotas: <font face='Courier'>current_</font>, que durante "
            "todo el paso es de solo lectura, y <font face='Courier'>next_</font>, en el que cada "
            "indice tiene un unico escritor. Al terminar el paso, los dos se intercambian."),
        parrafo(
            "De ahi se siguen tres propiedades que valen la pena enumerar. Primero, ningun hilo "
            "escribe una direccion que otro pueda leer durante el paso, de modo que no hace falta "
            "ningun candado sobre el estado de las pelotas. Segundo, como todas las tareas leen "
            "el mismo estado congelado, el resultado no depende del orden en que los hilos las "
            "ejecuten: la simulacion es <i>determinista</i> y produce exactamente los mismos bits "
            "con 1 hilo que con 16. Tercero, esa determinacion se puede convertir en una prueba "
            "automatizada, y es lo que hacen las suites "
            "<font face='Courier'>equivalencia</font> y "
            "<font face='Courier'>determinismo</font> descritas en la seccion 6.8."),
        parrafo(
            "La alternativa habitual &mdash;actualizar las pelotas en el mismo arreglo y proteger "
            "cada par con un candado&mdash; habria costado un mutex por colision, es decir del "
            "orden de N&sup2; operaciones atomicas por paso, y ademas habria hecho el resultado "
            "dependiente del orden de ejecucion. El costo del doble buffer es la memoria: dos "
            "arreglos en lugar de uno. Con 8&nbsp;000 pelotas de 56 bytes, eso son 896&nbsp;KB en "
            "total, un precio despreciable."),
        encabezado("6.4 Generacion pseudoaleatoria sin estado compartido", "h2"),
        parrafo(
            "Un detalle que suele romper el determinismo de un programa paralelo es el generador "
            "de numeros aleatorios. Usar <font face='Courier'>std::rand</font> o un unico "
            "<font face='Courier'>std::mt19937</font> global obligaria a serializar con un mutex "
            "y, peor aun, haria que el resultado dependiera del orden en que los hilos consumen "
            "numeros."),
        parrafo(
            "La solucion adoptada es que cada pelota lleve su propia semilla de 32 bits dentro de "
            "su estructura, y que la avance con un generador SplitMix32 sin estado global. La "
            "semilla inicial de la pelota <i>i</i> se deriva de la semilla global y de <i>i</i> "
            "mediante una funcion de mezcla, de modo que la inicializacion tampoco necesita "
            "recorrer el generador secuencialmente. El resultado es que la desviacion "
            "pseudoaleatoria que recibe una pelota al golpear una clavija depende unicamente de "
            "su propia historia, nunca del calendario de los hilos."),
        encabezado("6.5 Las cuatro versiones", "h2"),
        parrafo(
            "Las cuatro estrategias comparten el mismo nucleo de fisica, la funcion "
            "<font face='Courier'>advanceBall()</font>, y se diferencian unicamente en como "
            "recorren el arreglo de pelotas y en como acumulan los contadores compartidos. Eso "
            "garantiza que la comparacion sea justa: se esta midiendo la estrategia de "
            "paralelizacion, no dos implementaciones distintas de la fisica."),
        encabezado("Version 0 &mdash; Secuencial", "h3"),
        parrafo(
            "Un ciclo simple sobre las N pelotas, en un solo hilo, que acumula los sectores en el "
            "mismo recorrido. Es la referencia de correccion y el denominador de todo speedup "
            "reportado en este informe."),
        encabezado("Version 1 &mdash; Un std::thread por pelota", "h3"),
        parrafo(
            "Esta fue la primera version paralela del equipo, heredada de la entrega anterior. "
            "Crea N hilos persistentes, uno por pelota, y los coordina con una barrera de dos "
            "fases construida sobre un mutex y dos variables de condicion: el hilo coordinador "
            "publica los datos de la ronda, incrementa un numero de generacion y despierta a "
            "todos; cada worker calcula su pelota fuera de la seccion critica y decrementa un "
            "contador de pendientes; el ultimo en terminar despierta al coordinador."),
        parrafo(
            "La implementacion es correcta &mdash;pasa la prueba de equivalencia&mdash; pero es "
            "un desastre de rendimiento, y por eso se conserva en el codigo final: es el "
            "contraejemplo que justifica todo lo demas. Con N = 1&nbsp;000 el programa crea mil "
            "hilos del sistema operativo que, en cada uno de los dos sub-pasos de cada cuadro, "
            "deben despertarse, competir por un unico mutex, hacer unos pocos microsegundos de "
            "trabajo y volver a dormirse. Ese patron se conoce como &ldquo;estampida de "
            "hilos&rdquo; y su costo crece linealmente con N mientras el trabajo por hilo "
            "permanece constante. Por encima de 1&nbsp;024 pelotas el sistema operativo "
            "sencillamente rechaza la creacion; el programa detecta la excepcion, informa y "
            "continua con otra estrategia en lugar de abortar."),
        encabezado("Version 2 &mdash; OpenMP con reparto estatico", "h3"),
        parrafo(
            "La traduccion directa del ciclo secuencial. Una sola directiva convierte el "
            "recorrido en paralelo:"),
        Paragraph(
            "#pragma omp parallel for schedule(static) num_threads(threads) \\<br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;shared(readBalls, writeBalls, pegData, modifierData, "
            "binData, params) \\<br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;firstprivate(elements, pegCount, modifierCount, subTime, "
            "subDelta) \\<br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;reduction(+ : recycled)", E["mono"]),
        Spacer(1, 6),
        parrafo(
            "Los punteros a los buffers se copian a variables locales antes de la region para no "
            "depender del puntero <font face='Courier'>this</font> dentro de las clausulas, lo "
            "que hace el codigo mas portable entre compiladores y evita una indireccion en el "
            "ciclo caliente. El total de pelotas recicladas se acumula con una reduccion, y los "
            "contadores por sector &mdash;que si son un recurso realmente compartido&mdash; se "
            "protegen con <font face='Courier'>#pragma omp atomic</font>."),
        encabezado("Version 3 &mdash; OpenMP ajustado", "h3"),
        parrafo(
            "La tercera version corrige tres cosas que la medicion de la segunda dejo en "
            "evidencia:"),
        *vinetas([
            "<b>Una sola region paralela por cuadro.</b> La version estatica abria y cerraba una "
            "region por cada sub-paso de integracion. La ajustada abre "
            "<font face='Courier'>#pragma omp parallel</font> una vez y ejecuta dentro de ella "
            "todos los sub-pasos, usando la barrera implicita del "
            "<font face='Courier'>#pragma omp for</font> como sincronia entre uno y otro. La "
            "alternancia de buffers se resuelve por la paridad del sub-paso, porque dentro de la "
            "region no es posible intercambiar los vectores.",
            "<b>Reparto guiado.</b> Con <font face='Courier'>schedule(guided)</font> los bloques "
            "empiezan grandes y se reducen, lo que nivela la carga cuando unas pelotas colisionan "
            "mas que otras y, sobre todo, cuando dos de los ocho nucleos son mas lentos que los "
            "otros seis.",
            "<b>Contadores privatizados.</b> En lugar de una operacion atomica por pelota "
            "reciclada, cada hilo acumula en un arreglo privado y al final del cuadro fusiona su "
            "copia dentro de un <font face='Courier'>#pragma omp critical</font>. La seccion "
            "critica se ejecuta una vez por hilo y por cuadro, no una vez por pelota.",
        ]),
        parrafo(
            "Esta version incluye ademas una metrica de diagnostico: cada hilo registra su tiempo "
            "ocupado en una entrada alineada a 64 bytes &mdash;para que dos hilos nunca escriban "
            "en la misma linea de cache y no aparezca <i>false sharing</i>&mdash; y, tras un "
            "<font face='Courier'>#pragma omp barrier</font> explicito, un solo hilo calcula el "
            "cociente entre el tiempo maximo y el promedio. Ese numero es el desbalance de carga, "
            "y es lo que permitio identificar la asimetria de los nucleos como la causa de la "
            "caida de eficiencia con ocho hilos."),
        encabezado("6.6 Mecanismos de proteccion de memoria compartida", "h2"),
        parrafo(
            "El proyecto usa cuatro mecanismos distintos, cada uno donde corresponde. Vale la "
            "pena listarlos juntos porque la eleccion, y no solo la presencia, es lo que "
            "distingue una paralelizacion buena de una que apenas funciona:"),
        titulo_tabla("Mecanismos de sincronia empleados y su justificacion."),
        tabla([
            [Paragraph("Mecanismo", E["encabezado"]), Paragraph("Donde", E["encabezado"]),
             Paragraph("Por que ahi", E["encabezado"])],
            [Paragraph("Barrera de dos fases con mutex y dos condition_variable", E["celda"]),
             Paragraph("BallThreadSystem::runRound", E["celda"]),
             Paragraph("Es el unico modo de coordinar hilos crudos de C++. El coordinador no "
                       "avanza hasta que el contador de pendientes llega a cero, lo que impide "
                       "que OpenGL lea una posicion que otro hilo todavia escribe.", E["celda"])],
            [Paragraph("Barrera implicita de #pragma omp for", E["celda"]),
             Paragraph("Entre sub-pasos de integracion", E["celda"]),
             Paragraph("El sub-paso siguiente lee lo que el actual acaba de escribir, asi que la "
                       "barrera es obligatoria. Aprovechar la implicita evita agregar una "
                       "explicita redundante.", E["celda"])],
            [Paragraph("#pragma omp atomic", E["celda"]),
             Paragraph("Contadores de sector en la version estatica", E["celda"]),
             Paragraph("Varias pelotas pueden caer en el mismo sector en el mismo paso. El "
                       "incremento atomico es mas barato que una seccion critica cuando la "
                       "contencion es baja.", E["celda"])],
            [Paragraph("#pragma omp critical + contadores privados", E["celda"]),
             Paragraph("Fusion de sectores en la version ajustada", E["celda"]),
             Paragraph("Privatizar y fusionar una vez por hilo cambia el costo de O(reciclajes) "
                       "operaciones atomicas a O(hilos) secciones criticas por cuadro.", E["celda"])],
            [Paragraph("reduction(+ : ...)", E["celda"]),
             Paragraph("Total de pelotas recicladas", E["celda"]),
             Paragraph("La reduccion la implementa el compilador con acumuladores privados y una "
                       "combinacion en arbol; es la forma idiomatica y la mas eficiente.", E["celda"])],
            [Paragraph("#pragma omp barrier + #pragma omp single", E["celda"]),
             Paragraph("Metrica de desbalance de carga", E["celda"]),
             Paragraph("Todos los hilos deben haber publicado su tiempo antes de que uno solo lo "
                       "reduzca. Aqui la barrera explicita si es necesaria.", E["celda"])],
        ], [3.9 * cm, 3.9 * cm, ANCHO_UTIL - 7.8 * cm], alineacion_centro=False),
        encabezado("6.7 Parametrizacion y programacion defensiva", "h2"),
        parrafo(
            "El proyecto exige evitar variables fijas en el codigo, y el programa no tiene "
            "ninguna que afecte la carga de trabajo o el lienzo: todo se lee de la linea de "
            "comandos. Las opciones cubren la escena (N, niveles y anchura de la piramide, cantidad "
            "de modificadores y de sectores, radio y altura del cilindro, radio de la pelota, "
            "gravedad, restitucion, sub-pasos, semilla), la ventana (ancho, alto, pantalla "
            "completa, sincronia vertical), la camara (velocidad de giro e inclinacion), la "
            "ejecucion (modo e hilos) y la medicion (banco de pruebas y capturas)."),
        parrafo(
            "Cada valor se valida antes de usarse. Las conversiones numericas exigen que se "
            "consuma toda la cadena, de modo que una entrada como "
            "<font face='Courier'>12abc</font> se rechaza en lugar de aceptarse parcialmente; se "
            "verifica que las opciones que llevan valor efectivamente lo reciban antes de leer "
            "fuera de <font face='Courier'>argv</font>; y todos los rangos se comprueban, "
            "incluido el minimo de 640x480 pixeles que pide el enunciado y el minimo de diez "
            "repeticiones del banco de pruebas. Ante cualquier error el programa explica cual fue "
            "el problema, imprime la ayuda y termina con codigo distinto de cero."),
        parrafo(
            "Si el usuario no indica N, el programa lo solicita por consola. Esa solicitud solo "
            "ocurre cuando la entrada estandar es interactiva &mdash;de lo contrario, dentro de "
            "un script o de CTest, el programa se quedaria esperando para siempre&mdash;, admite "
            "Enter para conservar el valor por omision y reintenta hasta tres veces antes de "
            "rendirse."),
        encabezado("6.8 Verificacion automatizada", "h2"),
        parrafo(
            "El repositorio incluye cuatro suites de pruebas registradas en CTest. Las dos "
            "primeras son las que sostienen todo el argumento de correccion del informe:"),
        *vinetas([
            "<b>equivalencia</b>: ejecuta 180 pasos con 220 pelotas en los cuatro modos y exige "
            "que el estado final sea identico <i>bit a bit</i>, sin tolerancia. No se usa una "
            "comparacion aproximada a proposito: como el diseno garantiza que las operaciones de "
            "punto flotante se ejecutan en el mismo orden en todos los modos, cualquier "
            "diferencia indicaria una condicion de carrera real.",
            "<b>determinismo</b>: repite 240 pasos con 1, 2, 3, 5, 8 y 16 hilos y verifica que "
            "tanto el estado de las pelotas como los conteos por sector coincidan.",
            "<b>argumentos</b>: 21 casos de linea de comandos, entre validos e invalidos, que "
            "comprueban que la programacion defensiva acepta lo correcto y rechaza lo incorrecto "
            "con un mensaje explicativo.",
            "<b>conteos</b>: verifica que la suma de los sectores coincida exactamente con el "
            "total de pelotas recicladas. Detectaria un incremento perdido por una carrera en los "
            "contadores compartidos, que es justo lo que protegen "
            "<font face='Courier'>atomic</font> y <font face='Courier'>critical</font>.",
        ]),
        parrafo("Las cuatro suites pasan en la version entregada."),
    ]


def ms(valor):
    """Formatea un tiempo en ms con la precision que quepa en la columna."""
    if valor >= 100:
        return f"{valor:.1f}"
    if valor >= 10:
        return f"{valor:.2f}"
    return f"{valor:.3f}"


def _fila_resumen(modo, n, hilos=None):
    f = buscar(modo, n, hilos)
    if f is None or not f["ok"]:
        return None
    return f


def tabla_principal():
    """Tabla comparativa: para cada N, la mejor cifra de cada modo."""
    encabezados = ["N", "Secuencial<br/>ms/paso", "std::thread<br/>speedup",
                   "OMP estatico 8h<br/>speedup", "OMP ajustado 8h<br/>speedup",
                   "Mejor speedup<br/>observado", "Hilos del<br/>mejor"]
    datos = [[Paragraph(h, E["encabezado"]) for h in encabezados]]
    for n in VALORES_N:
        base = buscar("secuencial", n)
        hilo = _fila_resumen("std::thread", n)
        est = _fila_resumen("openmp-static", n, 8)
        aju = _fila_resumen("openmp-tuned", n, 8)
        candidatos = [f for f in RESUMEN
                      if f["n"] == n and f["ok"] and f["modo"].startswith("openmp")]
        mejor = max(candidatos, key=lambda f: f["speedup"])
        datos.append([
            Paragraph(f"{n:,}".replace(",", " "), E["celda_c"]),
            Paragraph(f"{base['min']:.3f}", E["celda_c"]),
            Paragraph(f"{hilo['speedup']:.2f}x" if hilo else "no viable", E["celda_c"]),
            Paragraph(f"{est['speedup']:.2f}x" if est else "&mdash;", E["celda_c"]),
            Paragraph(f"{aju['speedup']:.2f}x" if aju else "&mdash;", E["celda_c"]),
            Paragraph(f"<b>{mejor['speedup']:.2f}x</b>", E["celda_c"]),
            Paragraph(f"{mejor['hilos']}", E["celda_c"]),
        ])
    ancho = ANCHO_UTIL / 7
    return tabla(datos, [ancho] * 7)


def tabla_por_modo(modo, titulo):
    """Tabla completa de un modo: filas por N, columnas por cantidad de hilos."""
    hilos_lista = sorted({f["hilos"] for f in RESUMEN if f["modo"] == modo})
    encabezados = ["N"] + [f"{h} hilo{'s' if h > 1 else ''}" for h in hilos_lista]
    datos = [[Paragraph(h, E["encabezado"]) for h in encabezados]]
    for n in VALORES_N:
        fila = [Paragraph(f"{n:,}".replace(",", " "), E["celda_c"])]
        for h in hilos_lista:
            f = buscar(modo, n, h)
            if f is None or not f["ok"]:
                fila.append(Paragraph("&mdash;", E["celda_c"]))
            else:
                fila.append(Paragraph(
                    f"{f['min']:.3f} ms<br/>{f['speedup']:.2f}x &middot; E={f['ef']:.2f}",
                    E["celda_c"]))
        datos.append(fila)
    ancho_n = 1.9 * cm
    resto = (ANCHO_UTIL - ancho_n) / len(hilos_lista)
    return [KeepTogether([titulo_tabla(titulo),
                          tabla(datos, [ancho_n] + [resto] * len(hilos_lista))])]


def seccion_resultados():
    seq250 = buscar("secuencial", 250)
    seq8000 = buscar("secuencial", 8000)
    import math
    pendiente_total = (math.log(seq8000["min"] / seq250["min"]) / math.log(8000 / 250))
    seq1000 = buscar("secuencial", 1000)
    pendiente_alta = (math.log(seq8000["min"] / seq1000["min"]) / math.log(8.0))
    hilo1000 = buscar("std::thread", 1000)
    aju4_8000 = buscar("openmp-tuned", 8000, 4)
    est4_8000 = buscar("openmp-static", 8000, 4)
    aju8_8000 = buscar("openmp-tuned", 8000, 8)
    est8_8000 = buscar("openmp-static", 8000, 8)

    contenido = [
        encabezado("7. Resultados"),
        parrafo(
            "Esta seccion presenta las cifras agregadas. Las 828 mediciones individuales que las "
            "sostienen estan en el Anexo 3."),
        encabezado("7.1 Panorama general", "h2"),
        titulo_tabla("Resumen comparativo de las cuatro versiones. El speedup se calcula contra "
                     "la version secuencial medida con el mismo N."),
        tabla_principal(),
        parrafo(
            "La tabla concentra los tres hallazgos del proyecto. Primero, la version de un hilo "
            f"por pelota es consistentemente mas de cinco veces mas lenta que la secuencial: "
            f"con N = 1&nbsp;000 su speedup es de apenas {hilo1000['speedup']:.2f}x, es decir "
            f"que tarda {1 / hilo1000['speedup']:.1f} veces mas. Segundo, ambas versiones de "
            "OpenMP aceleran el programa en todos los tamanos probados, con un maximo de "
            f"{MEJOR['speedup']:.2f}x. Tercero, cual de las dos versiones de OpenMP conviene "
            "depende del punto de operacion: hasta cuatro hilos son equivalentes, y la ventaja "
            "clara de la ajustada aparece con ocho hilos y carga alta "
            f"({AJU8_MAX['speedup']:.2f}x contra {EST8_MAX['speedup']:.2f}x con N = 8&nbsp;000). "
            "La seccion 8.2 desglosa el cuadro completo."),
    ]

    contenido += figura(os.path.join(GRAFICAS, "fig1_speedup.png"),
                        "Speedup de las dos versiones de OpenMP frente al numero de hilos, con "
                        "un panel por cantidad de pelotas. La linea discontinua marca el speedup "
                        "ideal. La distancia entre las curvas y esa linea crece al reducir N, "
                        "que es el comportamiento que anticipa la ley de Gustafson.")

    contenido += [
        encabezado("7.2 Eficiencia", "h2"),
        parrafo(
            "La eficiencia es la cifra que mejor describe el aprovechamiento real del hardware. "
            "Los resultados dibujan un patron muy nitido: con dos y cuatro hilos, y siempre que N "
            "sea suficientemente grande, la eficiencia se mantiene por encima de 0.90; al pasar a "
            "seis y sobre todo a ocho hilos, cae de forma marcada."),
    ]
    contenido += figura(os.path.join(GRAFICAS, "fig2_eficiencia.png"),
                        "Eficiencia (speedup dividido entre el numero de hilos). El descenso a "
                        "partir de seis hilos es sistematico y se explica en la seccion 8.")
    contenido += figura(os.path.join(GRAFICAS, "fig7_mapa_eficiencia.png"),
                        "Eficiencia de la version OpenMP ajustada para cada combinacion de "
                        "carga y de hilos. La region de eficiencia alta se ensancha hacia la "
                        "derecha conforme crece N.", ancho=13.5 * cm)

    contenido += [
        encabezado("7.3 Escalamiento con la cantidad de pelotas", "h2"),
        parrafo(
            "En escala logaritmica doble, un costo O(N&sup2;) aparece como una recta de pendiente "
            f"2. La pendiente medida entre N = 250 y N = 8&nbsp;000 es de "
            f"{pendiente_total:.2f}, y entre N = 1&nbsp;000 y N = 8&nbsp;000 sube a "
            f"{pendiente_alta:.2f}. La diferencia es esperable: para N pequeno el costo lineal de "
            "las clavijas y los modificadores todavia pesa, y una parte del arreglo cabe en cache; "
            "conforme N crece, el termino cuadratico de las colisiones entre pelotas domina cada "
            "vez mas y la pendiente se acerca a 2."),
    ]
    contenido += figura(os.path.join(GRAFICAS, "fig3_escalamiento.png"),
                        "Tiempo por paso de fisica frente a N, en escala logaritmica doble. La "
                        "linea discontinua es el presupuesto de 16.7 ms que corresponde a 60 "
                        "cuadros por segundo.")

    contenido += [
        encabezado("7.4 Cuantas pelotas caben en 60 cuadros por segundo", "h2"),
        parrafo(
            "El enunciado pide medir cuantos elementos se pueden generar sin perder cuadros por "
            "segundo. Tomando como presupuesto los 16.7&nbsp;ms de un cuadro a 60&nbsp;FPS y "
            "midiendo solo la fisica, los limites observados son los siguientes:"),
        titulo_tabla("Ultimo valor de N medido que se mantiene dentro del presupuesto de 60 FPS, "
                     "y primer valor que lo excede."),
    ]

    filas = [[Paragraph(h, E["encabezado"]) for h in
              ["Version", "Ultimo N dentro<br/>del presupuesto", "ms/paso ahi",
               "Primer N que<br/>lo excede", "ms/paso ahi"]]]
    for modo, hilos, etiqueta in (("secuencial", 1, "Secuencial"),
                                  ("std::thread", None, "std::thread (1 por pelota)"),
                                  ("openmp-static", 8, "OpenMP estatico, 8 hilos"),
                                  ("openmp-tuned", 8, "OpenMP ajustado, 8 hilos")):
        puntos = sorted((f for f in RESUMEN if f["modo"] == modo and f["ok"]
                         and (hilos is None or f["hilos"] == hilos)), key=lambda f: f["n"])
        dentro = [f for f in puntos if f["min"] <= 1000 / 60]
        fuera = [f for f in puntos if f["min"] > 1000 / 60]
        filas.append([
            Paragraph(etiqueta, E["celda"]),
            Paragraph(f"{dentro[-1]['n']:,}".replace(",", " ") if dentro else "&mdash;", E["celda_c"]),
            Paragraph(f"{dentro[-1]['min']:.2f}" if dentro else "&mdash;", E["celda_c"]),
            Paragraph(f"{fuera[0]['n']:,}".replace(",", " ") if fuera else "no se alcanzo", E["celda_c"]),
            Paragraph(f"{fuera[0]['min']:.2f}" if fuera else "&mdash;", E["celda_c"]),
        ])
    contenido.append(tabla(filas, [5.4 * cm] + [(ANCHO_UTIL - 5.4 * cm) / 4] * 4,
                           alineacion_centro=False))
    contenido.append(parrafo(
        "En terminos practicos: la version secuencial se queda sin presupuesto entre 2&nbsp;000 y "
        "4&nbsp;000 pelotas, mientras que la version ajustada con ocho hilos todavia cumple con "
        "4&nbsp;000. La paralelizacion, por lo tanto, duplica con holgura la cantidad de "
        "elementos que el screensaver puede mostrar sin perder fluidez."))
    contenido += figura(os.path.join(GRAFICAS, "fig6_fps.png"),
                        "Cuadros por segundo que permite la fisica de cada version. Solo se "
                        "contabiliza el costo de la simulacion, no el del dibujado.")

    contenido += [
        encabezado("7.5 Comparacion directa de las dos versiones de OpenMP", "h2"),
        parrafo(
            "Con carga alta la version ajustada supera a la estatica precisamente donde la "
            f"estatica se degrada: con ocho hilos y N = 8&nbsp;000 pasa de "
            f"{est8_8000['speedup']:.2f}x a {aju8_8000['speedup']:.2f}x, una mejora relativa del "
            f"{100 * (aju8_8000['speedup'] / est8_8000['speedup'] - 1):.0f}&nbsp;%. Con cuatro "
            f"hilos, en cambio, las dos son practicamente indistinguibles "
            f"({est4_8000['speedup']:.2f}x contra {aju4_8000['speedup']:.2f}x), lo que confirma "
            "que la ganancia del reparto guiado proviene de nivelar hilos desiguales, no de "
            "reducir la sobrecarga en general. Con cargas pequenas la relacion se invierte: el "
            "reparto guiado cobra un costo de planificacion que ahi no alcanza a amortizarse."),
    ]
    contenido += figura(os.path.join(GRAFICAS, "fig4_static_vs_tuned.png"),
                        "Speedup de ambas versiones de OpenMP con N = 8 000. Con cuatro hilos "
                        "empatan; la diferencia aparece al usar los nucleos de eficiencia.",
                        ancho=15 * cm)

    contenido += [
        encabezado("7.6 Dispersion de las mediciones", "h2"),
        parrafo(
            "Las doce repeticiones de cada configuracion permiten juzgar cuanta confianza merece "
            "cada cifra. La forma de la distribucion es siempre la misma: un grupo compacto de "
            "valores bajos y unos pocos valores altos aislados. Esa asimetria es la firma tipica "
            "de la interferencia externa y es exactamente lo que justifica el criterio del minimo "
            "expuesto en la seccion 5.3: los valores altos no describen al programa, describen a "
            "lo que corria junto a el."),
        parrafo(
            "La dispersion crece de forma sistematica con la cantidad de hilos. Con uno, dos y "
            "cuatro hilos las repeticiones se agrupan estrechamente; con seis y ocho aparecen "
            "valores extremos. Tiene sentido: cuantos mas hilos pide el programa, mas probable es "
            "que alguno de ellos comparta un nucleo con un proceso del sistema, y el paso completo "
            "se retrasa hasta que ese hilo rezagado llega a la barrera."),
    ]
    contenido += figura(os.path.join(GRAFICAS, "fig5_dispersion.png"),
                        "Las doce repeticiones de cada medicion con N = 4 000. Cada punto es una "
                        "repeticion y la barra negra marca el promedio. La dispersion crece "
                        "visiblemente al usar mas de cuatro hilos.")
    return contenido


def seccion_analisis():
    hilo250 = buscar("std::thread", 250)
    hilo1000 = buscar("std::thread", 1000)
    aju8_250 = buscar("openmp-tuned", 250, 8)
    aju6_8000 = buscar("openmp-tuned", 8000, 6)
    aju8_8000 = buscar("openmp-tuned", 8000, 8)
    return [
        encabezado("8. Analisis y discusion"),
        encabezado("8.1 Por que fracasa un hilo por pelota", "h2"),
        parrafo(
            "El resultado mas contundente del proyecto es que la estrategia mas intuitiva "
            "&mdash;una tarea, un hilo&mdash; es la peor de todas. Conviene desarmar el numero. "
            f"Con N = 250 el paso secuencial cuesta {buscar('secuencial', 250)['min']:.3f}&nbsp;ms, "
            f"es decir unos {buscar('secuencial', 250)['min'] * 1000 / 250:.1f} microsegundos por "
            f"pelota. La version de hilos tarda {hilo250['min']:.3f}&nbsp;ms para el mismo "
            "trabajo. La diferencia no es computo: es el costo de despertar 250 hilos, hacerlos "
            "competir por un unico mutex, y volverlos a dormir, dos veces por cuadro."),
        parrafo(
            f"Ese costo escala con N mientras el trabajo por hilo permanece constante, de modo "
            f"que el speedup se estanca por debajo de 0.20x sin importar el tamano: "
            f"{hilo250['speedup']:.2f}x con N = 250 y {hilo1000['speedup']:.2f}x con "
            f"N = 1&nbsp;000. Dicho de otra forma, el programa dedica alrededor del "
            f"{100 * (1 - hilo1000['speedup']):.0f}&nbsp;% de su tiempo a coordinar hilos y el "
            "resto a simular."),
        parrafo(
            "En terminos de PCAM, el error esta en la etapa de aglomeracion: la particion en una "
            "tarea por pelota es correcta, pero mapear cada tarea a un hilo del sistema operativo "
            "salta la aglomeracion por completo. Los hilos son un recurso caro y limitado; las "
            "tareas, no. OpenMP hace exactamente lo que faltaba: agrupa muchas tareas en un "
            "bloque y le da ese bloque a un hilo de un equipo reutilizado."),
        parrafo(
            "Hay un limite adicional, mas duro: por encima de 1&nbsp;024 pelotas el sistema "
            "operativo rechaza la creacion de hilos. El programa captura la excepcion, informa "
            "en pantalla y cambia de modo en lugar de abortar, pero la conclusion es que la "
            "estrategia sencillamente no escala al rango de N que el proyecto necesita."),
        encabezado("8.2 El techo de seis hilos y los nucleos asimetricos", "h2"),
        parrafo(
            "La observacion mas interesante de las mediciones es que la eficiencia no decae de "
            "forma suave, sino que se quiebra al pasar de cuatro a seis hilos y empeora con ocho. "
            "La explicacion esta en el hardware: el Apple M1 Pro no tiene ocho nucleos iguales, "
            "sino seis de rendimiento y dos de eficiencia, y estos ultimos son sustancialmente "
            "mas lentos."),
        parrafo(
            "Con <font face='Courier'>schedule(static)</font> las iteraciones se reparten en "
            "bloques identicos antes de empezar. Si dos de los ocho bloques caen en nucleos "
            "lentos, los otros seis hilos terminan y esperan en la barrera a que los rezagados "
            "acaben. El tiempo del paso queda determinado por el hilo mas lento, y la eficiencia "
            "se desploma: es un caso de libro de desbalance de carga, solo que provocado por el "
            "hardware y no por los datos."),
        parrafo(
            f"El reparto guiado ataca justamente eso. Con N = 8&nbsp;000 y ocho hilos la version "
            f"estatica se queda en {buscar('openmp-static', 8000, 8)['speedup']:.2f}x mientras la "
            f"ajustada llega a {aju8_8000['speedup']:.2f}x, porque los nucleos rapidos siguen "
            "tomando bloques mientras los lentos terminan el suyo. Aun asi la eficiencia no "
            f"vuelve a los niveles de cuatro hilos ({aju8_8000['ef']:.2f} contra "
            f"{buscar('openmp-tuned', 8000, 4)['ef']:.2f}): agregar dos nucleos que rinden una "
            "fraccion de los otros seis nunca podra dar eficiencia 1."),
        parrafo(
            "Ahora bien, el reparto guiado no es gratis, y conviene ser preciso sobre cuando "
            "conviene, porque el resultado no es uniforme. Comparando las dos versiones celda a "
            "celda:"),
        *vinetas([
            "<b>Con dos hilos</b> son indistinguibles: las diferencias no pasan de 0.10x, dentro "
            "del ruido de medicion.",
            "<b>Con cuatro hilos</b> la estatica es igual o ligeramente mejor en las seis cargas. "
            "Tiene sentido: cuatro hilos caben de sobra en los seis nucleos rapidos, no hay "
            "desbalance que corregir, y lo unico que aporta el reparto guiado es su costo de "
            "planificacion.",
            "<b>Con ocho hilos</b>, que es cuando los dos nucleos lentos entran al equipo, la "
            "ajustada gana en las cuatro cargas de N mayor o igual a 1&nbsp;000, con una ventaja "
            f"de hasta el "
            f"{100 * (buscar('openmp-tuned', 2000, 8)['speedup'] / buscar('openmp-static', 2000, 8)['speedup'] - 1):.0f}"
            "&nbsp;%.",
            "<b>Con seis hilos</b> el resultado es mixto: la ajustada gana en el rango medio "
            "(N entre 1&nbsp;000 y 4&nbsp;000) y la estatica en los extremos.",
            "<b>Con N pequeno</b> (250 y 500) la estatica gana con cuatro, seis y ocho hilos: "
            "cada bloque de trabajo es tan corto que pedir tarea de nuevo cuesta mas que lo que "
            "se gana nivelando.",
        ]),
        parrafo(
            "La conclusion practica no es que una version sea mejor que la otra, sino que "
            "<font face='Courier'>schedule(guided)</font> es un seguro contra la heterogeneidad: "
            "se paga siempre y solo rinde cuando esa heterogeneidad existe. En un procesador "
            "homogeneo, o con cargas pequenas, el reparto estatico es la eleccion correcta."),
        parrafo(
            "Esta lectura se apoya en la dispersion de las mediciones. Las configuraciones de uno "
            "a cuatro hilos tienen desviaciones estandar minimas; las de seis y ocho muestran "
            "valores extremos aislados, que es lo que se espera cuando el planificador del "
            "sistema operativo decide, corrida a corrida, en que tipo de nucleo colocar cada "
            "hilo. Es tambien la razon por la que el informe reporta promedio, minimo, maximo y "
            "desviacion, y no solo el promedio."),
        encabezado("8.3 El tamano del problema decide si vale la pena paralelizar", "h2"),
        parrafo(
            f"Con N = 250 la version ajustada con ocho hilos alcanza un speedup de "
            f"{aju8_250['speedup']:.2f}x, es decir que practicamente no gana nada. El motivo es "
            f"que un paso secuencial cuesta solo {buscar('secuencial', 250)['min']:.3f}&nbsp;ms, "
            "y crear el equipo de hilos, repartir el trabajo y sincronizar en la barrera cuesta "
            "un orden de magnitud comparable. La regla practica que se desprende de las "
            "mediciones es que la paralelizacion empieza a rendir cuando el trabajo por paso "
            "supera aproximadamente el milisegundo, lo que en este programa ocurre entre "
            "N = 500 y N = 1&nbsp;000."),
        parrafo(
            "Ese comportamiento es la ilustracion directa de la ley de Gustafson: la fraccion "
            "secuencial del paso &mdash;preparar punteros, abrir la region, fusionar "
            "contadores&mdash; es casi constante, mientras que la fraccion paralela crece con "
            "N&sup2;. Al aumentar N la fraccion secuencial se vuelve despreciable y la eficiencia "
            "mejora, que es exactamente lo que muestra el mapa de calor de la Figura 3: la "
            "region oscura se ensancha hacia la derecha conforme se baja en la tabla."),
        encabezado("8.4 Costo y beneficio de cada iteracion", "h2"),
        parrafo(
            "Vale la pena separar cuanto aporto cada una de las tres mejoras que distinguen la "
            "version ajustada de la estatica, porque no todas rindieron lo mismo:"),
        *vinetas([
            "<b>Region paralela unica.</b> Ahorra la creacion de un equipo de hilos por sub-paso. "
            "Con dos sub-pasos por cuadro el ahorro es real pero modesto, y solo se nota con N "
            "pequeno, donde la sobrecarga pesa.",
            "<b>Reparto guiado.</b> Es la mejora que produjo casi toda la ganancia medible, pero "
            "unicamente con ocho hilos y carga alta, que es el caso en el que dos nucleos lentos "
            "se suman al equipo. Con cuatro hilos las dos versiones empatan, y con seis la "
            "estatica es incluso mejor, porque ahi no hay desbalance que corregir y el reparto "
            "guiado solo agrega costo de planificacion.",
            "<b>Contadores privatizados.</b> No mejoro el tiempo de forma medible en este "
            "programa, porque la cantidad de pelotas que llegan al fondo en un paso es pequena "
            "frente a N y la contencion sobre las operaciones atomicas era baja. Se conserva "
            "porque es la solucion correcta y porque su ventaja creceria si la piramide fuera mas "
            "corto o las pelotas cayeran mas rapido.",
        ]),
        parrafo(
            "Reportar la tercera mejora como neutra es tan util como reportar las otras dos: "
            "confirma que la contencion estaba en el reparto de la carga y no en los contadores, "
            "que era la hipotesis inicial del equipo."),
        encabezado("8.5 Limitaciones del estudio", "h2"),
        *vinetas([
            "Todas las mediciones provienen de una sola maquina. Un procesador con nucleos "
            "homogeneos deberia mostrar una curva de eficiencia mas plana hasta el total de "
            "nucleos, y probablemente una diferencia menor entre reparto estatico y guiado.",
            "El sistema operativo no estaba aislado: aunque no se ejecutaron tareas pesadas en "
            "paralelo, macOS mantiene procesos de fondo que compiten por los nucleos. Es una de "
            "las causas de la dispersion observada con seis y ocho hilos.",
            "macOS no permite fijar hilos a nucleos concretos, de modo que no fue posible probar "
            "politicas de afinidad (<font face='Courier'>OMP_PROC_BIND</font>) que en Linux "
            "habrian permitido separar el efecto del hardware del efecto del planificador.",
            "El banco de pruebas mide solo la fisica. El costo del dibujado se midio aparte "
            "&mdash;el HUD reporta ms de fisica y ms por cuadro por separado&mdash; pero no se "
            "paralelizo, porque el contexto de OpenGL no es seguro para multiples hilos.",
        ]),
    ]


def seccion_conclusiones():
    return [
        encabezado("9. Conclusiones"),
        *vinetas([
            "La paralelizacion con OpenMP acelero el nucleo de simulacion en todos los tamanos "
            f"probados, alcanzando un speedup maximo de <b>{MEJOR['speedup']:.2f}x</b> con "
            f"{MEJOR['hilos']} hilos y N = {MEJOR['n']:,} pelotas".replace(",", " ") +
            f" (eficiencia {MEJOR['ef']:.2f}), y una eficiencia superior a 0.85 de forma "
            "sostenida con dos y cuatro hilos siempre que N sea mayor o igual a 2&nbsp;000.",

            "Asignar un hilo del sistema operativo por cada elemento de la simulacion es una "
            "estrategia perdedora: resulto mas de cinco veces mas lenta que la version "
            "secuencial y deja de ser viable por encima de 1&nbsp;024 elementos. El costo de "
            "coordinar hilos crece con la cantidad de tareas mientras el trabajo por tarea "
            "permanece constante.",

            "La etapa de aglomeracion del metodo PCAM no es un tramite: es exactamente la etapa "
            "que separa la version que funciona de la que no. Agrupar muchas pelotas en un bloque "
            "y darle ese bloque a un hilo reutilizado es lo unico que cambia entre ambas.",

            "Disenar el estado con doble buffer &mdash;un arreglo de solo lectura y otro con un "
            "unico escritor por indice&mdash; elimina las condiciones de carrera por "
            "construccion, sin costo de sincronizacion, y ademas hace la simulacion determinista. "
            "Ese determinismo es lo que permitio verificar la correccion con pruebas automatizadas "
            "que comparan bit a bit el resultado de los cuatro modos.",

            "La eleccion de la politica de reparto importa mas que la cantidad de directivas, "
            "pero no existe una que gane siempre. En un procesador con nucleos asimetricos, "
            "<font face='Courier'>schedule(guided)</font> recupero una parte sustancial del "
            "rendimiento que <font face='Courier'>schedule(static)</font> perdia por desbalance "
            "cuando el equipo incluia los dos nucleos lentos y habia trabajo suficiente que "
            "repartir; en cambio, con cargas pequenas el reparto estatico gano en todas las "
            "cantidades de hilos, porque ahi el costo de planificacion del reparto guiado no "
            "alcanza a amortizarse.",

            "El beneficio de paralelizar depende del tamano del problema. Por debajo de unas 500 "
            "pelotas la sobrecarga de la region paralela consume la ganancia; a partir de ahi la "
            "eficiencia crece de forma sostenida con N, tal como describe la ley de Gustafson.",

            "En terminos del objetivo del screensaver, la paralelizacion permitio duplicar con "
            "holgura la cantidad de elementos que se pueden simular manteniendo 60 cuadros por "
            "segundo: la version secuencial pierde el presupuesto entre 2&nbsp;000 y 4&nbsp;000 "
            "pelotas, mientras que la version ajustada con ocho hilos todavia lo cumple con "
            "4&nbsp;000.",
        ]),
        encabezado("10. Recomendaciones"),
        *vinetas([
            "<b>Reducir la complejidad antes de paralelizar mas.</b> El siguiente salto de "
            "rendimiento no vendra de mas hilos sino de un mejor algoritmo: una rejilla espacial "
            "uniforme reduciria las colisiones de O(N&sup2;) a O(N) esperado. Paralelizar un "
            "algoritmo cuadratico da un factor constante; cambiar el algoritmo cambia la "
            "pendiente.",

            "<b>Repetir las mediciones en hardware homogeneo.</b> Buena parte del analisis de "
            "este informe gira en torno a la asimetria del M1 Pro. Medir en un procesador con "
            "nucleos iguales permitiria separar lo que es propio del programa de lo que es propio "
            "de la maquina.",

            "<b>Explorar politicas de afinidad.</b> En un sistema que lo permita, "
            "<font face='Courier'>OMP_PROC_BIND</font> y "
            "<font face='Courier'>OMP_PLACES</font> permitirian restringir el equipo a los "
            "nucleos de rendimiento y comprobar la hipotesis de la seccion 8.2 de forma directa.",

            "<b>Ajustar el tamano de bloque.</b> Se probaron los repartos estatico y guiado con "
            "sus valores por omision. Un barrido de <font face='Courier'>schedule(dynamic, k)</font> "
            "para distintos k podria encontrar un punto intermedio entre el costo de "
            "planificacion y el balance de carga.",

            "<b>No paralelizar el dibujado, sino reducirlo.</b> El contexto de OpenGL no admite "
            "multiples hilos, pero el renderizado ya se resolvio con un cuadrilatero texturizado "
            "por elemento en lugar de una malla de esfera. Si el dibujado volviera a ser el cuello "
            "de botella, el camino es el instanciado o un buffer de vertices, no mas hilos.",

            "<b>Conservar la version lenta en el repositorio.</b> Mantener la implementacion de un "
            "hilo por pelota junto a las de OpenMP tiene valor didactico y experimental: es el "
            "contraejemplo que da sentido a las demas y permite reproducir la comparacion en "
            "cualquier maquina.",
        ]),
    ]


def seccion_bibliografia():
    contenido = [encabezado("11. Bibliografia"),
                 parrafo("Las referencias siguen el formato APA. Se listan unicamente las fuentes "
                         "efectivamente consultadas durante el desarrollo del proyecto.")]
    estilo = ParagraphStyle("ref", parent=E["cuerpo"], leftIndent=20, firstLineIndent=-20,
                            spaceAfter=7)
    for referencia in BIBLIOGRAFIA:
        contenido.append(Paragraph(referencia, estilo))
    return contenido


def anexo_1():
    return [
        PageBreak(),
        encabezado("Anexo 1. Diagrama de flujo del programa"),
        parrafo(
            "El diagrama recorre el programa completo, desde la captura de argumentos hasta la "
            "liberacion de recursos. Se presenta en dos partes unidas por el conector "
            "<b>A</b>. Las formas siguen la convencion habitual: ovalo para inicio y fin, "
            "rectangulo para proceso, rombo para decision, paralelogramo para entrada y salida, y "
            "rectangulo con doble borde para las secciones que se ejecutan en paralelo. Los "
            "recuadros del grupo 7 indican de forma explicita el mecanismo de sincronia que usa "
            "cada version."),
        parrafo(
            "El archivo fuente del diagrama esta en "
            "<font face='Courier'>docs/graficas/diagrama_flujo.dot</font> y hay tambien una "
            "version vectorial en PDF, que permite ampliar sin perdida."),
        PageBreak(),
        *figura(os.path.join(GRAFICAS, "diagrama_flujo_p1.png"),
                "Diagrama de flujo, parte 1 de 2: arranque, captura de argumentos con "
                "programacion defensiva, solicitud de ingreso de datos, validacion de rangos y "
                "modo de banco de pruebas.", maximo_alto=20.5 * cm),
        PageBreak(),
        *figura(os.path.join(GRAFICAS, "diagrama_flujo_p2.png"),
                "Diagrama de flujo, parte 2 de 2: creacion de la ventana y de la escena, ciclo "
                "principal, secciones paralelas con sus mecanismos de sincronia, despliegue de "
                "resultados y liberacion ordenada de recursos.", maximo_alto=20.5 * cm),
    ]


def anexo_2():
    contenido = [
        PageBreak(),
        encabezado("Anexo 2. Catalogo de funciones"),
        parrafo(
            "El catalogo cubre las funciones, clases y estructuras relevantes del proyecto, "
            "agrupadas por archivo. Para cada una se indican sus entradas con nombre y tipo, sus "
            "salidas y una descripcion de su proposito y de su funcionamiento. Se incluyen "
            "tambien las funciones internas de cada unidad de traduccion cuando resultan "
            "necesarias para entender el diseno."),
        Spacer(1, 4),
    ]

    encabezados = [Paragraph(h, E["encabezado"]) for h in
                   ["Funcion / tipo", "Entradas", "Salidas", "Descripcion"]]
    archivo_actual = None
    filas = []

    def cerrar_bloque(archivo, filas_bloque):
        if not filas_bloque:
            return []
        return [
            Paragraph(f"<font face='Courier'>{archivo}</font>", E["h3"]),
            tabla([encabezados] + filas_bloque,
                  [4.5 * cm, 3.5 * cm, 3.0 * cm, ANCHO_UTIL - 11.0 * cm],
                  alineacion_centro=False),
            Spacer(1, 6),
        ]

    for archivo, firma, entradas, salidas, descripcion in CATALOGO:
        if archivo != archivo_actual:
            contenido += cerrar_bloque(archivo_actual, filas)
            filas = []
            archivo_actual = archivo
        filas.append([
            Paragraph(f"<font face='Courier' size='7'>{firma}</font>", E["celda"]),
            Paragraph(entradas, E["celda"]),
            Paragraph(salidas, E["celda"]),
            Paragraph(descripcion, E["celda"]),
        ])
    contenido += cerrar_bloque(archivo_actual, filas)
    return contenido


def anexo_3():
    contenido = [
        PageBreak(),
        encabezado("Anexo 3. Bitacora de pruebas"),
        parrafo(
            "Se realizaron 72 configuraciones distintas, combinando seis valores de N, cuatro "
            "modos de ejecucion y hasta cinco cantidades de hilos. Cada configuracion se midio "
            "<b>12 veces</b>, dos por encima del minimo de diez que exige el enunciado, lo que da "
            "un total de 828 mediciones individuales. Todas ellas estan en el archivo "
            "<font face='Courier'>docs/resultados/benchmark_muestras.csv</font>; las cifras "
            "agregadas estan en <font face='Courier'>docs/resultados/benchmark.csv</font> y la "
            "salida completa del banco en "
            "<font face='Courier'>docs/resultados/bitacora_benchmark.txt</font>."),
        parrafo(
            "El comando que reproduce exactamente la corrida reportada es el siguiente:"),
        Paragraph(
            "./build/plinko3d --benchmark --bench-balls 250,500,1000,2000,4000,8000 \\<br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;--bench-threads 1,2,4,6,8 --bench-reps 12 --bench-steps 240 "
            "\\<br/>&nbsp;&nbsp;&nbsp;&nbsp;--bench-out docs/resultados/benchmark.csv",
            E["mono"]),
        Spacer(1, 8),
        encabezado("A3.1 Resultados por version", "h2"),
    ]

    contenido += tabla_por_modo(
        "openmp-static",
        "Version OpenMP con reparto estatico. Cada celda muestra el tiempo promedio por paso, el "
        "speedup contra la version secuencial del mismo N y la eficiencia E.")
    contenido += tabla_por_modo(
        "openmp-tuned",
        "Version OpenMP ajustada (region unica, reparto guiado y contadores privatizados).")

    # Tabla de la version secuencial y de la de hilos.
    encabezados = [Paragraph(h, E["encabezado"]) for h in
                   ["N", "Pasos por<br/>repeticion", "Secuencial<br/>promedio (ms)",
                    "Secuencial<br/>desv. est.", "std::thread<br/>promedio (ms)",
                    "std::thread<br/>speedup", "std::thread<br/>eficiencia"]]
    filas = [encabezados]
    for n in VALORES_N:
        base = buscar("secuencial", n)
        hilo = buscar("std::thread", n)
        viable = hilo is not None and hilo["ok"]
        filas.append([
            Paragraph(f"{n:,}".replace(",", " "), E["celda_c"]),
            Paragraph(str(base["pasos"]), E["celda_c"]),
            Paragraph(f"{base['min']:.4f}", E["celda_c"]),
            Paragraph(f"{base['sd']:.4f}", E["celda_c"]),
            Paragraph(f"{hilo['min']:.4f}" if viable else "no viable", E["celda_c"]),
            Paragraph(f"{hilo['speedup']:.3f}x" if viable else "&mdash;", E["celda_c"]),
            Paragraph(f"{hilo['ef']:.5f}" if viable else "&mdash;", E["celda_c"]),
        ])
    contenido.append(titulo_tabla(
        "Version secuencial de referencia y version con un std::thread por pelota, con el mejor "
        "tiempo de las 12 repeticiones. La eficiencia de esta ultima se calcula sobre N hilos, "
        "que es la cantidad que realmente crea."))
    contenido.append(tabla(filas, [ANCHO_UTIL / 7] * 7))
    contenido.append(Paragraph(
        "Las celdas marcadas como &ldquo;no viable&rdquo; corresponden a valores de N por encima "
        "de 1&nbsp;024, donde el sistema operativo rechaza la creacion de un hilo por pelota. El "
        "programa detecta la condicion, la informa y continua con otro modo.", E["pie_tabla"]))

    contenido += [
        PageBreak(),
        encabezado("A3.2 Mediciones individuales", "h2"),
        parrafo(
            "Se reproducen a continuacion las doce repeticiones de cada configuracion para dos "
            "valores de N representativos: uno pequeno, donde la sobrecarga domina, y uno grande, "
            "donde la paralelizacion rinde. Los tiempos estan en milisegundos por paso de fisica."),
    ]

    for n in (500, 8000):
        configuraciones = [("secuencial", 1)]
        if buscar("std::thread", n) and buscar("std::thread", n)["ok"]:
            configuraciones.append(("std::thread", n))
        for modo in ("openmp-static", "openmp-tuned"):
            for h in (1, 2, 4, 6, 8):
                if buscar(modo, n, h):
                    configuraciones.append((modo, h))

        enc_min = ParagraphStyle("enc_min", parent=E["encabezado"], fontSize=6.3, leading=8)
        encabezados = [Paragraph("Configuracion", enc_min)]
        encabezados += [Paragraph(f"R{i}", enc_min) for i in range(1, 13)]
        encabezados += [Paragraph("Min.", enc_min), Paragraph("Desv.", enc_min)]
        filas = [encabezados]
        for modo, hilos in configuraciones:
            muestras = MUESTRAS.get((n, modo, hilos), [])
            if not muestras:
                continue
            f = buscar(modo, n, hilos)
            etiqueta = NOMBRE_MODO[modo]
            if modo != "secuencial":
                etiqueta += f" x{hilos}"
            fila = [Paragraph(etiqueta, E["celda_min_izq"])]
            for valor in muestras[:12]:
                fila.append(Paragraph(ms(valor), E["celda_min"]))
            fila.append(Paragraph(f"<b>{ms(f['min'])}</b>", E["celda_min"]))
            fila.append(Paragraph(ms(f["sd"]), E["celda_min"]))
            filas.append(fila)

        contenido.append(titulo_tabla(
            f"Las 12 repeticiones de cada configuracion con N = {n:,}".replace(",", " ") +
            ". R1 a R12 son las repeticiones individuales en milisegundos por paso; la columna "
            "Min. es el estimador que usa el informe."))
        ancho_etiqueta = 3.1 * cm
        resto = (ANCHO_UTIL - ancho_etiqueta) / 14
        contenido.append(tabla(filas, [ancho_etiqueta] + [resto] * 14, tamano=6.4))
        contenido.append(Spacer(1, 8))

    contenido += figura(os.path.join(GRAFICAS, "fig5_dispersion.png"),
                        "Captura grafica de las mediciones con N = 4 000: cada punto es una de "
                        "las doce repeticiones y la barra negra marca el promedio.")

    contenido += [
        encabezado("A3.3 Captura de la salida del banco de pruebas", "h2"),
        parrafo(
            "Se reproduce un fragmento textual de la salida por consola del banco de pruebas, "
            "correspondiente a la carga mayor evaluada."),
    ]

    ruta_log = os.path.join(RESULTADOS, "bitacora_benchmark.txt")
    if os.path.exists(ruta_log):
        with open(ruta_log) as archivo:
            lineas = archivo.read().splitlines()
        inicio = next((i for i, l in enumerate(lineas) if l.startswith("== N = 8000")), 0)
        fragmento = lineas[inicio:inicio + 13]
        texto = "<br/>".join(l.replace(" ", "&nbsp;") for l in fragmento)
        contenido.append(Table(
            [[Paragraph(texto, E["mono"])]], colWidths=[ANCHO_UTIL],
            style=TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#f5f7fa")),
                              ("BOX", (0, 0), (-1, -1), 0.5, LINEA),
                              ("LEFTPADDING", (0, 0), (-1, -1), 8),
                              ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                              ("TOPPADDING", (0, 0), (-1, -1), 8),
                              ("BOTTOMPADDING", (0, 0), (-1, -1), 8)])))
        contenido.append(Paragraph(
            "Fragmento de <font face='Courier'>docs/resultados/bitacora_benchmark.txt</font>.",
            E["pie_tabla"]))

    contenido += [
        encabezado("A3.4 Resultado de las pruebas automatizadas", "h2"),
        parrafo(
            "Ademas de las mediciones de rendimiento, la entrega incluye cuatro suites de pruebas "
            "de correccion que se ejecutan con "
            "<font face='Courier'>ctest --test-dir build --output-on-failure</font>."),
    ]
    ruta_ctest = os.path.join(RESULTADOS, "salida_ctest.txt")
    if os.path.exists(ruta_ctest):
        with open(ruta_ctest) as archivo:
            lineas = [l for l in archivo.read().splitlines() if l.strip()][-14:]
        texto = "<br/>".join(l.replace(" ", "&nbsp;") for l in lineas)
        contenido.append(Table(
            [[Paragraph(texto, E["mono"])]], colWidths=[ANCHO_UTIL],
            style=TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#f5f7fa")),
                              ("BOX", (0, 0), (-1, -1), 0.5, LINEA),
                              ("LEFTPADDING", (0, 0), (-1, -1), 8),
                              ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                              ("TOPPADDING", (0, 0), (-1, -1), 8),
                              ("BOTTOMPADDING", (0, 0), (-1, -1), 8)])))
        contenido.append(Paragraph("Salida de CTest en la version entregada.", E["pie_tabla"]))
    return contenido


def anexo_4():
    """Material suplementario: capturas del programa en ejecucion."""
    contenido = [
        PageBreak(),
        encabezado("Anexo 4. El programa en ejecucion"),
        parrafo(
            "Las capturas siguientes se tomaron con la opcion "
            "<font face='Courier'>--screenshot</font> del propio programa, que vuelca el "
            "framebuffer despues de un numero fijo de cuadros. Eso las hace reproducibles: la "
            "misma semilla y el mismo numero de cuadros producen la misma imagen."),
        parrafo(
            "El HUD de la esquina superior izquierda es el despliegue de resultados en vivo que "
            "pide el enunciado. Muestra los cuadros por segundo &mdash;en verde por encima de 58, "
            "en ambar entre 30 y 58 y en rojo por debajo&mdash;, el modo de ejecucion activo, N, "
            "la cantidad de hilos, el tiempo de fisica y el tiempo total por cuadro, el speedup "
            "estimado contra el modo secuencial y el total de pelotas recicladas. Las barras "
            "magenta que rodean la base son el histograma de los sectores contadores."),
    ]
    capturas = [
        ("piramide_frontal.png",
         "Configuracion por omision: N = 3 000 pelotas cayendo sobre una piramide de 277 "
         "clavijas repartidas en 12 niveles, con 24 sectores contadores en la base. Las clavijas "
         "grandes en tonos frios forman la estructura; las ambar son las que oscilan."),
        ("piramide_girada.png",
         "La misma escena unos segundos despues. La camara ha girado alrededor del eje vertical, "
         "lo que revela la forma conica y la corona de sectores. El giro es puramente visual y no "
         "interviene en la fisica."),
        ("piramide_estructura.png",
         "La misma piramide con solo 300 pelotas, para que se aprecie la estructura: los aros que "
         "unen las clavijas de cada nivel y las aristas que van del vertice a la base."),
    ]
    for archivo, pie in capturas:
        ruta = os.path.join(CAPTURAS, archivo)
        if os.path.exists(ruta):
            contenido += figura(ruta, pie, ancho=ANCHO_UTIL)
    return contenido


# ===========================================================================
def main():
    documento = InformeDocTemplate(
        SALIDA, pagesize=letter,
        title="Plinko 3D Paralelo - Proyecto 1 - Computacion Paralela y Distribuida",
        author="Ian Cumes, Javier Valladares, Nery Molina",
        subject="Paralelizacion con OpenMP de un screensaver con simulacion fisica")

    historia = []
    historia += construir_portada()
    historia.append(NextPageTemplate("contenido"))
    historia.append(PageBreak())
    historia += construir_indice()
    historia.append(PageBreak())
    historia += seccion_introduccion()
    historia += seccion_antecedentes()
    historia += seccion_objetivos()
    historia += seccion_marco_teorico()
    historia += seccion_metodologia()
    historia.append(PageBreak())
    historia += seccion_diseno()
    historia.append(PageBreak())
    historia += seccion_resultados()
    historia.append(PageBreak())
    historia += seccion_analisis()
    historia.append(PageBreak())
    historia += seccion_conclusiones()
    historia += seccion_bibliografia()
    historia += anexo_1()
    historia += anexo_2()
    historia += anexo_3()
    historia += anexo_4()

    # multiBuild es necesario para que el indice conozca los numeros de pagina.
    documento.multiBuild(historia)
    print(f"informe generado: {os.path.relpath(SALIDA, BASE)}")


if __name__ == "__main__":
    main()
