FROM ubuntu:24.04 AS dev

ENV DEBIAN_FRONTEND=noninteractive

# Core build tools and LLVM toolchain pinned to v18
RUN apt-get update && apt-get install -y --no-install-recommends \
  bash-completion \
  ca-certificates \
  cmake \
  git \
  locales \
  ninja-build \
  python3 \
  sudo \
  clang-18 \
  clangd-18 \
  clang-format-18 \
  clang-tidy-18 \
  libclang-rt-18-dev \
  lldb-18 \
  llvm-18 \
  && rm -rf /var/lib/apt/lists/*

# UTF-8 locale
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Register pinned binaries as the default commands
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
  && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
  && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100 \
  && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 \
  && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100 \
  && update-alternatives --install /usr/bin/run-clang-tidy run-clang-tidy /usr/bin/run-clang-tidy-18 100 \
  && update-alternatives --install /usr/bin/lldb lldb /usr/bin/lldb-18 100 \
  && update-alternatives --install /usr/bin/llvm-mc llvm-mc /usr/bin/llvm-mc-18 100 \
  && update-alternatives --install /usr/bin/llvm-objcopy llvm-objcopy /usr/bin/llvm-objcopy-18 100

# Set default compilers
ENV CC=clang
ENV CXX=clang++

# Passwordless sudo and /workspace folder
RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
  && chmod 0440 /etc/sudoers.d/ubuntu \
  && mkdir -p /workspace \
  && chown -R ubuntu:ubuntu /workspace

USER ubuntu
WORKDIR /workspace