#!/bin/sh
# Punto de entrada del contenedor del Diseno 4.
#
# Los binarios no se hornean en la imagen: el codigo se monta en /app y se
# compila en el primer uso dentro de un directorio de cache, porque
# docker-compose monta el repositorio entero y en Windows la marca de tiempo de
# los archivos siempre cambia, lo que haria que make recompilara cada vez.
set -e

CACHE=/tmp/build
BIN=/build
SRC=/app/src
INC=/app/headers
TESTS=/app/tests
IMAGES=/app/images

MPIEXEC=${MPIEXEC:-mpiexec}
NODES=${NODES:-4}

# Compila lo que falte. No se usa make porque el montaje de Windows cambia los
# mtimes y make no puede decidir si un .o esta al dia.
ensure_build() {
  mkdir -p "$BIN" "$CACHE"
  if [ ! -x "$BIN/mpi_filterer" ] || [ -n "$(find "$SRC" "$INC" -newer "$BIN/mpi_filterer" 2>/dev/null)" ]; then
    mpicxx -O2 -Wall -Wextra -I "$INC" -o "$BIN/mpi_filterer" "$SRC/mpi_filterer.cpp"
  fi
  if [ ! -x "$BIN/ref_filter" ] || [ -n "$(find "$TESTS/ref_filter.cpp" -newer "$BIN/ref_filter" 2>/dev/null)" ]; then
    g++ -O2 -Wall -Wextra -o "$BIN/ref_filter" "$TESTS/ref_filter.cpp"
  fi
}

run_filter() {
  # run_filter entrada salida filtro
  ensure_build
  # Los argumentos se leen de stdin solo en el nodo 0, asi que con mpiexec basta
  # con redirigir la entrada del proceso.
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$1" "$2" --f "$3"
}

case "$1" in
  build)
    ensure_build
    echo "Compilado: $BIN/mpi_filterer y $BIN/ref_filter"
    ;;

  selftest)
    ensure_build
    sh "$TESTS/selftest.sh"
    ;;

  demo)
    ensure_build
    set -x
    "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$IMAGES/damma.pgm" /out/damma_blur.pgm --f blur
    "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$IMAGES/sulfur.ppm" /out/sulfur_sharpen.ppm --f sharpening
    set +x
    echo "Salidas de la demo (en el host, dentro de out/):"
    echo "  out/damma_blur.pgm"
    echo "  out/sulfur_sharpen.ppm"
    ;;

  mpi_filterer)
    shift
    ensure_build
    exec "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$@"
    ;;

  ref_filter)
    shift
    ensure_build
    exec "$BIN/ref_filter" "$@"
    ;;

  shell)
    exec /bin/sh
    ;;

  # Para depurar: docker compose run --rm app sh -c "..."
  sh|bash)
    shift
    exec /bin/sh "$@"
    ;;

  mpicxx)
    exec mpicxx "$@"
    ;;

  help|*)
    cat <<'EOF'
Diseno 4: memoria distribuida con MPI

  N=n nodos (por omision 4). Se lanzan con: mpiexec -n $N

  Comandos:
    build                     compila mpi_filterer y ref_filter
    selftest                  corre toda la suite y dice si pasa
    demo                      filtra damma.pgm y sulfur.ppm con 4 nodos
    mpi_filterer A B --f f    filtro distribuido con N nodos
    ref_filter A B --f f      filtro secuencial de referencia
    shell                     shell dentro del contenedor
    mpicxx ...                compilador de MPI

  Filtros: blur, laplace, sharpening
EOF
    if [ "$1" != "help" ] && [ $# -gt 0 ]; then
      exit 1
    fi
    ;;

esac