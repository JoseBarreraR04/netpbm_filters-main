#!/usr/bin/env bash
# Verificacion del Diseno 3 (memoria compartida).
#
#   docker compose run --rm app selftest
#
# Comprueba que el resultado de th_filterer (4 regiones + 4 hilos) es identico
# byte a byte al de una referencia secuencial de un solo hilo, tanto para blur,
# laplace como sharpening, en imagenes P2 y P3, con anchos impares y con
# entradas diminutas de borde. Ademas valida los casos de error del programa.
set -u

BUILD_DIR="${BUILD_DIR:-/build}"
SRC_DIR=/app/src
TESTS_DIR=/app/tests
IMAGES_DIR=/app/images
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

PASS=0
FAIL=0

ok()   { PASS=$((PASS + 1)); echo "ok   $1"; }
bad()  { FAIL=$((FAIL + 1)); echo "FALLO $1"; }

echo "==============================================================="
echo " Verificacion del Diseno 3 - th_filterer"
echo "==============================================================="
echo

# --- construye lo que hace falta (el script es autonomo) --------------------
echo "--- construyendo los programas ---"
mkdir -p "$BUILD_DIR"
if ! g++ -std=c++11 -O2 -Wall -Wextra -pthread -o "$BUILD_DIR/th_filterer" \
         "$SRC_DIR/th_filterer.cpp" 2>"$WORK/tf.log"; then
  bad "no se pudo compilar th_filterer"
  sed 's/^/       /' "$WORK/tf.log"
  exit 1
fi
ok "th_filterer construido"
if ! g++ -std=c++11 -O2 -o "$BUILD_DIR/filterer" "$SRC_DIR/filterer.cpp" 2>"$WORK/fi.log"; then
  bad "no se pudo compilar filterer"
  sed 's/^/       /' "$WORK/fi.log"
else
  ok "filterer construido"
fi
echo

# --- compilacion limpia ----------------------------------------------------
echo "--- compilacion con -Wall -Wextra ---"
BUILD_LOG="$WORK/build.log"
if g++ -std=c++11 -O2 -Wall -Wextra -pthread -o "$BUILD_DIR/th_filterer_w" \
        "$SRC_DIR/th_filterer.cpp" > "$BUILD_LOG" 2>&1; then
  if [ -s "$BUILD_LOG" ]; then
    bad "la compilacion emitio warnings"
    sed 's/^/       /' "$BUILD_LOG"
  else
    ok "compila sin warnings"
  fi
else
  bad "no compila"
  sed 's/^/       /' "$BUILD_LOG"
fi
echo

# --- referencia secuencial -------------------------------------------------
g++ -std=c++11 -O2 -o "$WORK/ref" "$TESTS_DIR/ref_filter.cpp" 2>"$WORK/ref.log" || {
  bad "no se pudo compilar la referencia"; sed 's/^/       /' "$WORK/ref.log"; exit 1; }
ok "referencia secuencial compilada"
echo

# comparacion <etiqueta> <entrada> <filtro> [stdin]
compare() {
  local etiqueta="$1" entrada="$2" filtro="$3" modo="${4:-ruta}"
  local par="$WORK/par.out" seq="$WORK/seq.out"
  if [ "$modo" = "stdin" ]; then
    "$BUILD_DIR/th_filterer" "$par" --f "$filtro" < "$entrada" > /dev/null 2>"$WORK/err.log"
  else
    "$BUILD_DIR/th_filterer" "$entrada" "$par" --f "$filtro" > /dev/null 2>"$WORK/err.log"
  fi
  if [ $? -ne 0 ]; then
    bad "$etiqueta ($filtro): th_filterer fallo"
    sed 's/^/       /' "$WORK/err.log"
    return
  fi
  if ! "$WORK/ref" "$entrada" "$seq" --f "$filtro" 2>/dev/null; then
    bad "$etiqueta ($filtro): la referencia fallo"
    return
  fi
  if cmp -s "$par" "$seq"; then
    ok "$etiqueta ($filtro)"
  else
    bad "$etiqueta ($filtro): las imagenes difieren"
  fi
}

# --- paridad con la referencia sobre las imagenes del proyecto --------------
echo "--- paridad con la referencia secuencial (todas las imagenes y filtros) ---"
for nombre in damma.pgm damma.ppm sulfur.pgm sulfur.ppm lena.pgm lena.ppm \
              fruit.pgm fruit.ppm puj.pgm puj.ppm feep.pgm feep2.pgm; do
  [ -f "$IMAGES_DIR/$nombre" ] || { echo "skip $nombre (no esta)"; continue; }
  for filtro in blur laplace sharpening; do
    compare "$nombre" "$IMAGES_DIR/$nombre" "$filtro"
  done
done
echo

# --- imagenes diminutas: dimensiones impares y bordes ------------------------
echo "--- dimensiones impares y casos borde (1x1, 1x2, 2x1, ...) ---"
TINY="$WORK/tiny"
mkdir -p "$TINY"
seed=7
for dim in "1 1" "1 2" "2 1" "1 5" "5 1" "2 2" "3 3" "5 7" "7 5" "4 4" "2 3" "3 2" "6 6" "9 9"; do
  set -- $dim
  w=$1; h=$2
  for formato in P2 P3; do
    if [ "$formato" = "P2" ]; then ch=1; ext=pgm; else ch=3; ext=ppm; fi
    f="$TINY/t_${w}x${h}_${formato}.${ext}"
    {
      echo "$formato"
      echo "# comentario opcional de prueba"
      echo "$w $h"
      echo "255"
      n=0
      total=$((w * h * ch))
      while [ "$n" -lt "$total" ]; do
        seed=$(( (seed * 1103515245 + 12345) % 2147483648 ))
        echo -n "$((seed % 256))"
        n=$((n + 1))
        [ "$n" -lt "$total" ] && echo -n " "
        [ $((n % 16)) -eq 0 ] && echo
      done
      echo
    } > "$f"
    for filtro in blur laplace sharpening; do
      compare "${w}x${h} $formato" "$f" "$filtro"
    done
  done
done
echo

# --- entrada por stdin ------------------------------------------------------
echo "--- entrada por stdin ---"
for nombre in damma.pgm sulfur.ppm lena.pgm; do
  for filtro in blur laplace sharpening; do
    compare "stdin $nombre" "$IMAGES_DIR/$nombre" "$filtro" stdin
  done
done
echo

# --- paridad con el filterer.cpp del repo -----------------------------------
echo "--- paridad del blur con src/filterer.cpp (diseno secuencial) ---"
if g++ -std=c++11 -O2 -o "$WORK/filterer" "$SRC_DIR/filterer.cpp" 2>"$WORK/f.log"; then
  for nombre in damma.pgm sulfur.pgm lena.pgm fruit.pgm puj.pgm feep.pgm; do
    "$WORK/filterer" "$IMAGES_DIR/$nombre" "$WORK/seq_repo.pgm" > /dev/null 2>&1
    "$BUILD_DIR/th_filterer" "$IMAGES_DIR/$nombre" "$WORK/par_repo.pgm" --f blur > /dev/null 2>&1
    if cmp -s "$WORK/seq_repo.pgm" "$WORK/par_repo.pgm"; then
      ok "$nombre blur: th_filterer == filterer.cpp"
    else
      bad "$nombre blur: difieren del filterer.cpp"
    fi
  done
else
  echo "skip filterer.cpp no compila"
  sed 's/^/       /' "$WORK/f.log"
fi
echo

# --- casos de error ---------------------------------------------------------
echo "--- casos de error (deben salir con codigo distinto de 0) ---"
expect_fail() {
  local etiqueta="$1"; shift
  "$@" > /dev/null 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then ok "$etiqueta (exit=$rc)"; else bad "$etiqueta deberia fallar"; fi
}
expect_fail "sin argumentos"      "$BUILD_DIR/th_filterer"
expect_fail "sin --f"             "$BUILD_DIR/th_filterer" "$IMAGES_DIR/lena.pgm" "$WORK/x.pgm"
expect_fail "--f sin valor"       "$BUILD_DIR/th_filterer" "$IMAGES_DIR/lena.pgm" "$WORK/x.pgm" --f
expect_fail "filtro inexistente"  "$BUILD_DIR/th_filterer" "$IMAGES_DIR/lena.pgm" "$WORK/x.pgm" --f gaussian
expect_fail "entrada inexistente" "$BUILD_DIR/th_filterer" "$WORK/no_existe.pgm" "$WORK/x.pgm" --f blur
expect_fail "entrada no valida"   "$BUILD_DIR/th_filterer" /app/README.md "$WORK/x.pgm" --f blur
expect_fail "demasiadas rutas"    "$BUILD_DIR/th_filterer" a b c --f blur
echo

# --- formato de salida ------------------------------------------------------
echo "--- formato del archivo generado ---"
"$BUILD_DIR/th_filterer" "$IMAGES_DIR/feep.pgm" "$WORK/feep_out.pgm" --f blur > /dev/null
if command -v pnmfile > /dev/null 2>&1; then
  file "$WORK/feep_out.pgm" | sed 's/^/     /'
fi
if head -1 "$WORK/feep_out.pgm" | grep -qx P2; then
  ok "la cabecera se conserva (P2)"
else
  bad "la cabecera no se conserva"
fi
echo

echo "==============================================================="
echo " Resultado: $PASS correctos, $FAIL fallos"
echo "==============================================================="
[ "$FAIL" -eq 0 ] || exit 1