# anif

Play a video as live color ASCII art, with audio, in your terminal.

## Install

Requires Node 18+. Unzip this package, then from inside the folder:

```
./install.sh
```

This installs ffmpeg (if missing) using your system's package manager
(brew, apt, dnf, pacman, or choco) and links the `anif` command globally.

If you'd rather do it by hand:

```
brew install ffmpeg        # macOS
sudo apt install ffmpeg    # Ubuntu/Debian
sudo dnf install ffmpeg    # Fedora
sudo pacman -S ffmpeg      # Arch
winget install --id Gyan.FFmpeg --version 9.0.1 --exact   # Windows
npm install -g .
```

## Usage

```
anif video.mp4
```

Press `q` or `Ctrl+C` to quit. The video loops automatically until you do.

## Uninstall

```
npm uninstall -g anif
```
