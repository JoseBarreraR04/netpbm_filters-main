# Entorno de compilacion y pruebas para el Micro-Proyecto No. 2 (Diseno 3).
#
# Construir:  docker build -t netpbm-filters-d3 .
# Ejecutar:    docker run --rm -it -v "${PWD}:/app" -v "${PWD}/out:/out" netpbm-filters-d3 th_filterer images/damma.pgm /out/damma_blur.pgm --f blur
#
# O, mas comodo, con compose:  docker compose run --rm app th_filterer images/damma.pgm /out/damma_blur.pgm --f blur
FROM gcc:13

# netpbm aporta pnmfile / pnmpsnr / pnmscale para inspeccionar las imagenes.
RUN apt-get update \
    && apt-get install -y --no-install-recommends netpbm file \
    && rm -rf /var/lib/apt/lists/*

# Codigo fuente y scripts. Si se monta el proyecto sobre /app estos archivos se
# reemplazan por los del host, de modo que el codigo que se compila es siempre
# el que esta editando el usuario.
WORKDIR /app
COPY src/ /app/src/
COPY tests/ /app/tests/
COPY docker/ /app/docker/
COPY images/ /app/images/
RUN chmod +x /app/docker/entrypoint.sh /app/tests/selftest.sh

# Los binarios se compilan en /build para que un bind mount sobre /app nunca los
# tape y nunca aparezcan como archivos sin seguimiento dentro del repositorio.
ENV BUILD_DIR=/build
RUN mkdir -p /build

ENTRYPOINT ["/app/docker/entrypoint.sh"]
CMD ["help"]