#!/bin/sh
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

printf "\033[1;36m===================================================\033[0m\n"
printf "\033[1;36m          anif — ASCII Video Player Installer      \033[0m\n"
printf "\033[1;36m===================================================\033[0m\n"

# 1. Detect Operating System and Architecture
OS_RAW="$(uname -s)"
ARCH_RAW="$(uname -m)"

OS_NAME="unknown"
ARCH_NAME="unknown"

case "$OS_RAW" in
    Linux)
        if [ -n "$PREFIX" ] && [ -d "$PREFIX" ] && echo "$PREFIX" | grep -q "com.termux"; then
            OS_NAME="android"
        elif [ -d "/data/data/com.termux" ]; then
            OS_NAME="android"
        else
            OS_NAME="linux"
        fi
        ;;
    Darwin)
        OS_NAME="darwin"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        OS_NAME="windows"
        ;;
    *)
        OS_NAME="linux"
        ;;
esac

case "$ARCH_RAW" in
    x86_64|amd64)
        ARCH_NAME="x86_64"
        ;;
    aarch64|arm64|armv8*)
        ARCH_NAME="aarch64"
        ;;
    armv7*|armhf|arm)
        ARCH_NAME="armhf"
        ;;
    i686|i386|x86)
        ARCH_NAME="x86"
        ;;
    *)
        ARCH_NAME="$ARCH_RAW"
        ;;
esac

printf "\033[1;34m==>\033[0m Detected Platform: \033[1;32m%s-%s\033[0m (%s %s)\n" "$OS_NAME" "$ARCH_NAME" "$OS_RAW" "$ARCH_RAW"

# 2. Acquire or Build anif binary
PREBUILT_BIN="bin/anif-${OS_NAME}-${ARCH_NAME}"
if [ "$OS_NAME" = "windows" ]; then
    PREBUILT_BIN="bin/anif-${OS_NAME}-${ARCH_NAME}.exe"
fi

if [ -f "$PREBUILT_BIN" ] && [ -x "$PREBUILT_BIN" ]; then
    printf "\033[1;34m==>\033[0m Found prebuilt binary: %s\n" "$PREBUILT_BIN"
    cp "$PREBUILT_BIN" ./anif
    chmod 755 ./anif
elif command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 || command -v clang >/dev/null 2>&1; then
    CC="cc"
    if command -v gcc >/dev/null 2>&1; then CC="gcc"; fi
    if command -v clang >/dev/null 2>&1; then CC="clang"; fi
    printf "\033[1;34m==>\033[0m Compiling anif natively with %s...\n" "$CC"
    if command -v make >/dev/null 2>&1; then
        make clean >/dev/null 2>&1 || true
        make CC="$CC"
    else
        $CC -Iinclude -O3 -Wall -Wextra -std=c99 src/*.c -o anif -lm
    fi
    mkdir -p bin
    cp ./anif "$PREBUILT_BIN" 2>/dev/null || true
else
    printf "\033[1;31mError:\033[0m No prebuilt binary found for %s-%s and no C compiler installed.\n" "$OS_NAME" "$ARCH_NAME"
    exit 1
fi

# 3. Check for single static FFmpeg binary or download it automatically
printf "\033[1;34m==>\033[0m Checking for FFmpeg binary...\n"
STATIC_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anif/bin"
mkdir -p "$STATIC_DIR"

if ! command -v ffmpeg >/dev/null 2>&1 && [ ! -x "$STATIC_DIR/ffmpeg" ] && [ ! -x "./bin/ffmpeg" ]; then
    printf "\033[1;33m[anif]\033[0m FFmpeg binary not found. Downloading standalone static single FFmpeg binary...\n"
    
    # Platform-specific static binary download URL
    FF_URL=""
    case "${OS_NAME}-${ARCH_NAME}" in
        linux-x86_64)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-64.zip"
            ;;
        linux-aarch64|android-aarch64)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-arm-64.zip"
            ;;
        linux-armhf|android-armhf)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-armhf-32.zip"
            ;;
        darwin-*)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-osx-64.zip"
            ;;
        windows-x86_64)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-win-64.zip"
            ;;
        *)
            FF_URL="https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-64.zip"
            ;;
    esac

    if [ -n "$FF_URL" ]; then
        TMP_ZIP="$STATIC_DIR/ffmpeg_tmp.zip"
        printf "\033[1;34m==>\033[0m Downloading: %s\n" "$FF_URL"
        if command -v curl >/dev/null 2>&1; then
            curl -L -f --progress-bar -o "$TMP_ZIP" "$FF_URL"
        elif command -v wget >/dev/null 2>&1; then
            wget -q --show-progress -O "$TMP_ZIP" "$FF_URL"
        fi

        if [ -f "$TMP_ZIP" ]; then
            printf "\033[1;34m==>\033[0m Extracting single static ffmpeg binary to %s...\n" "$STATIC_DIR"
            if command -v unzip >/dev/null 2>&1; then
                unzip -o -q "$TMP_ZIP" -d "$STATIC_DIR"
            elif command -v python3 >/dev/null 2>&1; then
                python3 -c "import zipfile; zipfile.ZipFile('$TMP_ZIP').extractall('$STATIC_DIR')"
            fi
            rm -f "$TMP_ZIP"
            chmod 755 "$STATIC_DIR"/ffmpeg* 2>/dev/null || true
        fi
    fi
fi

# 4. Install anif binary to system or user bin directory
INSTALL_DIR=""
SUDO=""

if [ -n "$PREFIX" ] && [ -d "$PREFIX/bin" ] && [ -w "$PREFIX/bin" ]; then
    INSTALL_DIR="$PREFIX/bin"
elif [ -w "/usr/local/bin" ]; then
    INSTALL_DIR="/usr/local/bin"
elif [ -d "$HOME/.local/bin" ] || mkdir -p "$HOME/.local/bin" 2>/dev/null; then
    INSTALL_DIR="$HOME/.local/bin"
elif command -v sudo >/dev/null 2>&1; then
    INSTALL_DIR="/usr/local/bin"
    SUDO="sudo"
else
    INSTALL_DIR="$HOME/.local/bin"
    mkdir -p "$INSTALL_DIR" 2>/dev/null || true
fi

printf "\033[1;34m==>\033[0m Installing anif binary to \033[1;32m%s/anif\033[0m...\n" "$INSTALL_DIR"
if [ -n "$SUDO" ]; then
    $SUDO cp ./anif "$INSTALL_DIR/anif"
    $SUDO chmod 755 "$INSTALL_DIR/anif"
else
    cp ./anif "$INSTALL_DIR/anif"
    chmod 755 "$INSTALL_DIR/anif"
fi

# 5. Verify installation
printf "\n"
./anif --check || true

printf "\n"
printf "\033[1;32m===================================================\033[0m\n"
printf "\033[1;32m✓ anif has been successfully installed!\033[0m\n"
printf "\033[1;32m===================================================\033[0m\n"
printf "\n"
printf "Usage:\n"
printf "  anif <video_file>           # Default TrueColor ASCII playback\n"
printf "  anif -b <video_file>        # High-definition Half-Block mode (2x resolution)\n"
printf "  anif -L <video_file>        # Luminance ramp mode\n"
printf "  anif --once <video_file>    # Play once without looping\n"
printf "\n"
printf "Example:\n"
printf "  anif video.mp4\n\n"

