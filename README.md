Una aplicación en C++ que aplica filtros a imágenes en formatos PGM (escala de grises) y PPM (color). 

# Clonar o descargar el código

```bash
git clone https://github.com/japeto/netpbm_filters/tree/main
cd netpbm_filters
```

# Correr con Docker (no requiere g++ instalado)

Todo el proyecto se compila y se prueba dentro de un contenedor, así que no
necesitas un compilador nativo.

## 1. Un solo comando para verificar que funciona

```bash
docker compose run --rm app selftest
```

Compila los tres programas y comprueba que `th_filterer` (Diseño 3) produce
exactamente la misma imagen que una referencia secuencial de un solo hilo, en
blur, laplace y sharpening, sobre todas las imágenes del proyecto (incluidas
las de ancho impar como `sulfur.pgm` 823x1000 y las de color P3), además de
probar los casos de error.

## 2. Recorrido guiado con damma y sulfur

```bash
docker compose run --rm app demo
```

## 3. Ejecutar un filtro concreto

```bash
# las salidas van a ./out en Windows
docker compose run --rm app th_filterer images/damma.pgm  /out/damma_blur.pgm        --f blur
docker compose run --rm app th_filterer images/sulfur.pgm /out/sulfur_laplace.pgm    --f laplace
docker compose run --rm app th_filterer images/sulfur.ppm /out/sulfur_sharpening.ppm --f sharpening

# lectura desde la entrada estándar (desde cmd o bash; en PowerShell ver más abajo)
docker compose run --rm app th_filterer /out/lena_blur.pgm --f blur < images/lena.pgm
```

## 4. Comparar con el diseño secuencial

```bash
docker compose run --rm app filterer images/damma.pgm /out/damma_secuencial.pgm
```

## 5. Medir el speedup limitando los núcleos

```bash
docker compose run --rm un-nucleo      th_filterer images/damma.pgm /out/damma_1cpu.pgm --f blur
docker compose run --rm cuatro-nucleos th_filterer images/damma.pgm /out/damma_4cpu.pgm --f blur
```

Con 1 CPU el tiempo de CPU queda cerca del total; con 4, el tiempo de CPU
multiplica al total. Esa relación es el *speedup* del diseño.

> Ojo: con estas imágenes el filtrado dura pocos milisegundos, así que una sola
> medida es ruidosa. Para el informe repite el comando varias veces y quédate con
> el promedio o la mediana.

## 6. Entrar al contenedor

```bash
docker compose run --rm app shell
```

## Rutas dentro del contenedor

| Ruta | Contenido |
| --- | --- |
| `/app` | El proyecto, montado desde Windows |
| `/app/images` | Imágenes de prueba |
| `/out` | Carpeta de salidas, montada en `./out` |
| `/build` | Binarios compilados |

## Comandos equivalentes con `docker run`

Si prefieres no usar compose, la imagen ya trae el código y las imágenes de
prueba, así que ni siquiera hace falta montar carpetas:

```bash
docker build -t netpbm-filters-d3 .

docker run --rm netpbm-filters-d3 selftest
docker run --rm netpbm-filters-d3 th_filterer images/damma.pgm /out/damma_blur.pgm --f blur

# con las salidas guardadas en ./out del host
docker run --rm -v ./out:/out netpbm-filters-d3 th_filterer images/damma.pgm /out/damma_blur.pgm --f blur
```

Si además quieres que el contenedor use tu código local mientras editas:

```bash
docker run --rm -v "${PWD}:/app" -v "${PWD}/out:/out" netpbm-filters-d3 selftest
```

## Nota sobre PowerShell

La redirección `<` no existe en PowerShell, así que el ejemplo de `stdin` hay que
lanzarlo desde `cmd` o usar `Get-Content`:

```powershell
cmd /c "docker run --rm -i netpbm-filters-d3 th_filterer /out/lena_blur.pgm --f blur < images\lena.pgm"
# o bien
Get-Content images\lena.pgm -Raw | docker run --rm -i netpbm-filters-d3 th_filterer /out/lena_blur.pgm --f blur
```

# Compilar el programa (solo si ya tienes g++ instalado)

```bash
# Compilación básica
g++ -o processor processor.cpp

# Compilación con optimizaciones
g++ -O2 -o processor processor.cpp

# Compilación con warnings habilitados
g++ -Wall -Wextra -o processor processor.cpp
```

# Uso del processor

```bash
  ./processor lena.ppm lena2.ppm ##lectura desde la entrada estándar 
```

## Ejemplos de Archivos de entrada

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

### Recursos Adicionales

### 🔍 **Formatos de Imagen**
- [Especificación formato PPM](http://netpbm.sourceforge.net/doc/ppm.html)
- [Especificación formato PGM](http://netpbm.sourceforge.net/doc/pgm.html)
- [Wikipedia - Netpbm format](https://en.wikipedia.org/wiki/Netpbm)

### 🎨 **Procesamiento de Imágenes**
- [Kernels de convolución](https://en.wikipedia.org/wiki/Kernel_(image_processing))
- [Filtros de imagen - OpenCV docs](https://docs.opencv.org/4.x/d4/d13/tutorial_py_filtering.html)
- [Image Processing Fundamentals](https://imageprocessingplace.com/