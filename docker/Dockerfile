# Resolve OFX Win64 Builder
# Base on Ubuntu 24.04 (Noble) for modern LLVM/Clang
FROM ubuntu:24.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install core build tools
RUN sed -i 's|http://archive.ubuntu.com/ubuntu/|http://ftp.udx.icscoe.jp/Linux/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources && \
    sed -i 's|http://security.ubuntu.com/ubuntu/|http://ftp.udx.icscoe.jp/Linux/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources && \
    apt-get update && apt-get install -y \
    clang-18 \
    lld-18 \
    llvm-18 \
    mingw-w64 \
    cmake \
    ninja-build \
    make \
    ccache \
    git \
    zip \
    ca-certificates \
    curl \
    opencl-headers \
    python3-pip \
    pipx \
    && chmod -R 777 /opt \
    && rm -rf /var/lib/apt/lists/*

# Install Code Metrics Tools (scc, tokei, lizard)
ENV PATH="/root/.local/bin:${PATH}"
RUN curl -sL https://github.com/boyter/scc/releases/download/v3.7.0/scc_Linux_x86_64.tar.gz | tar xz -C /usr/local/bin scc && \
    curl -sL https://github.com/XAMPPRocky/tokei/releases/download/v12.1.2/tokei-x86_64-unknown-linux-musl.tar.gz | tar xz -C /usr/local/bin && \
    pipx install lizard

# CUDA: create libcuda.a import library stub for nvcuda.dll
RUN printf 'LIBRARY nvcuda\nEXPORTS\ncuModuleLoadData\ncuModuleGetFunction\ncuMemAlloc_v2\ncuMemAllocHost_v2\ncuMemFreeHost\ncuMemcpyHtoD_v2\ncuMemcpyHtoDAsync_v2\ncuMemFree_v2\ncuLaunchKernel\ncuStreamSynchronize\n' > /tmp/cuda.def && \
    x86_64-w64-mingw32-dlltool -d /tmp/cuda.def -l /usr/x86_64-w64-mingw32/lib/libcuda.a

# OpenCL: copy headers into MinGW sysroot and create libOpenCL.a import library stub
RUN mkdir -p /usr/x86_64-w64-mingw32/include/CL && \
    cp /usr/include/CL/*.h /usr/x86_64-w64-mingw32/include/CL/ && \
    printf 'LIBRARY OpenCL\nEXPORTS\nclGetCommandQueueInfo\nclCreateProgramWithSource\nclBuildProgram\nclGetProgramBuildInfo\nclCreateKernel\nclReleaseKernel\nclReleaseProgram\nclSetKernelArg\nclEnqueueNDRangeKernel\nclGetKernelWorkGroupInfo\nclCreateBuffer\nclEnqueueWriteBuffer\nclFlush\nclReleaseMemObject\n' > /tmp/OpenCL.def && \
    x86_64-w64-mingw32-dlltool -d /tmp/OpenCL.def -l /usr/x86_64-w64-mingw32/lib/libOpenCL.a

# Set Clang 18 and LLVM tools as default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-18 100 && \
    update-alternatives --install /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-18 100 && \
    update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/lld-18 100 && \
    update-alternatives --install /usr/bin/windres windres /usr/bin/x86_64-w64-mingw32-windres 100

# Disable git detached HEAD advice globally for cleaner logs
RUN git config --global advice.detachedHead false

# Prepare SDK directories
WORKDIR /opt

# 1. OpenFX SDK
RUN git clone --quiet --depth 1 --branch OFX_Release_1.5.1 https://github.com/ofxa/openfx.git ofx-sdk

# 2. Dependencies (Blend2D, AsmJit, utf8cpp, glaze, freetype, harfbuzz)
RUN mkdir -p 3rdparty && \
    git clone --quiet --depth 1 https://github.com/blend2d/blend2d.git 3rdparty/blend2d && \
    git clone --quiet --depth 1 https://github.com/asmjit/asmjit.git 3rdparty/asmjit && \
    git clone --quiet --depth 1 --branch v4.0.9 https://github.com/nemtrif/utfcpp.git 3rdparty/utf8cpp && \
    git clone --quiet --depth 1 --branch v2.2.0 https://github.com/stephenberry/glaze.git 3rdparty/glaze && \
    git clone --quiet --depth 1 --branch VER-2-14-3 https://github.com/freetype/freetype.git 3rdparty/freetype && \
    git clone --quiet --depth 1 --branch 14.2.0 https://github.com/harfbuzz/harfbuzz.git 3rdparty/harfbuzz

# Environment variables for CMake
ENV OFX_SDK_DIR=/opt/ofx-sdk
ENV BLEND2D_DIR=/opt/3rdparty/blend2d
ENV ASMJIT_DIR=/opt/3rdparty/asmjit
ENV UTF8CPP_DIR=/opt/3rdparty/utf8cpp
ENV GLAZE_DIR=/opt/3rdparty/glaze
ENV FREETYPE_DIR=/opt/3rdparty/freetype
ENV HARFBUZZ_DIR=/opt/3rdparty/harfbuzz

# Entrypoint setup
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Workspace setup
WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
