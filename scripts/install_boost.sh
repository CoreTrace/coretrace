#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

# Default installation directory and version
INSTALL_DIR="${CMAKE_BINARY_DIR}/boost_install"
BOOST_VERSION="1.82.0"
BOOST_UNDERSCORE_VERSION=${BOOST_VERSION//./_}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --prefix=*)
      INSTALL_DIR="${1#*=}"
      shift
      ;;
    --version=*)
      BOOST_VERSION="${1#*=}"
      BOOST_UNDERSCORE_VERSION=${BOOST_VERSION//./_}
      shift
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

echo "Installing Boost ${BOOST_VERSION} to ${INSTALL_DIR}"

# Create directories
mkdir -p "${INSTALL_DIR}"
mkdir -p /tmp/boost_build

# Download and extract
cd /tmp/boost_build

BOOST_URL="https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_UNDERSCORE_VERSION}.tar.gz"
BOOST_ARCHIVE="boost_${BOOST_UNDERSCORE_VERSION}.tar.gz"

if [ ! -f "${BOOST_ARCHIVE}" ]; then
  echo "Downloading Boost ${BOOST_VERSION}..."
  wget "${BOOST_URL}" -O "${BOOST_ARCHIVE}" || { echo "Failed to download Boost"; exit 1; }
  
  # Verify download
  if [ ! -s "${BOOST_ARCHIVE}" ]; then
    echo "Downloaded file is empty or corrupt"
    rm -f "${BOOST_ARCHIVE}"
    exit 1
  fi
fi

# Extract only if directory doesn't exist
if [ ! -d "boost_${BOOST_UNDERSCORE_VERSION}" ]; then
  echo "Extracting Boost..."
  tar -xzf "${BOOST_ARCHIVE}" || { echo "Failed to extract Boost archive"; exit 1; }
fi

# Check if extraction was successful
if [ ! -d "boost_${BOOST_UNDERSCORE_VERSION}" ]; then
  echo "Boost extraction failed. Directory not found."
  exit 1
fi

cd "boost_${BOOST_UNDERSCORE_VERSION}" || { echo "Failed to change to Boost directory"; exit 1; }

# Bootstrap and build
echo "Running bootstrap..."
./bootstrap.sh --prefix="${INSTALL_DIR}" || { echo "Bootstrap failed"; exit 1; }

echo "Building and installing Boost..."
./b2 install --prefix="${INSTALL_DIR}" --with-system --with-filesystem --with-program_options -j$(nproc) || { echo "Build failed"; exit 1; }

# Verify installation
if [ ! -d "${INSTALL_DIR}/include/boost" ]; then
  echo "Boost installation failed. Include directory not found."
  exit 1
fi

echo "Boost ${BOOST_VERSION} installed successfully to ${INSTALL_DIR}"
exit 0
