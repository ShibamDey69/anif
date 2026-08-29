```markdown
# anif

Play a video as live color ASCII art, with audio, in your terminal.

## Requirements

- **Node.js 18+** — required on every platform. If you don't have it, install it first from https://nodejs.org before doing anything else below.
- **ffmpeg** (which includes `ffplay`) — see install steps per OS below.

## Install

### macOS / Linux

Unzip this package, then from inside the folder:

```
./install.sh
```

This installs ffmpeg (if missing) using your system's package manager (brew, apt, dnf, or pacman) and links the `anif` command globally.

If you'd rather do it by hand:

```
brew install ffmpeg        # macOS
sudo apt install ffmpeg    # Ubuntu/Debian
sudo dnf install ffmpeg    # Fedora
sudo pacman -S ffmpeg      # Arch
npm install -g .
```

### Windows

There's no install script for Windows — it has to be done manually:

1. Install Node.js 18+ from https://nodejs.org if you haven't already.
2. Install ffmpeg with winget (Command Prompt or PowerShell):

   ```

   winget install --id Gyan.FFmpeg --version 9.0.1 --exact
   ```

3. Open a **new** terminal window (PATH changes don't apply to already-open terminals) and confirm it worked:

   ```
   ffmpeg -version
   ```

4. From inside the unzipped `anif` folder, run:

   ```
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
```
