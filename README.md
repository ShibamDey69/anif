# anif

> Play video as high-performance live 24-bit TrueColor ASCII / ANSI art in your terminal. Written in pure C with no external libraries to build, using the ffmpeg/ffprobe/ffplay already on your system (or installed via your platform's own package manager).

---

## Features

- ⚡ **Pure C & Zero External Dependencies**: Written in standard C99 with zero external library requirements. Minimal memory footprint and instant startup.
- 📦 **Uses Your Package Manager**: `ffmpeg`, `ffprobe`, and `ffplay` are installed with the official package manager for your platform (`apt`/`dnf`/`yum`/`pacman`/`zypper`/`apk` on Linux, `brew` on macOS, `pkg` on Termux, `winget` on Windows) — no third-party binary mirrors involved.
- 🚀 **Multi-OS Installers**: `install.sh` detects your OS & CPU architecture on Linux, macOS, and Termux; `install.ps1` does the same natively on Windows. Both install the ffmpeg suite, then build/install the `anif` binary itself.
- 🎨 **24-bit TrueColor ANSI Rendering**: Full 24-bit RGB terminal rendering with run-length color sequence optimization for high frame rates without flickering.
- 🔲 **High-Definition Half-Block Mode (`-b`)**: Uses Unicode upper half-blocks (`▀`) with foreground and background colors to pack two vertical pixels into every terminal cell, delivering 2x vertical resolution and true 1:1 square pixel aspect ratio.
- 🖥️ **Alternate Screen Buffer Support**: Switches to the terminal's alternate screen buffer during playback and restores your terminal session cleanly upon exit without leaving artifacts or messing up scrollback history.
- 🔊 **Synchronized Audio Playback**: Audio playback in sync with video frames (via optional FFplay or audio sink), or smooth video playback if audio is disabled/unsupported.
- 🔁 **Looping & Responsive Controls**: Infinite or counted looping, custom character sets (`#`, custom strings, or 10-level luminance ramp), dynamic terminal resize adaptation, and instant responsive exit (`q`, `Q`, `ESC`, `Ctrl+C`).

---

## Installation

### Automatic Install

**Linux, macOS, and Termux:**

```bash
./install.sh
```

**Windows (PowerShell):**

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1
```

Each installer will:
1. Install `ffmpeg`, `ffprobe`, and `ffplay` with your platform's own default package manager, if they aren't already on your PATH — apt/dnf/yum/pacman/zypper/apk on Linux, Homebrew on macOS, `pkg` on Termux, or `winget` (falling back to Chocolatey) on Windows.
2. Use a prebuilt `anif` binary if one is bundled for your OS/architecture, or compile `anif` natively from source otherwise.
3. Install `anif` onto your PATH.
4. Run `anif --check` to confirm everything was found.

Nothing is downloaded from a third-party binary mirror — only your OS's own package sources are used.

---

### Manual Build

```bash
make
sudo make install
```

Then install `ffmpeg` (which provides `ffprobe` and `ffplay` too) with your package manager, e.g. `sudo apt install ffmpeg`, `brew install ffmpeg`, `sudo pacman -S ffmpeg`, `pkg install ffmpeg`, or `winget install ffmpeg`.

---

## How anif Finds FFmpeg

`anif` searches for `ffmpeg` (and `ffprobe`/`ffplay`) in the following order:

1. **Environment Variables**: `ANIF_FFMPEG_PATH` / `ANIF_FFPLAY_PATH` / `ANIF_FFPROBE_PATH`, or a directory via `ANIF_BIN_DIR`
2. **App Static Bin Directory**: `~/.local/share/anif/bin/` (or `$XDG_DATA_HOME/anif/bin/`) — a place to drop a portable build if you don't want to use a package manager
3. **Executable Directory**: Same folder as `anif` (`./bin/ffmpeg` or `./ffmpeg`)
4. **System PATH**: Standard system-wide `ffmpeg` binary — this is where `install.sh`/`install.ps1` and every package manager listed above install it

### FFmpeg Not Found

If `ffmpeg` isn't found at runtime, `anif` prints the install command for your platform (and points to `install.sh`/`install.ps1`). You can also see this at any time with:

```bash
anif --download-ffmpeg
```

---

## Usage

```bash
anif [options] <video_file>
```

### Examples

```bash
# High-definition TrueColor Half-Block mode (2x vertical resolution)
anif -b video.mp4

# Play video with default ASCII characters (infinite loop)
anif video.mp4

# Play once without looping
anif --once video.mp4

# Play using ASCII luminance density ramp ( .:-=+*#%@)
anif -L video.mp4

# Play with a custom character or emoji
anif -c "@" video.mp4

# Override width and height
anif -W 80 -H 30 video.mp4

# Play without audio
anif --no-audio video.mp4

# Check detected binary paths
anif --check
```

### CLI Options

| Flag | Description |
|---|---|
| `-b, --block` | Use high-definition half-block TrueColor mode (`▀`, 2x vertical resolution) |
| `-c, --char <str>` | Character to use for rendering (default: `#`) |
| `-L, --luminance` | Use ASCII density ramp (` .:-=+*#%@`) based on pixel luminance |
| `-W, --width <cols>` | Override rendering width in terminal columns |
| `-H, --height <rows>` | Override rendering height in terminal rows |
| `-f, --fps <rate>` | Override playback frame rate |
| `-1, --once` | Play video once (disable looping) |
| `-l, --loop [N]` | Loop playback (default: infinite, or specify count `N`) |
| `--no-audio` | Disable audio playback |
| `--check` | Display detected FFmpeg binary path and status |
| `--download-ffmpeg` | Show the ffmpeg install command for your platform |
| `--ffmpeg <path>` | Path to custom `ffmpeg` binary |
| `--ffplay <path>` | Path to custom `ffplay` binary (optional) |
| `--ffprobe <path>` | Path to custom `ffprobe` binary (optional) |
| `-v, --version` | Display version information |
| `-h, --help` | Display help message |

### Keyboard Controls

- `q`, `Q`, `ESC`, or `Ctrl+C` : Quit playback immediately

---

## Uninstall

```bash
sudo make uninstall
```

