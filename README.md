# anif

> Play video as high-performance live 24-bit TrueColor ASCII / ANSI art in your terminal. Written in pure C with zero runtime dependencies and single static FFmpeg binary support.

---

## Features

- ⚡ **Pure C & Zero External Dependencies**: Written in standard C99 with zero external library requirements. Minimal memory footprint and instant startup.
- 📦 **Single Static FFmpeg Binary**: Only requires a single standalone `ffmpeg` binary. Probing, video decoding, and playback work 100% self-contained without needing `ffprobe`, `ffplay`, or system package managers (`apt`, `brew`, `pkg`, `dnf`, `pacman`).
- 🚀 **Multi-OS Binaries & Automatic Platform Installation**: Installer automatically detects your operating system & CPU architecture (Linux x86_64, Linux aarch64, Linux armhf, macOS Intel, macOS Apple Silicon, Android/Termux, Windows) and sets up the appropriate binary immediately.
- 🎨 **24-bit TrueColor ANSI Rendering**: Full 24-bit RGB terminal rendering with run-length color sequence optimization for high frame rates without flickering.
- 🔲 **High-Definition Half-Block Mode (`-b`)**: Uses Unicode upper half-blocks (`▀`) with foreground and background colors to pack two vertical pixels into every terminal cell, delivering 2x vertical resolution and true 1:1 square pixel aspect ratio.
- 🖥️ **Alternate Screen Buffer Support**: Switches to the terminal's alternate screen buffer during playback and restores your terminal session cleanly upon exit without leaving artifacts or messing up scrollback history.
- 🔊 **Synchronized Audio Playback**: Audio playback in sync with video frames (via optional FFplay or audio sink), or smooth video playback if audio is disabled/unsupported.
- 🔁 **Looping & Responsive Controls**: Infinite or counted looping, custom character sets (`#`, custom strings, or 10-level luminance ramp), dynamic terminal resize adaptation, and instant responsive exit (`q`, `Q`, `ESC`, `Ctrl+C`).

---

## Installation

### Automatic Install (Linux / macOS / Termux / Windows)

Run the universal installer:

```bash
./install.sh
```

The installer will:
1. Automatically detect your OS and CPU architecture.
2. Use the prebuilt binary or compile natively from source.
3. Automatically configure the standalone static single `ffmpeg` binary if not already present on your system (no root or package manager required).
4. Install `anif` into your PATH.

---

### Manual Build

```bash
make
sudo make install
```

To fetch the single static `ffmpeg` binary:

```bash
make fetch-ffmpeg
```

---

## Static Single FFmpeg Binary Support

`anif` searches for `ffmpeg` in the following order:

1. **Environment Variables**: `ANIF_FFMPEG_PATH` or `ANIF_BIN_DIR`
2. **App Static Bin Directory**: `~/.local/share/anif/bin/ffmpeg` (or `$XDG_DATA_HOME/anif/bin/ffmpeg`)
3. **Executable Directory**: Same folder as `anif` (`./bin/ffmpeg` or `./ffmpeg`)
4. **System PATH**: Standard system-wide `ffmpeg` binary

### Automatic Download

If `ffmpeg` is missing, run:

```bash
anif --download-ffmpeg
```

Or run `anif` interactively; it will automatically download the standalone static single binary for your platform.

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
| `--download-ffmpeg` | Download standalone single static FFmpeg binary for current platform |
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

