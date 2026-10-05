// Micro-Proyecto No. 2 - Programacion Paralela 2026-II
// Diseno 4: Memoria distribuida
//
// Band: una banda horizontal de filas. Cada rank de MPI recibe una, y para
// filtrar la primera y la ultima fila necesita una fila de halo por arriba y
// por abajo, que le pide a sus vecinos con MPI_Sendrecv.
//
// Se eligio el reparto en bandas horizontales (y no en cuadrantes como el
// Diseno 3) porque asi cada pixel de un borde solo depende de la fila
// inmediatamente superior o inferior, que pertenece a un unico vecino. En
// cuadrantes haria falta ademas el vecino diagonal de la esquina, y el
// intercambio pasaria de 2 vecinos a 8.

#ifndef BAND_H
#define BAND_H

// Las filas [y0, y1) son responsabilidad de este rank.
struct Band {
  int y0;
  int y1;
  int rows() const { return y1 - y0; }
};

// Reparte `height` filas entre `ranks` bandas horizontales. Las filas sobrantes
// (cuando height no es multiplo de ranks) van a las primeras bandas, de modo
// que la diferencia entre dos bandas nunca es mayor que una fila.
inline Band splitBands(int height, int rank, int ranks) {
  const int base = height / ranks;
  const int extra = height % ranks;
  const int y0 = rank * base + (rank < extra ? rank : extra);
  const int rows = base + (rank < extra ? 1 : 0);
  Band band;
  band.y0 = y0;
  band.y1 = y0 + rows;
  return band;
}

// Etiqueta legible de la banda, para la salida por pantalla.
inline const char* bandLabel(int rank, int ranks) {
  return (rank == 0) ? "nodo 0 (norte)"
                     : (rank == ranks - 1 ? "ultimo nodo (sur)" : "nodo intermedio");
}

#endif  // BAND_H