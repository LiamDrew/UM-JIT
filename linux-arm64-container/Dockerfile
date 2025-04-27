FROM --platform=linux/arm64 ubuntu:22.04

# Set non-interactive mode for apt-get
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    clang \
    libc6-dev \
    gdb \
    valgrind \
    git \
    time \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Create a non-root user without a specific UID
RUN useradd -m -s /bin/bash developer && \
    mkdir -p /home/developer && \
    chown -R developer:developer /home/developer

USER developer
WORKDIR /home/developer

# Default to bash shell
CMD ["/bin/bash"]

# NOTE: Command to build image (with name dev-tools-aarch64)
# docker buildx build --platform=linux/arm64 -t dev-tools-aarch64 .

# NOTE: Command to run a container using this image
# docker run --platform=linux/arm64 -it -v "$PWD:/home/developer/" -v "$PWD/../umasm:/home/developer/umasm" dev-tools-aarch64

