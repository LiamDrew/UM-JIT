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
    dbus \
    dbus-x11 \
    qtbase5-dev \
    qt5-qmake \
    qttools5-dev-tools \
    libqt5core5a \
    libqt5gui5 \
    libqt5widgets5 \
    libxcb1 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libqt5core5a \
    libqt5gui5 \
    libqt5widgets5 \
    libxcb1 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-shape0 \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user with macOS UID
RUN useradd -u 501 -m -s /bin/bash developer && \
    mkdir -p /home/developer && \
    chown -R developer:developer /home/developer && \
    chmod -R 755 /home/developer
USER developer
WORKDIR /home/developer

# Make sure runtime directory exists with correct permissions
RUN mkdir -p /tmp/runtime-developer && \
    chown -R developer:developer /tmp/runtime-developer && \
    chmod 700 /tmp/runtime-developer

# Set up the entrypoint
CMD ["/bin/bash"]

# FROM ubuntu:22.04

# # Prevent interactive prompts during package installation
# ENV DEBIAN_FRONTEND=noninteractive

# # Install essential packages and development tools
# RUN apt-get update && apt-get install -y \
#     build-essential \
#     time \
#     gcc \
#     g++ \
#     gdb \
#     valgrind \
#     kcachegrind \
#     graphviz \
#     cmake \
#     git \
#     && rm -rf /var/lib/apt/lists/*

# # Set up X11 forwarding for GUI applications
# RUN apt-get update && apt-get install -y \
#     libx11-6 \
#     libxext6 \
#     libxrender1 \
#     libxtst6 \
#     libxi6 \
#     dbus \
#     dbus-x11 \
#     qtbase5-dev \
#     qt5-qmake \
#     qttools5-dev-tools \
#     && rm -rf /var/lib/apt/lists/*

# # Create a non-root user with macOS UID
# RUN useradd -u 501 -m -s /bin/bash developer && \
#     mkdir -p /home/developer && \
#     chown -R developer:developer /home/developer && \
#     chmod -R 755 /home/developer
# USER developer
# WORKDIR /home/developer

# # Make sure runtime directory exists with correct permissions
# RUN mkdir -p /tmp/runtime-developer && \
#     chown -R developer:developer /tmp/runtime-developer && \
#     chmod 700 /tmp/runtime-developer

# # Set up the entrypoint
# CMD ["/bin/bash"]

# # # Create a non-root user
# # RUN useradd -m -s /bin/bash developer
# # USER developer
# # WORKDIR /home/developer

# # # Set up the entrypoint
# # CMD ["/bin/bash"]

# docker run -it \
#     -e DISPLAY=host.docker.internal:0 \
#     -v /tmp/.X11-unix:/tmp/.X11-unix \
#     -v "$PWD/docker_shared:/home/developer/shared" \
#     dev-tools