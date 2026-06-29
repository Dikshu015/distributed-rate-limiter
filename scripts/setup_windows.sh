#!/usr/bin/env bash
# Windows setup for distributed-rate-limiter, using MSYS2 + Docker.
# ... (header comments unchanged) ...

set -euo pipefail

echo "=================================================="
echo "Step 1: Checking if MSYS2 system is up to date"
echo "=================================================="
# pacman -Syu prints "nothing to do" on stdout when fully updated.
# We capture its output and refuse to continue if an update is needed,
# rather than assuming the human already ran it.
UPDATE_CHECK=$(pacman -Syu --noconfirm 2>&1) || true
echo "$UPDATE_CHECK"

if ! echo "$UPDATE_CHECK" | grep -q "nothing to do"; then
  echo ""
  echo "!! System update was applied just now (or is still in progress)."
  echo "!! If pacman asked you to restart your terminal, do so now, then"
  echo "!! re-run this script from the beginning."
  echo ""
  read -p "Press Enter to continue ONLY if no restart was requested: "
fi

echo "=================================================="
echo "Step 2: Install the C++ toolchain"
echo "=================================================="
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain

echo "=================================================="
echo "Step 3: Install the Redis C++ client libraries"
echo "=================================================="
pacman -S --needed mingw-w64-ucrt-x86_64-hiredis mingw-w64-ucrt-x86_64-redis-plus-plus

echo "=================================================="
echo "Step 4: Install CMake and Make"
echo "=================================================="
pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make

echo "=================================================="
echo "Step 5: Confirm Docker Desktop is running"
echo "=================================================="
if ! docker ps > /dev/null 2>&1; then
  echo "!! Docker doesn't seem to be running."
  echo "!! Open Docker Desktop manually (Start menu), wait for 'Engine"
  echo "!! running', then re-run this script."
  exit 1
fi
docker --version
docker ps

echo "=================================================="
echo "Step 6: Start a local Redis container"
echo "=================================================="
if docker ps -a --format '{{.Names}}' | grep -q '^redis-dev$'; then
  echo "redis-dev container already exists."
  if docker ps --format '{{.Names}}' | grep -q '^redis-dev$'; then
    echo "It's already running."
  else
    echo "It exists but is stopped — starting it now."
    docker start redis-dev
  fi
else
  echo "No existing redis-dev container found — creating one."
  docker run -d --name redis-dev -p 6379:6379 redis:7-alpine
fi

echo ""
echo "=================================================="
echo "Setup complete. Next steps to build the project:"
echo "=================================================="
echo "  mkdir -p build && cd build"
echo "  cmake .. -G \"MinGW Makefiles\""
echo "  cmake --build ."
echo "  cd .."
echo "  ./build/server.exe node_A     # must run from project root, not build/"