#!/bin/bash

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
if [ ! -f "boost_${BOOST_UNDERSCORE_VERSION}.tar.gz" ]; then
  echo "Downloading Boost ${BOOST_VERSION}..."
  wget -q "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_UNDERSCORE_VERSION}.tar.gz"
fi

tar -xzf "boost_${BOOST_UNDERSCORE_VERSION}.tar.gz"
cd "boost_${BOOST_UNDERSCORE_VERSION}"

# Bootstrap and build
./bootstrap.sh --prefix="${INSTALL_DIR}"
./b2 install --prefix="${INSTALL_DIR}" --with-system --with-filesystem --with-program_options -j$(nproc)

echo "Boost ${BOOST_VERSION} installed successfully to ${INSTALL_DIR}"
exit 0
