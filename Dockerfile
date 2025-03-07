FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    curl \
    pkg-config \
    libssl-dev \
    wget \
    lsb-release \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Install newer CMake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware-archive-keyring.gpg >/dev/null && \
    echo "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/kitware-ubuntu.list && \
    apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

# Install Rust and cbindgen
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN cargo install cbindgen

# Set working directory
WORKDIR /app

# Copy the project files
COPY . .

# Build Rust library first
RUN cargo build --release

# Build the project
RUN mkdir -p build && cd build && \
    cmake -G Ninja .. && \
    ninja

# Set the entry point to bash for interactive testing
ENTRYPOINT ["/bin/bash"] 