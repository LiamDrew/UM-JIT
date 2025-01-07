FROM --platform=linux/amd64 ubuntu:22.04

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
    qtbase5-dev \
    graphviz \
    cmake \
    git \
    linux-tools-generic \
    linux-tools-common \
    linux-cloud-tools-generic \
    strace \
    ltrace \
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
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user with macOS UID
RUN useradd -u 501 -m -s /bin/bash developer && \
    mkdir -p /home/developer && \
    chown -R developer:developer /home/developer && \
    chmod -R 755 /home/developer

# # Configure perf permissions
# RUN echo 'developer ALL=(ALL:ALL) NOPASSWD: /usr/bin/perf' >> /etc/sudoers && \
#     echo 'kernel.perf_event_paranoid = -1' >> /etc/sysctl.conf && \
#     echo 'kernel.kptr_restrict = 0' >> /etc/sysctl.conf

USER developer
WORKDIR /home/developer

# Set environment variable for QT
ENV QT_X11_NO_MITSHM=1

CMD ["/bin/bash"]


# Run this container with:
# docker run -it \
#     --platform linux/amd64 \
#     -e DISPLAY=host.docker.internal:0 \
#     -v /tmp/.X11-unix:/tmp/.X11-unix \
#     -v "$PWD/docker_shared:/home/developer/shared" \
#     dev-tools-x86

docker run -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-x86