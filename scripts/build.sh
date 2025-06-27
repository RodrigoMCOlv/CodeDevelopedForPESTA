#!/bin/bash
set -e
set -u
set -o pipefail

# --- Configuration Variables ---
# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo "Script directory: $SCRIPT_DIR"

# Define the root of your project (can_bridge)
# This assumes build.sh is in can_bridge/scripts
PROJECT_ROOT="${SCRIPT_DIR}/.."
echo "Project root: $PROJECT_ROOT"

# Define the source directory containing CMakeLists.txt and your code
SOURCE_DIR="${PROJECT_ROOT}/software"
echo "Source directory: $SOURCE_DIR"

# Define the build directory (e.g., can_bridge/build)
BUILD_DIR="${PROJECT_ROOT}/build"
echo "Build directory: $BUILD_DIR"

# Ensure PICO_SDK_PATH is set (it should be from your .bashrc)
# If it's not set, this script will exit due to 'set -u'
if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "Error: PICO_SDK_PATH environment variable is not set."
    echo "Please set it in your .bashrc (e.g., export PICO_SDK_PATH=\"/home/rodrigo/Documents/pico-sdk\") and source it."
    exit 1
fi
echo "PICO_SDK_PATH: $PICO_SDK_PATH"


# --- Functions ---

# Function to display usage information
usage() {
    echo "Usage: ./build.sh [clean|build|all]"
    echo "  clean : Removes the build directory."
    echo "  build : Configures and builds the project using CMake."
    echo "  all   : Performs clean and then build."
    exit 1
}

# Cleanup function
clean() {
    echo "--- Cleaning up build directory ---"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo "Removed: $BUILD_DIR"
    else
        echo "Build directory not found, nothing to clean."
    fi
}

# Build function
build() {
    echo "--- Creating build directory ---"
    mkdir -p "${BUILD_DIR}"

    echo "--- Configuring project with CMake ---"
    # Navigate to the build directory and run CMake from there,
    # pointing it to the source directory and explicitly passing PICO_SDK_PATH.
    cd "${BUILD_DIR}"
    cmake "${SOURCE_DIR}" -DPICO_SDK_PATH="${PICO_SDK_PATH}" # <--- KEY CHANGE HERE

    echo "--- Building project with Make ---"
    make -j$(nproc)

    # Optional: If your CMakeLists.txt defines an install target for the main executable
    # and you want to install it to a specific local folder:
    # echo "--- Installing project ---"
    # make install

    echo "Build process complete for RP2040."
}

# --- Main Logic ---
case "${1:-}" in
    clean)
        clean
        ;;
    build)
        build
        ;;
    all)
        clean
        build
        ;;
    *)
        usage
        ;;
esac

echo "--- Script finished ---"