#Requires -Version 5.1
<#
    anif installer -- Windows (PowerShell)

    ffmpeg, ffprobe, and ffplay are installed with winget, the official
    Windows Package Manager, falling back to Chocolatey if winget isn't
    available. Nothing is downloaded from a third-party binary mirror.

    Linux, macOS, and Termux users: run install.sh instead of this script.
#>

# ---------------------------------------------------------------------------
# 0. Make sure we're actually on Windows
# ---------------------------------------------------------------------------
if ($PSVersionTable.PSVersion.Major -ge 6 -and -not $IsWindows) {
    Write-Host "install.ps1 targets Windows." -ForegroundColor Red
    Write-Host "On Linux, macOS, or Termux, run ./install.sh instead." -ForegroundColor Red
    exit 1
}

function Write-Info($Message) { Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Ok($Message)   { Write-Host "[ok] $Message" -ForegroundColor Green }
function Write-Warn($Message) { Write-Host "[warn] $Message" -ForegroundColor Yellow }
function Write-Err($Message)  { Write-Host "Error: $Message" -ForegroundColor Red }

function Test-CommandExists($Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "          anif - ASCII Video Player Installer      " -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

$RepoDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
Set-Location $RepoDir

# ---------------------------------------------------------------------------
# 1. Detect CPU architecture
# ---------------------------------------------------------------------------
switch ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { $ArchName = "x86_64" }
    "ARM64" { $ArchName = "aarch64" }
    "x86"   { $ArchName = "x86" }
    default { $ArchName = $env:PROCESSOR_ARCHITECTURE }
}
Write-Info "Detected platform: windows-${ArchName}"

# ---------------------------------------------------------------------------
# 2. Install ffmpeg, ffprobe, and ffplay via winget (falls back to choco)
# ---------------------------------------------------------------------------
function Update-SessionPath {
    # winget/choco persist to the Machine/User PATH, but this already-running
    # process won't see that change until we re-read and merge it in.
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath    = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$env:Path;$machinePath;$userPath"
}

function Test-FfmpegSuite {
    return (Test-CommandExists ffmpeg) -and (Test-CommandExists ffprobe) -and (Test-CommandExists ffplay)
}

function Show-FfmpegSuiteReport {
    $allPresent = $true
    if (Test-CommandExists ffmpeg) {
        Write-Ok "ffmpeg  -> $((Get-Command ffmpeg).Source)"
    } else {
        Write-Err "ffmpeg  -> NOT FOUND (required)"
        $allPresent = $false
    }
    if (Test-CommandExists ffprobe) {
        Write-Ok "ffprobe -> $((Get-Command ffprobe).Source)"
    } else {
        Write-Warn "ffprobe -> not found (optional, used for faster/more accurate metadata probing)"
        $allPresent = $false
    }
    if (Test-CommandExists ffplay) {
        Write-Ok "ffplay  -> $((Get-Command ffplay).Source)"
    } else {
        Write-Warn "ffplay  -> not found (optional, used for audio playback)"
        $allPresent = $false
    }
    return $allPresent
}

function Install-FfmpegSuite {
    if (Test-CommandExists winget) {
        Write-Info "Installing ffmpeg via winget..."
        try {
            winget install --id Gyan.FFmpeg -e --source winget `
                --accept-package-agreements --accept-source-agreements --disable-interactivity
            if ($LASTEXITCODE -eq 0) {
                Write-Ok "ffmpeg, ffprobe, and ffplay installed."
            } else {
                Write-Warn "winget exited with code ${LASTEXITCODE} (it may already be installed)."
            }
        } catch {
            Write-Warn "winget install failed: $($_.Exception.Message)"
        }
        Update-SessionPath
    } else {
        Write-Warn "winget was not found. It ships as 'App Installer' on Windows 10/11;"
        Write-Warn "see https://aka.ms/getwinget to install it."
    }

    if (Test-FfmpegSuite) { return }

    if (Test-CommandExists choco) {
        Write-Info "Installing ffmpeg via Chocolatey..."
        try {
            choco install ffmpeg -y
            if ($LASTEXITCODE -eq 0) {
                Write-Ok "ffmpeg, ffprobe, and ffplay installed."
            } else {
                Write-Warn "choco exited with code ${LASTEXITCODE}. Chocolatey installs often need an elevated (Administrator) PowerShell."
            }
        } catch {
            Write-Warn "choco install failed: $($_.Exception.Message)"
        }
        Update-SessionPath
    }

    if (-not (Test-FfmpegSuite)) {
        Write-Warn "If winget/choco just installed it, try closing and reopening this terminal (PATH may need a refresh)."
        Write-Warn "Manual install: https://ffmpeg.org/download.html or 'winget install ffmpeg'"
    }
}

if (Test-FfmpegSuite) {
    Write-Ok "ffmpeg, ffprobe, and ffplay are already installed."
} else {
    Install-FfmpegSuite
    Write-Host ""
    Write-Info "ffmpeg suite status after install:"
    if (-not (Show-FfmpegSuiteReport)) {
        Write-Warn "ffprobe/ffplay are optional -- anif still works with just ffmpeg, but with reduced probing/audio support."
    }
}

# ---------------------------------------------------------------------------
# 3. Acquire or build anif.exe
# ---------------------------------------------------------------------------
$PrebuiltBin = Join-Path "bin" "anif-windows-${ArchName}.exe"
$AnifReady = $false

if (Test-Path $PrebuiltBin) {
    Write-Info "Found prebuilt binary: ${PrebuiltBin}"
    Copy-Item $PrebuiltBin ".\anif.exe" -Force
    $AnifReady = $true
} elseif ((Test-CommandExists gcc) -or (Test-CommandExists clang)) {
    $CC = if (Test-CommandExists gcc) { "gcc" } else { "clang" }
    Write-Info "Compiling anif natively with ${CC}..."
    $srcFiles = Get-ChildItem -Path "src" -Filter "*.c" | ForEach-Object { $_.FullName }
    try {
        & $CC -Iinclude -O3 -Wall -Wextra -std=c99 @srcFiles -o anif.exe -lm
        if ($LASTEXITCODE -eq 0 -and (Test-Path ".\anif.exe")) {
            $AnifReady = $true
            New-Item -ItemType Directory -Force -Path "bin" | Out-Null
            Copy-Item ".\anif.exe" $PrebuiltBin -Force -ErrorAction SilentlyContinue
        }
    } catch {
        Write-Warn "Compilation failed: $($_.Exception.Message)"
    }
}

if (-not $AnifReady) {
    Write-Err "Could not obtain anif.exe for windows-${ArchName}."
    Write-Host ""
    Write-Host "anif's source uses POSIX-only APIs (termios, fork/exec, select) that" -ForegroundColor Yellow
    Write-Host "plain Windows/MinGW does not provide, so a native compile from" -ForegroundColor Yellow
    Write-Host "PowerShell usually will not succeed. ffmpeg itself was still installed" -ForegroundColor Yellow
    Write-Host "above -- for anif, use one of:" -ForegroundColor Yellow
    Write-Host "  - WSL: wsl --install, then run ./install.sh inside your Linux distro"
    Write-Host "  - MSYS2 (its MSYS environment, not MINGW64/UCRT64) or Cygwin, then"
    Write-Host "    run ./install.sh from that shell"
    Write-Host ""
    exit 1
}

# ---------------------------------------------------------------------------
# 4. Install anif.exe onto PATH
# ---------------------------------------------------------------------------
$InstallDir = Join-Path $env:LOCALAPPDATA "Programs\anif"
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item ".\anif.exe" (Join-Path $InstallDir "anif.exe") -Force

Write-Info "Installing anif binary to ${InstallDir}\anif.exe..."

$UserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
if (-not $UserPath) { $UserPath = "" }
if ($UserPath -notlike "*${InstallDir}*") {
    [System.Environment]::SetEnvironmentVariable("Path", "${UserPath};${InstallDir}", "User")
    Write-Warn "Added ${InstallDir} to your User PATH. Restart your terminal for it to take effect there."
}
if ($env:Path -notlike "*${InstallDir}*") { $env:Path = "$env:Path;$InstallDir" }

# ---------------------------------------------------------------------------
# 5. Verify installation
# ---------------------------------------------------------------------------
Write-Host ""
& (Join-Path $InstallDir "anif.exe") --check

Write-Host ""
Write-Host "===================================================" -ForegroundColor Green
Write-Host "anif has been successfully installed!" -ForegroundColor Green
Write-Host "===================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Usage:"
Write-Host "  anif <video_file>           # Default TrueColor ASCII playback"
Write-Host "  anif -b <video_file>        # High-definition Half-Block mode (2x resolution)"
Write-Host "  anif -L <video_file>        # Luminance ramp mode"
Write-Host "  anif --once <video_file>    # Play once without looping"
Write-Host ""
Write-Host "Example:"
Write-Host "  anif video.mp4"
Write-Host ""
