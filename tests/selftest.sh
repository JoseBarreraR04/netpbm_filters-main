#!/bin/sh
# Suite del Diseno 4. Comprueba que el filtro distribuido con MPI produce
# exactamente la misma imagen, byte a byte, que el filtro secuencial de
# referencia.
#
# La idea es simple: ref_filter recorre la imagen entera sin dividirla, y
# mpi_filterer la parte en bandas y pide las filas de halo a sus vecinos. Si los
# dos coinciden, el reparto y el intercambio estan correctos. No se permite
# comparar con tolerancia numerica, porque ambas versiones hacen la misma
# aritmetica y cualquier diferencia es un error real de particion.
set -e

BIN=${BIN:-/build}
MPIEXEC=${MPIEXEC:-mpiexec}
NODES=${NODES:-4}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

OK=0
FAIL=0

pass() {
  OK=$((OK + 1))
  printf '  ok   %s\n' "$1"
}

fail() {
  FAIL=$((FAIL + 1))
  printf '  FAIL %s\n' "$1"
}

# compare a b etiqueta
compare() {
  if [ ! -f "$1" ] || [ ! -f "$2" ]; then
    fail "$3 (falta un archivo de salida)"
    return
  fi
  if cmp -s "$1" "$2"; then
    pass "$3"
  else
    fail "$3 (las imagenes difieren)"
  fi
}

# make_case imagen_anchura_imagen_alto etiqueta
# Crea una imagen pequena y conocida, con un borde duro y un patron, para que un
# reparto de bandas mal hecho se note de inmediato en las filas de union.
make_case() {
  _w=$1
  _h=$2
  _tag=$3
  _file="$WORK/$_tag.pgm"
  {
    echo "P2"
    echo "# imagen generada por selftest.sh"
    echo "$_w $_h"
    echo "255"
    y=0
    while [ "$y" -lt "$_h" ]; do
      x=0
      while [ "$x" -lt "$_w" ]; do
        # Bordes en 0 y 255, patron en medio: dos filas contiguas muy distintas
        # para que una fila de halo equivocada sea visible.
        if [ "$x" -eq 0 ] || [ "$y" -eq 0 ]; then
          echo 0
        elif [ "$x" -eq $((_w - 1)) ] || [ "$y" -eq $((_h - 1)) ]; then
          echo 255
        else
          echo $(( (x * 7 + y * 13) % 256 ))
        fi
        x=$((x + 1))
      done
      y=$((y + 1))
    done
  } >"$_file"
  echo "$_file"
}

echo "== 1. Los tres filtros sobre las imagenes del proyecto =="
for img in damma.pgm sulfur.ppm; do
  for filter in blur laplace sharpening; do
    ref="$WORK/ref_${img}_${filter}"
    out="$WORK/out_${img}_${filter}"
    "$BIN/ref_filter" "/app/images/$img" "$ref" --f "$filter"
    "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "/app/images/$img" "$out" --f "$filter" >/dev/null
    compare "$ref" "$out" "$img + $filter ($NODES nodos)"
  done
done

echo
echo "== 2. La imagen debe salir identica sea cual sea el numero de nodos =="
# El reparto en bandas cambia con el numero de nodos, pero el resultado final
# no debe. Si un halo se pierde o se duplica, aqui se rompe.
for filter in blur laplace sharpening; do
  ref="$WORK/ref_nodes_$filter"
  "$BIN/ref_filter" /app/images/damma.pgm "$ref" --f "$filter"
  for n in 1 2 3 4 5 8; do
    out="$WORK/out_nodes_${filter}_$n"
    "$MPIEXEC" -n "$n" "$BIN/mpi_filterer" /app/images/damma.pgm "$out" --f "$filter" >/dev/null
    compare "$ref" "$out" "damma + $filter con $n nodo(s)"
  done
done

echo
echo "== 3. Imagenes de alta impar, que forzan filas de mas y de menos =="
# Con alto primo y numero de nodos primo, algunas bandas tienen una fila mas
# que otras. Son exactamente el caso en el que un reparto erroneo pasaria
# desapercibido en una imagen de alto par.
for dims in "1 1" "1 5" "5 1" "2 3" "3 2" "7 11" "13 17" "31 29" "64 65" "100 99"; do
  w=${dims% *}
  h=${dims#* }
  img=$(make_case "$w" "$h" "case_${w}x${h}")
  for filter in blur laplace sharpening; do
    ref="$WORK/ref_${w}x${h}_${filter}"
    out="$WORK/out_${w}x${h}_${filter}"
    "$BIN/ref_filter" "$img" "$ref" --f "$filter"
    "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$img" "$out" --f "$filter" >/dev/null
    compare "$ref" "$out" "${w}x${h} + $filter"
  done
done

echo
echo "== 4. Mas nodos que filas: algunos nodos no reciben nada =="
# Con una imagen de 2 filas y 4 nodos, dos nodos se quedan sin banda. Deben
# participar igual en el intercambio sin romperlo.
for filter in blur laplace sharpening; do
  img=$(make_case 3 2 "tiny")
  ref="$WORK/ref_tiny_$filter"
  out="$WORK/out_tiny_$filter"
  "$BIN/ref_filter" "$img" "$ref" --f "$filter"
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$img" "$out" --f "$filter" >/dev/null
  compare "$ref" "$out" "3x2 con $NODES nodos + $filter"
done

echo
echo "== 5. Formato P3 a color, tres canales =="
p3="$WORK/color.ppm"
{
  echo "P3"
  echo "4 3"
  echo "255"
  echo "255 0 0  0 255 0  0 0 255  255 255 0"
  echo "10 20 30  40 50 60  70 80 90  100 110 120"
  echo "0 0 0  255 255 255  128 0 128  0 128 0"
} >"$p3"
for filter in blur laplace sharpening; do
  ref="$WORK/ref_color_$filter"
  out="$WORK/out_color_$filter"
  "$BIN/ref_filter" "$p3" "$ref" --f "$filter"
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$p3" "$out" --f "$filter" >/dev/null
  compare "$ref" "$out" "color.ppm + $filter"
done

echo
echo "== 6. Comentarios y formato de cabecera =="
# Los comentarios # se pueden poner entre cualquier valor. Un parser que no los
# salta dara numeros equivocados.
odd="$WORK/odd.pgm"
{
  echo "P2 # numero magico con comentario pegado"
  echo "5 5 # ancho y alto"
  echo "255 # valor maximo"
  y=0
  while [ "$y" -lt 5 ]; do
    x=0
    while [ "$x" -lt 5 ]; do
      echo "# pixel $x,$y"
      echo $(( (x * 11 + y * 17) % 256 ))
      x=$((x + 1))
    done
    y=$((y + 1))
  done
} >"$odd"
for filter in blur laplace sharpening; do
  ref="$WORK/ref_odd_$filter"
  out="$WORK/out_odd_$filter"
  "$BIN/ref_filter" "$odd" "$ref" --f "$filter"
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "$odd" "$out" --f "$filter" >/dev/null
  compare "$ref" "$out" "cabecera con comentarios + $filter"
done

echo
echo "== 7. La entrada por stdin da el mismo resultado que por archivo =="
# Aqui NO se usa mpiexec, a proposito. El reenvio de stdin de hydra (MPICH)
# rompe en este contenedor: mata cualquier hijo que lea la entrada estandar, y
# se reproduce sin este proyecto con
#     mpiexec -n 1 sh -c 'cat > /dev/null; sleep 3' < fichero
# que tambien muere con codigo 141. No es un fallo de mpi_filterer: el mismo
# binario launched directly da la imagen correcta byte a byte.
for filter in blur laplace sharpening; do
  ref="$WORK/ref_stdin_$filter"
  out="$WORK/out_stdin_$filter"
  "$BIN/ref_filter" /app/images/damma.pgm "$ref" --f "$filter"
  "$BIN/mpi_filterer" "$out" --f "$filter" </app/images/damma.pgm >/dev/null
  compare "$ref" "$out" "stdin + $filter"
done

echo
echo "== 8. El numero magico de la salida se conserva =="
# Salir con el mismo P2 o P3 es parte del formato, no un detalle menor.
for img in damma.pgm sulfur.ppm; do
  out="$WORK/magic_$img"
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" "/app/images/$img" "$out" --f blur >/dev/null
  expected=$(head -c 2 "/app/images/$img")
  got=$(head -c 2 "$out")
  if [ "$expected" = "$got" ]; then
    pass "magic de $img se conserva ($got)"
  else
    fail "magic de $img cambio: era $expected, quedo $got"
  fi
done

echo
echo "== 9. Los errores se reportan y el codigo de salida no es cero =="
check_error() {
  _label=$1
  shift
  if "$@" >/dev/null 2>&1; then
    fail "$_label (debio fallar y devolvio exito)"
  else
    pass "$_label"
  fi
}
check_error "filtro inexistente" "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" /app/images/damma.pgm "$WORK/e1.pgm" --f nope
check_error "falta el filtro" "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" /app/images/damma.pgm "$WORK/e2.pgm"
check_error "fichero que no existe" "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" /app/images/no_existe.pgm "$WORK/e3.pgm" --f blur
check_error "no es una imagen" "$BIN/ref_filter" /app/README.md "$WORK/e4.pgm" --f blur
check_error "sobran rutas" "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" a b c --f blur

echo
echo "== 10. La tabla de tiempos incluye un tiempo por nodo =="
report="$WORK/report.txt"
"$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" /app/images/damma.pgm "$WORK/timed.pgm" --f blur \
  >"$report"
missing=0
r=0
while [ "$r" -lt "$NODES" ]; do
  if ! grep -q "nodo $r :" "$report"; then
    missing=$((missing + 1))
  fi
  r=$((r + 1))
done
if [ "$missing" -eq 0 ]; then
  pass "la tabla reporta los $NODES nodos"
else
  fail "faltan $missing nodos en la tabla de tiempos"
fi
if grep -q "Tiempo total del filtrado" "$report" && grep -q "Tiempo de CPU del filtrado" "$report"; then
  pass "se informa el tiempo total y el de CPU"
else
  fail "falta el tiempo total o el de CPU"
fi

echo
echo "== 11. Los filtros dan resultados distintos entre si =="
# Si blur y laplace dieran lo mismo, la suite pasaria sin comprobar nada.
for filter in blur laplace sharpening; do
  out="$WORK/distinct_$filter"
  "$MPIEXEC" -n "$NODES" "$BIN/mpi_filterer" /app/images/damma.pgm "$out" --f "$filter" >/dev/null
done
different=0
cmp_different() {
  if ! cmp -s "$WORK/distinct_$1" "$WORK/distinct_$2"; then
    different=$((different + 1))
  fi
}
cmp_different blur laplace
cmp_different blur sharpening
cmp_different laplace sharpening
if [ "$different" -eq 3 ]; then
  pass "los tres filtros producen imagenes distintas entre si"
else
  fail "solo $different de 3 pares de filtros difieren"
fi

echo
echo "==============================================="
echo "Resultado: $((OK + FAIL)) pruebas, $OK correctas, $FAIL fallidas"
echo "==============================================="
[ "$FAIL" -eq 0 ] || exit 1