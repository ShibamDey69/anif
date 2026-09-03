#!/bin/sh
# anif installer -- Linux, macOS, and Termux (Android)
#
# ffmpeg, ffprobe, and ffplay are installed with the platform's own default
# package manager (apt/dnf/yum/pacman/zypper/apk on Linux, Homebrew on macOS,
# pkg on Termux). Nothing is downloaded from a third-party binary mirror.
#
# Windows users: run install.ps1 in PowerShell instead of this script.
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

BLUE="$(printf '\033[1;34m')"; CYAN="$(printf '\033[1;36m')"
GREEN="$(printf '\033[1;32m')"; YELLOW="$(printf '\033[1;33m')"
RED="$(printf '\033[1;31m')"; RESET="$(printf '\033[0m')"

info() { printf "%s==>%s %s\n" "$BLUE" "$RESET" "$1"; }
ok()   { printf "%s[ok]%s %s\n" "$GREEN" "$RESET" "$1"; }
warn() { printf "%s[warn]%s %s\n" "$YELLOW" "$RESET" "$1"; }
fail() { printf "%sError:%s %s\n" "$RED" "$RESET" "$1" >&2; }

printf "%s===================================================%s\n" "$CYAN" "$RESET"
printf "%s          anif - ASCII Video Player Installer      %s\n" "$CYAN" "$RESET"
printf "%s===================================================%s\n" "$CYAN" "$RESET"

# ---------------------------------------------------------------------------
# 1. Detect operating system and CPU architecture
# ---------------------------------------------------------------------------
OS_RAW="$(uname -s)"
ARCH_RAW="$(uname -m)"
OS_NAME="linux"
ARCH_NAME="$ARCH_RAW"

case "$OS_RAW" in
    Linux)
        if { [ -n "$PREFIX" ] && [ -d "$PREFIX" ] && echo "$PREFIX" | grep -q "com.termux"; } || [ -d "/data/data/com.termux" ]; then
            OS_NAME="android"
        else
            OS_NAME="linux"
        fi
        ;;
    Darwin)
        OS_NAME="darwin"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        fail "Detected $OS_RAW. This script covers Linux, macOS, and Termux only."
        fail "On native Windows, run install.ps1 in PowerShell instead:"
        fail "  powershell -ExecutionPolicy Bypass -File install.ps1"
        exit 1
        ;;
    *)
        OS_NAME="linux"
        ;;
esac

case "$ARCH_RAW" in
    x86_64|amd64)          ARCH_NAME="x86_64" ;;
    aarch64|arm64|armv8*)  ARCH_NAME="aarch64" ;;
    armv7*|armhf|arm)      ARCH_NAME="armhf" ;;
    i686|i386|x86)         ARCH_NAME="x86" ;;
esac

info "Detected platform: ${OS_NAME}-${ARCH_NAME} (${OS_RAW} ${ARCH_RAW})"

# ---------------------------------------------------------------------------
# 2. Install ffmpeg, ffprobe, and ffplay via the platform's package manager
# ---------------------------------------------------------------------------
SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
fi

have_ffmpeg_suite() {
    command -v ffmpeg >/dev/null 2>&1 && command -v ffprobe >/dev/null 2>&1 && command -v ffplay >/dev/null 2>&1
}

# Print a clear per-binary status line for ffmpeg/ffprobe/ffplay individually,
# rather than only reporting the group as a whole.
report_ffmpeg_suite() {
    all_present=1
    if command -v ffmpeg >/dev/null 2>&1; then
        ok "ffmpeg  -> $(command -v ffmpeg)"
    else
        fail "ffmpeg  -> NOT FOUND (required)"
        all_present=0
    fi
    if command -v ffprobe >/dev/null 2>&1; then
        ok "ffprobe -> $(command -v ffprobe)"
    else
        warn "ffprobe -> not found (optional, used for faster/more accurate metadata probing)"
        all_present=0
    fi
    if command -v ffplay >/dev/null 2>&1; then
        ok "ffplay  -> $(command -v ffplay)"
    else
        warn "ffplay  -> not found (optional, used for audio playback)"
        all_present=0
    fi
    [ "$all_present" -eq 1 ]
}

install_linux_ffmpeg() {
    if command -v apt-get >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via apt (bundled in one package)..."
        if $SUDO apt-get update && $SUDO apt-get install -y ffmpeg; then
            ok "apt install finished."
        else
            warn "apt-get install failed. Try: sudo apt-get install ffmpeg"
        fi
        return
    fi

    if command -v dnf >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via dnf..."
        if $SUDO dnf install -y ffmpeg; then
            ok "dnf install finished."
        elif $SUDO dnf install -y ffmpeg-free; then
            ok "ffmpeg-free installed (ffmpeg, ffprobe, ffplay with fewer codecs)."
            warn "For full codec support, enable RPM Fusion: https://rpmfusion.org/Configuration"
        else
            warn "dnf install failed. Try: sudo dnf install ffmpeg"
        fi
        return
    fi

    if command -v yum >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via yum..."
        if $SUDO yum install -y ffmpeg || $SUDO yum install -y ffmpeg-free; then
            ok "yum install finished."
        else
            warn "yum install failed. On RHEL/CentOS this usually needs EPEL + RPM Fusion enabled first."
        fi
        return
    fi

    if command -v pacman >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via pacman..."
        if $SUDO pacman -Sy --noconfirm ffmpeg; then
            ok "pacman install finished."
        else
            warn "pacman install failed. Try: sudo pacman -S ffmpeg"
        fi
        return
    fi

    if command -v zypper >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via zypper..."
        if $SUDO zypper --non-interactive install ffmpeg; then
            ok "zypper install finished."
        else
            warn "zypper install failed. Try: sudo zypper install ffmpeg"
        fi
        return
    fi

    if command -v apk >/dev/null 2>&1; then
        info "Installing ffmpeg, ffprobe, and ffplay via apk..."
        if $SUDO apk add ffmpeg; then
            ok "apk install finished."
        else
            warn "apk add failed. Try: sudo apk add ffmpeg"
        fi
        return
    fi

    warn "No supported package manager found (apt, dnf, yum, pacman, zypper, apk)."
    warn "Install ffmpeg, ffprobe, and ffplay manually: https://ffmpeg.org/download.html"
}

install_ffmpeg_suite() {
    case "$OS_NAME" in
        android)
            # Termux splits its ffmpeg build across two separate packages:
            #   pkg install ffmpeg  -> gives you ffmpeg AND ffprobe
            #   pkg install ffplay  -> a separate package, needed for ffplay
            # so ffmpeg/ffprobe and ffplay are fetched with two distinct
            # commands here, retrying each individually if still missing.
            if ! command -v ffmpeg >/dev/null 2>&1 || ! command -v ffprobe >/dev/null 2>&1; then
                info "Installing ffmpeg + ffprobe via Termux's official pkg repository..."
                if pkg install -y ffmpeg; then
                    ok "ffmpeg + ffprobe package installed."
                else
                    warn "pkg install ffmpeg failed. Try running: pkg install ffmpeg"
                fi
            fi
            if ! command -v ffplay >/dev/null 2>&1; then
                info "Installing ffplay via Termux's official pkg repository (separate package)..."
                if pkg install -y ffplay 2>/dev/null; then
                    ok "ffplay package installed."
                else
                    warn "ffplay package unavailable/failed (optional; audio playback will be skipped)."
                    warn "You can retry manually with: pkg install ffplay"
                fi
            fi
            ;;
        darwin)
            if ! command -v brew >/dev/null 2>&1; then
                fail "Homebrew was not found. macOS has no built-in package manager;"
                fail "Homebrew is the standard one. Install it from https://brew.sh"
                fail "then re-run this script."
                return
            fi
            info "Installing ffmpeg, ffprobe, and ffplay via Homebrew (one formula, all three)..."
            if brew install ffmpeg; then
                ok "brew install finished."
            else
                warn "brew install ffmpeg failed. Try running it manually."
            fi
            ;;
        *)
            install_linux_ffmpeg
            ;;
    esac
}

if have_ffmpeg_suite; then
    ok "ffmpeg, ffprobe, and ffplay are already installed."
else
    install_ffmpeg_suite
    printf "\n"
    info "ffmpeg suite status after install:"
    report_ffmpeg_suite || warn "ffprobe/ffplay are optional -- anif still works with just ffmpeg, but with reduced probing/audio support."
fi

# ---------------------------------------------------------------------------
# 3. Acquire or build the anif binary
# ---------------------------------------------------------------------------
PREBUILT_BIN="bin/anif-${OS_NAME}-${ARCH_NAME}"

if [ -f "$PREBUILT_BIN" ] && [ -x "$PREBUILT_BIN" ]; then
    info "Found prebuilt binary: $PREBUILT_BIN"
    cp "$PREBUILT_BIN" ./anif
    chmod 755 ./anif
elif command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 || command -v clang >/dev/null 2>&1; then
    CC="cc"
    command -v gcc   >/dev/null 2>&1 && CC="gcc"
    command -v clang >/dev/null 2>&1 && CC="clang"
    info "Compiling anif natively with $CC..."
    if command -v make >/dev/null 2>&1; then
        make clean >/dev/null 2>&1 || true
        make CC="$CC"
    else
        $CC -Iinclude -O3 -Wall -Wextra -std=c99 src/*.c -o anif -lm
    fi
    mkdir -p bin
    cp ./anif "$PREBUILT_BIN" 2>/dev/null || true
else
    fail "No prebuilt binary for $OS_NAME-$ARCH_NAME and no C compiler (cc/gcc/clang) found."
    fail "Install a C compiler (e.g. build-essential / Xcode Command Line Tools / clang) and re-run."
    exit 1
fi

# ---------------------------------------------------------------------------
# 4. Install the anif binary onto PATH
# ---------------------------------------------------------------------------
INSTALL_DIR=""
BIN_SUDO=""

if [ -n "$PREFIX" ] && [ -d "$PREFIX/bin" ] && [ -w "$PREFIX/bin" ]; then
    INSTALL_DIR="$PREFIX/bin"
elif [ -w "/usr/local/bin" ]; then
    INSTALL_DIR="/usr/local/bin"
elif [ -d "$HOME/.local/bin" ] || mkdir -p "$HOME/.local/bin" 2>/dev/null; then
    INSTALL_DIR="$HOME/.local/bin"
elif command -v sudo >/dev/null 2>&1; then
    INSTALL_DIR="/usr/local/bin"
    BIN_SUDO="sudo"
else
    INSTALL_DIR="$HOME/.local/bin"
    mkdir -p "$INSTALL_DIR" 2>/dev/null || true
fi

info "Installing anif binary to $INSTALL_DIR/anif..."
$BIN_SUDO cp ./anif "$INSTALL_DIR/anif"
$BIN_SUDO chmod 755 "$INSTALL_DIR/anif"

case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *) warn "$INSTALL_DIR is not on your PATH. Add this to your shell profile:"
       warn "  export PATH=\"$INSTALL_DIR:\$PATH\"" ;;
esac

# ---------------------------------------------------------------------------
# 5. Verify installation
# ---------------------------------------------------------------------------
printf "\n"
./anif --check || true

printf "\n%s===================================================%s\n" "$GREEN" "$RESET"
printf "%sanif has been successfully installed!%s\n" "$GREEN" "$RESET"
printf "%s===================================================%s\n\n" "$GREEN" "$RESET"
printf "Usage:\n"
printf "  anif <video_file>           # Default TrueColor ASCII playback\n"
printf "  anif -b <video_file>        # High-definition Half-Block mode (2x resolution)\n"
printf "  anif -L <video_file>        # Luminance ramp mode\n"
printf "  anif --once <video_file>    # Play once without looping\n\n"
printf "Example:\n"
printf "  anif video.mp4\n\n"
