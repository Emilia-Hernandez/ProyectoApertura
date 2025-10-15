# Proyecto Apertura

### DBSCAN paralelo para la detección de ruido (outliers) con OpenMP

**Autora:** Emilia Hernández Medina  
**Curso:** Cómputo Paralelo y en la Nube  
**Fecha:** Octubre 2025

---

## Descripción general

Este proyecto implementa versiones **serial y paralelas** del algoritmo **DBSCAN (Density-Based Spatial Clustering of Applications with Noise)** para la detección de puntos de ruido u outliers en conjuntos de datos bidimensionales.

La implementación fue realizada en **C++17** utilizando **OpenMP** para la paralelización.

Se desarrollaron tres versiones del algoritmo:

- **Versión serial:** implementación base secuencial.
- **Versión paralela P1:** paralelización directa sobre todo el conjunto de datos (espacio indivisible).
- **Versión paralela P2:** paralelización por subespacios (división espacial en bloques).

El objetivo principal fue analizar y comparar el desempeño de estas estrategias de paralelización bajo distintos tamaños de datos y configuraciones de hilos.

---

## Estructura del repositorio

```
ProyectoApertura/
│
├── data/                        # Archivos CSV de entrada (raw) y salida (processed)
│   ├── *_data.csv              # Datos de entrada (coordenadas x,y)
│   ├── *_CR.csv                # Datos procesados con etiquetas core/outlier
│
├── DBSCAN_noise.ipynb          # Libreta para visualizar resultados y generar datasets
├── results.ipynb               # Análisis de resultados y generación de gráficas
├── p1.cpp                      # Código fuente principal con versiones serial, P1 y P2
├── resultados_benchmarks.csv   # Registro de tiempos y speedups promediados
└── ReportePoryecto1.docx       # Reporte completo con metodología y análisis
```

---

## Implementación y estrategias de paralelización

### Bibliotecas utilizadas

```cpp
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <filesystem>
#include <iomanip>
```

- **OpenMP (omp.h)** → control de paralelización.
- **fstream, sstream, filesystem** → manejo de archivos CSV.
- **vector y set** → estructuras dinámicas de almacenamiento.

### Versiones del algoritmo

- **Serial:** Clasifica los puntos como core u outlier calculando distancias euclidianas.
- **P1:** Paraleliza los bucles principales sobre el conjunto completo de puntos.
- **P2:** Divide el espacio en submatrices y paraleliza el cálculo dentro de cada bloque.

Cada versión genera un archivo CSV etiquetando los puntos y midiendo los tiempos de ejecución.

Los resultados se registran en `resultados_benchmarks.csv` junto con los speedups calculados.

---

## Diseño experimental

### Parámetros del experimento

- **Tamaños de dataset:** {20000, 40000, 80000, 120000, 140000, 160000, 180000, 200000}
- **Número de hilos:** {1, 5, 11, 22}
- **Iteraciones:** 10 repeticiones por configuración para obtener promedios confiables.
- **Épsilon (ε):** 0.05
- **minPts:** 5

Los resultados se analizaron con Python (pandas, matplotlib) desde `results.ipynb`.

---

## Resultados y análisis

- La versión **P1** obtuvo los mejores speedups en todos los escenarios, alcanzando valores de hasta **7.3×** con 22 hilos.
- La versión **P2**, aunque estable, presentó una mejora limitada (**≈3×**), debido al overhead al dividir el espacio en bloques.
- El rendimiento aumentó significativamente hasta cierto punto (**≈11 hilos**), estabilizándose después por saturación de los núcleos virtuales.

### Conclusión:

La paralelización global (P1) aprovechó mejor los recursos del chip M3 Pro, mostrando mejor escalabilidad y eficiencia frente a la versión segmentada (P2).
