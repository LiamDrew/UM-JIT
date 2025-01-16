FROM --platform=linux/amd64 ubuntu:22.04

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

# # Create a non-root user with macOS UID
# RUN useradd -u 501 -m -s /bin/bash developer && \
#     mkdir -p /home/developer && \
#     chown -R developer:developer /home/developer

# Create a non-root user without a specific UID
RUN useradd -m -s /bin/bash developer && \
    mkdir -p /home/developer && \
    chown -R developer:developer /home/developer

USER developer
WORKDIR /home/developer

# Default to bash shell
CMD ["/bin/bash"]

# build image
# docker buildx build --platform=linux/amd64 -t dev-tools-x86 .

# run container
# docker run --platform=linux/amd64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-x86
