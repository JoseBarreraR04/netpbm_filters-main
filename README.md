Una aplicación en C++ que aplica filtros a imágenes en formatos PGM (escala de grises) y PPM (color). 

# Clonar o descargar el código

```bash
git clone https://github.com/japeto/netpbm_filters/tree/main
cd netpbm_filters
```

# Compilar el programa

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