FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential packages and development tools
RUN apt-get update && apt-get install -y \
    build-essential \
    time \
    gcc \
    g++ \
    gdb \
    valgrind \
    kcachegrind \
    graphviz \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set up X11 forwarding for GUI applications
RUN apt-get update && apt-get install -y \
    libx11-6 \
    libxext6 \
    libxrender1 \
    libxtst6 \
    libxi6 \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user
RUN useradd -m -s /bin/bash developer
USER developer
WORKDIR /home/developer

# Set up the entrypoint
CMD ["/bin/bash"]

# docker run -it \
#     -e DISPLAY=host.docker.internal:0 \
#     -v /tmp/.X11-unix:/tmp/.X11-unix \
#     -v "$PWD/docker_shared:/home/developer/shared" \
#     dev-tools