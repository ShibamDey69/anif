#!/usr/bin/env node
import { spawn, spawnSync } from "child_process";
import readline from "readline";

const file = process.argv[2];

if (!file) {
  console.error("Usage: anif <video>");
  process.exit(1);
}

function hasBinary(cmd) {
  const result = spawnSync(cmd, ["-version"], { stdio: "ignore" });
  return !result.error;
}

if (!hasBinary("ffmpeg") || !hasBinary("ffplay")) {
  console.error("anif needs ffmpeg and ffplay, and couldn't find them on your system.");
  console.error("");
  console.error("Install them with:");
  console.error("  macOS:    brew install ffmpeg");
  console.error("  Ubuntu:   sudo apt install ffmpeg");
  console.error("  Fedora:   sudo dnf install ffmpeg");
  console.error("  Arch:     sudo pacman -S ffmpeg");
  console.error("  Windows:  winget install --id Gyan.FFmpeg --version 9.0.1 --exact");
  console.error("            (or) choco install ffmpeg");
  console.error("");
  console.error("Or just run install.sh from the anif package, which does this for you.");
  process.exit(1);
}

const BPP = 3;

const tw = process.stdout.columns || 100;
const th = process.stdout.rows || 40;

const run = (cmd, args) =>
  new Promise((resolve, reject) => {
    const p = spawn(cmd, args);
    let out = "";

    p.stdout.on("data", d => (out += d));
    p.on("error", reject);
    p.on("close", c =>
      c === 0 ? resolve(out.trim()) : reject(Error(`${cmd} failed`))
    );
  });

const meta = JSON.parse(
  await run("ffprobe", [
    "-v", "error",
    "-select_streams", "v:0",
    "-show_entries", "stream=width,height,r_frame_rate",
    "-of", "json",
    file
  ])
).streams[0];

const ratio = Number(meta.width) / Number(meta.height);

let W = tw;
let H = Math.floor(W / ratio / 2);

if (H > th - 1) {
  H = th - 1;
  W = Math.floor(H * ratio * 2);
}

const FRAME = W * H * BPP;

const [num, den] = meta.r_frame_rate.split("/").map(Number);
const fps = num / (den || 1);
const frameDuration = 1000 / fps;

function draw(frame) {
  let out = "";

  for (let i = 0, p = 0; i < FRAME; i += 3, p++) {
    const r = frame[i];
    const g = frame[i + 1];
    const b = frame[i + 2];

    const y = (77 * r + 150 * g + 29 * b) >> 8;
    const c = y > 127 ? "1" : "0";

    out += `\x1b[38;2;${r};${g};${b}m${c}`;

    if ((p + 1) % W === 0) out += "\x1b[0m\n";
  }

  readline.cursorTo(process.stdout, 0, 0);
  process.stdout.write(out);
}

const current = { forceFinish: null };

let quit = false;

function killCurrent() {
  current.forceFinish?.();
}

function playOnce() {
  return new Promise(resolve => {
    let data = Buffer.alloc(0);
    let startTime = null;
    let frameIndex = 0;
    let finished = false;
    let ffmpegClosed = false;

    const audio = spawn("ffplay", [
      "-nodisp",
      "-autoexit",
      "-loglevel", "quiet",
      file
    ], {
      stdio: "ignore"
    });

    const ffmpeg = spawn("ffmpeg", [
      "-loglevel", "error",
      "-i", file,
      "-vf", `scale=${W}:${H}`,
      "-pix_fmt", "rgb24",
      "-f", "rawvideo",
      "pipe:1"
    ], {
      stdio: ["ignore", "pipe", "ignore"]
    });

    current.forceFinish = finish;

    function maybeFinish() {
      if (finished) return;
      if (ffmpegClosed && data.length < FRAME) {
        finish();
      }
    }

    function tick() {
      if (finished) return;

      if (startTime === null) {
        if (data.length < FRAME) return;
        startTime = Date.now();
      }

      const elapsed = Date.now() - startTime;
      const dueIndex = Math.floor(elapsed / frameDuration);

      if (dueIndex <= frameIndex) return;

      const available = Math.floor(data.length / FRAME);
      if (available === 0) {
        maybeFinish();
        return;
      }

      const wantSkip = ffmpegClosed
        ? 1
        : Math.min(dueIndex - frameIndex, available);
      const dropBytes = (wantSkip - 1) * FRAME;

      if (dropBytes > 0) {
        data = data.subarray(dropBytes);
      }

      const frame = data.subarray(0, FRAME);
      data = data.subarray(FRAME);

      draw(frame);

      frameIndex = dueIndex;

      maybeFinish();
    }

    const timer = setInterval(tick, Math.max(1, frameDuration / 2));

    ffmpeg.stdout.on("data", chunk => {
      data = Buffer.concat([data, chunk]);
    });

    function finish() {
      if (finished) return;
      finished = true;

      clearInterval(timer);

      ffmpeg.kill("SIGTERM");
      audio.kill("SIGTERM");

      current.forceFinish = null;

      resolve();
    }

    ffmpeg.on("close", () => {
      ffmpegClosed = true;
      maybeFinish();
    });
    ffmpeg.on("error", () => {
      ffmpegClosed = true;
      maybeFinish();
    });

    audio.on("error", () => {});
  });
}

readline.emitKeypressEvents(process.stdin);
process.stdin.setRawMode?.(true);

process.stdout.write("\x1b[?25l");

process.stdin.on("keypress", (_, key) => {
  if (key?.name === "q" || (key?.ctrl && key?.name === "c")) {
    quit = true;
    killCurrent();
  }
});

while (!quit) {
  process.stdout.write("\x1b[2J\x1b[H");
  await playOnce();
}

process.stdin.setRawMode?.(false);
process.stdout.write("\x1b[0m\x1b[?25h\n");
process.exit(0);
