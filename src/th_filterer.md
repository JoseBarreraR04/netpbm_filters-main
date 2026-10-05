Este programa en C++ aplica filtros a imágenes en formato PGM (escala de grises, P2) y PPM (color, P3).

## Diseño 3: Memoria compartida

La imagen de entrada se divide en cuatro regiones (arriba-izquierda, arriba-derecha, abajo-izquierda y abajo-derecha) y cada región se le entrega a un hilo (`std::thread`) junto con el filtro que debe aplicar. Los cuatro hilos escriben sobre un único buffer de salida en rangos disjuntos, por lo que no hace falta sincronización: la imagen de entrada solo se lee y la de salida solo se escribe en posiciones que no se solapan.

Filtros disponibles: `blur` (suavizado), `laplace` y `sharpening` (realce).

## Compilacion

```bash
g++ -std=c++11 -O2 -Wall -Wextra -pthread -o ../th_filterer th_filterer.cpp
```

## Ubicacion

```bash
cd ..
```

## Ejecucion

```bash
# con rutas de archivos
./th_filterer images/damma.pgm /tmp/damma_blur.pgm --f blur
./th_filterer images/sulfur.pgm /tmp/sulfur_sharpening.pgm --f sharpening

# cuando solo se pasa la salida, la entrada se lee desde stdin
./th_filterer /tmp/lena_blur.pgm --f blur < images/lena.pgm
```

## Mediciones

Después de filtrar, el programa imprime el reparto de regiones y los tiempos del paso de filtrado (tiempo total con `CLOCK_MONOTONIC` y tiempo de CPU con `CLOCK_PROCESS_CPUTIME_ID`):

```
Filtro: blur | Hilos: 4 | Imagen: P2 1000x1278 | Regiones: 500x639
  hilo 0 -> arriba-izquierda x:[0,500) y:[0,639)
  hilo 1 -> arriba-derecha   x:[500,1000) y:[0,639)
  hilo 2 -> abajo-izquierda  x:[0,500) y:[639,1278)
  hilo 3 -> abajo-derecha    x:[500,1000) y:[639,1278)
Tiempo total del filtrado : 0.062431 s (62.431 ms)
Tiempo de CPU del filtrado : 0.224118 s (224.118 ms)
```