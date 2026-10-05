Una aplicación en C++ que aplica filtros a imágenes en formatos PGM (escala de grises) y PPM (color).

# Diseño 4: memoria distribuida con MPI

La imagen se divide en **bandas horizontales de filas** y cada nodo de MPI recibe
una. Como el kernel es 3x3, para filtrar la primera y la última fila de su banda
un nodo necesita una fila de halo por encima y otra por debajo, que le pide a
sus vecinos inmediatos con `MPI_Sendrecv`.

Con una imagen de 1000x1278 y 4 nodos:

```txt
nodo 0 -> y:[   0, 320)   320 filas    halo de abajo: nodo 1
nodo 1 -> y:[ 320, 640)   320 filas    halo de arriba: nodo 0   halo de abajo: nodo 2
nodo 2 -> y:[ 640, 959)   319 filas    halo de arriba: nodo 1   halo de abajo: nodo 3
nodo 3 -> y:[ 959,1278)   319 filas    halo de arriba: nodo 2
```

Se eligen bandas horizontales y no cuadrantes (como el Diseño 3) porque así cada
píxel de un borde sólo depende de la fila inmediatamente superior o inferior, que
pertenece a un único vecino. En cuadrantes haría falta además el vecino diagonal
de la esquina, y el intercambio pasaría de 2 vecinos a 8.

## Cómo se ejecuta

Todo pasa por Docker, así que no hace falta tener MPI instalado en el equipo.

```bash
# 1. Construir la imagen (una sola vez)
docker compose build

# 2. Comprobar que todo funciona
docker compose run --rm app selftest

# 3. Ver una demostración con 4 nodos
docker compose run --rm app demo

# 4. Filtrar una imagen concreta
docker compose run --rm app mpi_filterer images/damma.pgm /out/damma_blur.pgm --f blur
```

Sin `docker compose`, montando el repositorio a mano:

```bash
docker run --rm -v "${PWD}:/app" -v "${PWD}/out:/out" -w /app \
  netpbm-filters-d4-mpi mpi_filterer images/damma.pgm /out/damma_blur.pgm --f blur
```

El número de nodos se cambia con la variable de entorno `NODES`:

```bash
docker compose run --rm -e NODES=2 app mpi_filterer images/damma.pgm /out/salida.pgm --f blur
```

## Filtros

| Filtro         | Kernel 3x3                                | Notas                          |
| -------------- | ----------------------------------------- | ------------------------------ |
| `blur`         | `1/9` en las 9 posiciones                  | renormaliza en los bordes      |
| `laplace`      | `0 -1 0 / -1 4 -1 / 0 -1 0`               | valor absoluto                 |
| `sharpening`   | `0 -1 0 / -1 5 -1 / 0 -1 0`               | valor absoluto                 |

Las tres operaciones son las mismas del Diseño 3, así que las imágenes de los dos
diseños se pueden comparar byte a byte.

## Formato de los archivos

Entrada y salida pueden ser P2 (gris) o P3 (color). La salida conserva el número
mágico de la entrada. También se aceptan comentarios `#` en cualquier punto entre
valores, como permite el formato netpbm.

Entrada por la entrada estándar: si se da una sola ruta, esa es la salida y la
imagen de entrada se lee de `stdin`.

```bash
docker compose run --rm app mpi_filterer /out/salida.pgm --f blur < images/damma.pgm
```

> **Aviso:** este modo funciona, pero dentro de `mpiexec` el reenvío de `stdin`
> de hydra (MPICH) rompe en este entorno: mata cualquier hijo que lea la entrada
> estándar, y se reproduce sin este proyecto con
> `mpiexec -n 1 sh -c 'cat > /dev/null; sleep 3' < fichero`, que también muere con
> código 141. Ejecutado sin `mpiexec` el mismo binario da la imagen correcta byte
> a byte. Por eso la prueba 7 de la suite lanza el binario directamente.

## Tiempos

Cada nodo mide el tiempo total (`CLOCK_MONOTONIC`) y el de CPU
(`CLOCK_PROCESS_CPUTIME_ID`) alrededor del filtrado de su banda, y el nodo 0
imprime la tabla con el reparto de bandas:

```txt
Filtro: blur | Nodos MPI: 4 | Imagen: P2 1000x1278 | Canales: 1

Reparto en bandas horizontales:
  nodo 0 -> y:[   0, 320)   320 filas
  ...

Tiempo por nodo (solo el filtrado de su banda):
  nodo 0 : total    7.368 ms | CPU    7.366 ms
  nodo 1 : total    8.947 ms | CPU    8.942 ms
  nodo 2 : total    8.215 ms | CPU    8.212 ms
  nodo 3 : total   11.080 ms | CPU   11.076 ms
Tiempo total del filtrado : 0.011080 s (11.080 ms)  [nodo mas lento]
Tiempo de CPU del filtrado : 0.035596 s (35.596 ms)  [suma de los 4 nodos]
```

El tiempo total que se reporta es el del nodo más lento, que es lo que marca la
duración real de la fase distribuida; la suma de CPU mide el trabajo total
aprovechando los 4 núcleos.

## Pruebas

`docker compose run --rm app selftest` lanza 76 pruebas y debe terminar con
`0 fallidas`. Lo importante es que `tests/ref_filter.cpp`, un filtro secuencial
de un solo hilo, sirve de oráculo: si la versión con MPI y la secuencial dan la
misma imagen byte a byte, el reparto en bandas y el intercambio de halos están
bien. Se compara byte a byte, sin tolerancia numérica, porque ambas versiones
hacen exactamente la misma aritmética.

Se comprueba, entre otras cosas:

- los tres filtros sobre las imágenes del proyecto;
- que el resultado es idéntico con 1, 2, 3, 4, 5 y 8 nodos;
- alturas impares, que obligan a unas bandas a tener una fila más que otras;
- imágenes más bajas que el número de nodos, donde hay nodos sin filas;
- P3 a color, cabeceras con comentarios, entrada por `stdin` y códigos de error.

## Archivos

| Archivo                   | Para qué sirve                                        |
| ------------------------- | ----------------------------------------------------- |
| `src/mpi_filterer.cpp`    | el programa distribuido                               |
| `headers/image.h`         | lectura y escritura de P2 y P3                        |
| `headers/filter.h`        | los tres kernels y el filtrado de una banda           |
| `headers/band.h`          | el reparto en bandas y la etiqueta de cada una        |
| `tests/ref_filter.cpp`    | filtro secuencial, oráculo de las pruebas             |
| `tests/selftest.sh`       | la suite                                              |
| `docker/entrypoint.sh`    | compila y lanza con el número de nodos que se le pase |

## Clonar o descargar el código

```bash
git clone https://github.com/japeto/netpbm_filters/tree/main
cd netpbm_filters
```

## `processor`: el programa original

Este es el programa secuencial de una sola máquina, y el punto de partida del ejercicio.

```bash
g++ -O2 -o processor processor.cpp
./processor images/lena.ppm images/lena2.ppm
```

## Ejemplos de archivos de entrada

Ejemplo PGM (P2):

```txt
P2
3 2
255
255 0 0
0 255 0
0 0 255
```

Ejemplo PPM (P3):

```txt
P3
2 2
255
255 0 0   0 255 0
0 0 255   255 255 0
```

## Recursos adicionales

### Formatos de imagen

- [Especificación formato PPM](http://netpbm.sourceforge.net/doc/ppm.html)
- [Especificación formato PGM](http://netpbm.sourceforge.net/doc/pgm.html)
- [Wikipedia - Netpbm format](https://en.wikipedia.org/wiki/Netpbm)

### Procesamiento de imágenes

- [Kernels de convolución](https://en.wikipedia.org/wiki/Kernel_(image_processing))
- [Filtros de imagen - OpenCV docs](https://docs.opencv.org/4.x/d4/d13/tutorial_py_filtering.html)
- [Image Processing Fundamentals](https://imageprocessingplace.com/)
- [MPI - Message Passing Interface](https://www.mpi-forum.org/)