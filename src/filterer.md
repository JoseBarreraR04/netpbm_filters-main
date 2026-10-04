# Diseño 2: Versión Secuencial (filterer)

Aplicación modular en C++ orientada a objetos (OOP) para aplicar filtros de convolución 2D a imágenes en formato **PGM (P2 - escala de grises)** y **PPM (P3 - color)**.

## Características

- **Paradigma Orientado a Objetos (OOP)**: Arquitectura desacoplada en clases `Image` y jerarquía de filtros (`Filter` y `ConvolutionFilter`).
- **Restricciones de Rendimiento y Memoria**: 
  - Uso estricto de arreglos dinámicos en memoria contigua (`int* pixels`) en lugar de `std::vector`.
  - Manipulación de cadenas mediante punteros y funciones estándar de C (`char*`, `const char*`) sin usar `std::string`.
- **Filtros Soportados (Kernels 3x3)**:
  - `blur`: Filtro de suavizado / media (kernel 3x3 normalizado con divisor 9).
  - `sharpen`: Filtro de realce de bordes (kernel 3x3 con centro 5).
  - `laplace`: Detección de bordes laplaciano de 8 vecinos (kernel 3x3 con centro 8 y vecinos -1), con clamping directo a `[0, max_color]`.
- **Tratamiento de Bordes**: *Zero-padding* (cualquier coordenada fuera de los límites de la imagen se asume con valor 0).
- **Encadenamiento de Múltiples Filtros**: Soporte para aplicar múltiples filtros en cascada en una sola ejecución.
- **Medición de Tiempos**: Medición de alta resolución tanto de tiempo de CPU (`std::clock`) como tiempo total de reloj (`std::chrono::high_resolution_clock`), impreso en consola y registrado automáticamente en `tiempos.csv`.

---

## Compilación

Compilación directa con optimizaciones `-O2` y flags de advertencia habilitados:

```bash
g++ -O2 -Wall -Wextra -I headers src/Image.cpp src/Filter.cpp src/filterer.cpp -o filterer
```

En Windows con MinGW:
```powershell
g++ -O2 -Wall -Wextra -I headers src/Image.cpp src/Filter.cpp src/filterer.cpp -o filterer.exe
```

---

## Sintaxis de Ejecución

```bash
./filterer <imagen_entrada> <imagen_salida> --f <filtro1> [--f <filtro2> ...]
# o alternativamente con lista separada por comas:
./filterer <imagen_entrada> <imagen_salida> --f <filtro1,filtro2,...>
```

### Ejemplos

1. **Aplicar filtro blur a imagen PPM:**
   ```bash
   ./filterer images/fruit.ppm images/fruit_blur.ppm --f blur
   ```

2. **Aplicar filtro sharpen a imagen PGM:**
   ```bash
   ./filterer images/lena.pgm images/lena_sharpen.pgm --f sharpen
   ```

3. **Aplicar detección de bordes (laplace) a imagen PPM:**
   ```bash
   ./filterer images/fruit.ppm images/fruit_laplace.ppm --f laplace
   ```

4. **Encadenar múltiples filtros (blur y luego sharpen):**
   ```bash
   ./filterer images/puj.ppm images/puj_blur_sharpen.ppm --f blur,sharpen
   # o también:
   ./filterer images/puj.ppm images/puj_blur_sharpen.ppm --f blur --f sharpen
   ```

---

## Registro de Métricas (`tiempos.csv`)

Cada ejecución registra automáticamente en el archivo `tiempos.csv` las siguientes columnas:
- `fecha_hora`: Marca temporal de ejecución.
- `archivo_entrada`: Ruta de la imagen procesada.
- `formato`: Formato detectado (`PGM (P2)` o `PPM (P3)`).
- `ancho` / `alto`: Dimensiones en píxeles.
- `canales`: Cantidad de canales (1 para gris, 3 para RGB).
- `filtros`: Lista de filtros aplicados en orden.
- `tiempo_cpu_s`: Tiempo de CPU utilizado en segundos.
- `tiempo_total_s`: Tiempo total de ejecución (Wall-clock) en segundos.