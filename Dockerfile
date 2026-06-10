ARG BASE_IMAGE=ubuntu:22.04
FROM ${BASE_IMAGE} AS builder

ARG CC=gcc-12
ARG CXX=g++-12
ARG BUILD_TYPE=Release

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    gcc-12 \
    g++-12 \
    clang-15 \
    ninja-build \
    python3 \
    python3-pip \
    git \
    ca-certificates \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-15 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-15 100 \
    && python3 -m pip install --no-cache-dir meson ninja \
    && rm -rf /var/lib/apt/lists/*

COPY . /src
WORKDIR /src

RUN cmake -S . -B build_ci_test \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_C_COMPILER=${CC} \
    -DCMAKE_CXX_COMPILER=${CXX} \
    -DBLESSSTAR_TREAT_WARNINGS_AS_ERRORS=ON \
    -DBLESSSTAR_ENABLE_PURITY_CHECK=ON \
    && cmake --build build_ci_test -j $(nproc) \
    && meson setup build-meson \
    && meson compile -C build-meson

FROM ${BASE_IMAGE} AS runner

ARG CACHEBUST=1
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libstdc++6 \
    python3 \
    python3-pip \
    python-is-python3 \
    ca-certificates \
    && python3 -m pip install --no-cache-dir meson ninja \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src /src
COPY tools/docker/entrypoint-ctest-meson.sh /usr/local/bin/docker-test-entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/docker-test-entrypoint.sh \
    && chmod +x /usr/local/bin/docker-test-entrypoint.sh

WORKDIR /src
ENTRYPOINT ["/usr/local/bin/docker-test-entrypoint.sh"]
CMD ["-j", "2"]
