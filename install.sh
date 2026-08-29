#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v node >/dev/null 2>&1; then
  echo "Node.js is required but wasn't found. Install Node 18+ first, then run this script again."
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1 || ! command -v ffplay >/dev/null 2>&1; then
  echo "Installing ffmpeg..."

  if command -v brew >/dev/null 2>&1; then
    brew install ffmpeg
  elif command -v apt >/dev/null 2>&1; then
    sudo apt update && sudo apt install -y ffmpeg
  elif command -v apt >/dev/null 2>&1; then
    pkg update -y && pkg install ffmpeg -y
  elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y ffmpeg
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm ffmpeg
  elif command -v winget >/dev/null 2>&1; then
    winget install --id Gyan.FFmpeg --version 9.0.1 --exact --silent --accept-package-agreements --accept-source-agreements
  elif command -v choco >/dev/null 2>&1; then
    choco install ffmpeg -y
  else
    echo "Couldn't detect a package manager. Install ffmpeg manually, then rerun this script."
    exit 1
  fi
fi

cd "$DIR"
npm install -g .

echo ""
echo "Done. Run it from anywhere with:"
echo "  anif <video file>"
