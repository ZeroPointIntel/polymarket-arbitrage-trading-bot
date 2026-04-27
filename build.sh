#!/bin/bash
set -e

echo "==> Setting up C++ Build Environment for Trading Core from Repository Root..."

# Check if Conan or CMake is installed globally or in current env
if ! command -v conan &> /dev/null || ! command -v cmake &> /dev/null; then
    echo "==> Missing Conan or CMake in PATH. Setting up a local Python virtual environment..."
    
    # Create venv if it doesn't exist
    if [ ! -d ".venv" ]; then
        python3 -m venv .venv
    fi
    
    # Activate venv
    source .venv/bin/activate
    
    # Install conan and cmake inside the venv
    echo "==> Installing Conan and CMake in virtual environment..."
    pip3 install conan cmake
else
    echo "==> Conan and CMake found in PATH."
fi

echo "==> Creating default Conan profile if it doesn't exist..."
conan profile detect --force || true
# Fix for macOS apple-clang 21.0 not being in the default conan settings.yml
sed -i.bak 's/compiler.version=21/compiler.version=15.0/g' ~/.conan2/profiles/default || true

# Install dependencies using Conan
echo "==> Installing dependencies via Conan..."
mkdir -p build
cd build

# Point conan and cmake to the trading-core directory
conan install ../trading-core --build=missing -s compiler.version=15.0

# Configure and Build using CMake
echo "==> Configuring CMake..."
cmake ../trading-core -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

echo "==> Building trading-core..."
cmake --build . --config Release

echo "==> Build successful! Executable is at build/trading-core"
