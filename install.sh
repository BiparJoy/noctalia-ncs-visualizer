#!/usr/bin/env bash
# Builds and installs the aurora-ncs renderer into ~/.local/bin (no sudo).
# The Noctalia plugin itself is installed from Noctalia's plugin browser.
#
#   ./install.sh            build + install to ~/.local
#   PREFIX=/usr/local sudo -E ./install.sh   system-wide, if you prefer
set -euo pipefail

cd "$(dirname "$0")/renderer"
PREFIX="${PREFIX:-$HOME/.local}"

missing=()
command -v cmake >/dev/null || missing+=(cmake)
command -v c++ >/dev/null || missing+=("a C++ compiler")
command -v cava >/dev/null || missing+=(cava)
if ! command -v qmake6 >/dev/null && ! command -v qtpaths6 >/dev/null; then
    missing+=("Qt 6 development files")
fi
if ((${#missing[@]})); then
    echo "Missing: ${missing[*]}" >&2
    cat >&2 <<'HELP'

Install the build dependencies first:
  Fedora / Nobara : sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel layer-shell-qt-devel cava
  Arch            : sudo pacman -S cmake base-devel qt6-base qt6-declarative layer-shell-qt cava
  Debian / Ubuntu : sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev liblayershellqtinterface-dev cava
  openSUSE        : sudo zypper install cmake gcc-c++ qt6-base-devel qt6-declarative-devel layer-shell-qt6-devel cava
HELP
    exit 1
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build -j"$(nproc)"
cmake --install build

echo
echo "Installed $PREFIX/bin/aurora-ncs"
echo "Next: Noctalia Settings -> Plugins -> install \"NCS Visualizer\","
echo "then add the \"NCS Visualizer\" widget in the desktop widget editor."
