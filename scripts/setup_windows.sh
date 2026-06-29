#!/usr/bin/env bash
# Windows setup for distributed-rate-limiter, using MSYS2 + Docker.
#
# This project was developed on Windows using the MSYS2 UCRT64
# environment for the C++ toolchain, and Docker Desktop for running
# Redis (Redis has no official native Windows build).
#
# Run the steps below from the MSYS2 UCRT64 terminal specifically
# (launch via C:\msys64\ucrt64.exe), not plain MSYS2 or Git Bash.
# Builds and links must happen from this UCRT64 shell — other MSYS2
# sub-environments are known to fail at the linking step on this setup.

set -euo pipefail

echo "== Step 1: Update the MSYS2 system =="
echo "Run this first, and re-run it if it asks you to restart the terminal:"
echo "  pacman -Syu"
echo ""

echo "== Step 2: Install the C++ toolchain =="
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain

echo "== Step 3: Install the Redis C++ client libraries =="
pacman -S --needed mingw-w64-ucrt-x86_64-hiredis mingw-w64-ucrt-x86_64-redis-plus-plus

echo "== Step 4: Install CMake and Make =="
pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make

echo "== Step 5: Confirm Docker Desktop is running =="
echo "Open Docker Desktop manually first, then verify:"
docker --version
docker ps

echo "== Step 6: Start a local Redis container =="
echo "If this is your first time, create the container:"
echo "  docker run -d --name redis-dev -p 6379:6379 redis:7-alpine"
echo "If the container already exists but is stopped:"
echo "  docker start redis-dev"

echo ""
echo "Setup complete. Next steps:"
echo "  mkdir -p build && cd build"
echo "  cmake .. -G \"MinGW Makefiles\""
echo "  cmake --build ."
echo "  cd .."
echo "  ./build/server.exe node_A     # must run from project root, not build/"