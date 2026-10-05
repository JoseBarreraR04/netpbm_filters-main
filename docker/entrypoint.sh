#!/usr/bin/env bash
# Compila lo que se le pide (solo recompila si el fuente es mas nuevo que el
# binario) y ejecuta el comando pedido.
#
#   docker compose run --rm app th_filterer images/damma.pgm /out/damma_blur.pgm --f blur
#   docker compose run --rm app filterer  images/damma.pgm /out/damma_seq.pgm
#   docker compose run --rm app processor images/lena.pgm /out/lena_copy.pgm
#   docker compose run --rm app demo
#   docker compose run --rm app selftest
#   docker compose run --rm app shell
set -euo pipefail

SRC_DIR=/app/src
BUILD_DIR="${BUILD_DIR:-/build}"
OUT_DIR=/out
CXXFLAGS=(-std=c++11 -O2 -Wall -Wextra)

mkdir -p "$BUILD_DIR" "$OUT_DIR"

# ensure <programa> <fuente> [flags extra...]
ensure() {
  local prog="$1" src="$2"
  shift 2
  if [ -x "$BUILD_DIR/$prog" ] && [ ! "$SRC_DIR/$src" -nt "$BUILD_DIR/$prog" ]; then
    return 0
  fi
  echo "[docker] compilando $prog ..."
  g++ "${CXXFLAGS[@]}" "$@" -o "$BUILD_DIR/$prog" "$SRC_DIR/$src"
}

usage() {
  cat <<'EOF'
Micro-Proyecto No. 2 - Diseno 3 (memoria compartida)

Uso:
  app th_filterer ENTRADA SALIDA --f blur|laplace|sharpening
  app th_filterer SALIDA --f blur|laplace|sharpening      (entrada por stdin)
  app filterer  ENTRADA SALIDA                            (blur secuencial)
  app processor ENTRADA SALIDA                            (solo copia/reformatea)
  app demo                                               (recorrido guiado)
  app selftest                                           (verificacion completa)
  app shell                                              (bash interactivo)
  app help                                               (este mensaje)

Ejemplos:
  app th_filterer images/damma.pgm  /out/damma_blur.pgm        --f blur
  app th_filterer images/sulfur.pgm /out/sulfur_laplace.pgm    --f laplace
  app th_filterer images/sulfur.ppm /out/sulfur_sharpening.ppm --f sharpening
  app th_filterer /out/lena_blur.pgm --f blur < images/lena.pgm

Rutas dentro del contenedor:
  /app           -> el proyecto (montado desde Windows)
  /app/images    -> imagenes de prueba
  /out           -> carpeta de salidas (montada en ./out)
  /build         -> binarios compilados
EOF
}

selftest_all() {
  if [ ! -f /app/tests/selftest.sh ]; then
    echo "no se encuentra /app/tests/selftest.sh"
    return 1
  fi
  bash /app/tests/selftest.sh
}

demo() {
  echo "==============================================================="
  echo " Diseno 3: demo con las imagenes damma y sulfur"
  echo "==============================================================="
  echo
  echo "--- damma.pgm 1000x1278 con blur ---"
  ensure th_filterer th_filterer.cpp -pthread
  "$BUILD_DIR/th_filterer" images/damma.pgm "$OUT_DIR/damma_blur.pgm" --f blur
  echo
  echo "--- sulfur.pgm 823x1000 (ancho impar) con laplace ---"
  "$BUILD_DIR/th_filterer" images/sulfur.pgm "$OUT_DIR/sulfur_laplace.pgm" --f laplace
  echo
  echo "--- sulfur.ppm 823x1000 color con sharpening ---"
  "$BUILD_DIR/th_filterer" images/sulfur.ppm "$OUT_DIR/sulfur_sharpening.ppm" --f sharpening
  echo
  echo "--- entrada por stdin ---"
  "$BUILD_DIR/th_filterer" "$OUT_DIR/lena_blur.pgm" --f blur < images/lena.pgm
  echo
  echo "--- processor (diseno 1): solo copia y reformatea ---"
  ensure processor processor.cpp
  "$BUILD_DIR/processor" images/lena.pgm "$OUT_DIR/lena_copy.pgm" > /dev/null
  echo "ok   lena.pgm -> lena_copy.pgm"
  echo
  echo "=== el blur paralelo coincide con el blur secuencial (filterer.cpp) ==="
  ensure filterer filterer.cpp
  "$BUILD_DIR/filterer" images/damma.pgm "$OUT_DIR/damma_secuencial.pgm" > /dev/null
  if cmp -s "$OUT_DIR/damma_blur.pgm" "$OUT_DIR/damma_secuencial.pgm"; then
    echo "ok   las dos imagenes son identicas byte a byte"
  else
    echo "FALLO difieren"
  fi
  echo
  echo "=== verificacion completa ==="
  selftest_all
}

cmd="${1:-help}"
if [ "$#" -gt 0 ]; then
  shift
fi

case "$cmd" in
  help|-h|--help) usage ;;
  demo)           demo ;;
  selftest|test)  selftest_all ;;
  shell|bash)     exec /bin/bash ;;
  th_filterer)    ensure th_filterer th_filterer.cpp -pthread
                  exec "$BUILD_DIR/th_filterer" "$@" ;;
  filterer)       ensure filterer filterer.cpp
                  exec "$BUILD_DIR/filterer" "$@" ;;
  processor)      ensure processor processor.cpp
                  exec "$BUILD_DIR/processor" "$@" ;;
  *)
    echo "comando desconocido: $cmd"
    usage
    exit 1 ;;
esac