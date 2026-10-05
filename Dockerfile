# Diseno 4: memoria distribuida con MPI.
#
# Se usa debian:bookworm-slim en vez de la imagen gcc porque esta ultima trae un
# gfortran a medias instalado que rompe update-alternatives cuando libmpich-dev
# lo arrastra como dependencia. Aqui se instala g++ explicitamente.
#
# MPICH en vez de OpenMPI porque mpiexec arranca los rangos sin necesidad de
# --allow-run-as-root ni --oversubscribe dentro del contenedor.
FROM debian:bookworm-slim

# netpbm aporta pamfile/pgm... para ver las imagenes; file identifica el tipo.
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ \
        make \
        mpich \
        libmpich-dev \
        netpbm \
        file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

ENV NODES=4

# Las fuentes y las imagenes de prueba se montan en tiempo de ejecucion, asi que
# la imagen solo trae el punto de entrada.
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["help"]