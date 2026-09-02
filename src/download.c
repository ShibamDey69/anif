#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static char g_static_dir[1024] = {0};

const char *get_default_static_dir(void) {
    if (g_static_dir[0] != '\0') {
        return g_static_dir;
    }

    const char *custom = getenv("ANIF_BIN_DIR");
    if (custom && custom[0] != '\0') {
        snprintf(g_static_dir, sizeof(g_static_dir), "%s", custom);
        return g_static_dir;
    }

    const char *xdg_data = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");

    if (xdg_data && xdg_data[0] != '\0') {
        snprintf(g_static_dir, sizeof(g_static_dir), "%s/anif/bin", xdg_data);
    } else if (home && home[0] != '\0') {
        snprintf(g_static_dir, sizeof(g_static_dir), "%s/.local/share/anif/bin", home);
    } else {
        snprintf(g_static_dir, sizeof(g_static_dir), "./bin");
    }

    return g_static_dir;
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static bool is_executable_file(const char *path) {
    return (access(path, X_OK) == 0);
}

/* Check if directory contains ffmpeg (and optionally ffplay, ffprobe) */
static bool check_dir_for_binaries(const char *dir, anif_options_t *opts) {
    char f_ffmpeg[1024];
    char f_ffplay[1024];
    char f_ffprobe[1024];

    snprintf(f_ffmpeg, sizeof(f_ffmpeg), "%s/ffmpeg", dir);
    snprintf(f_ffplay, sizeof(f_ffplay), "%s/ffplay", dir);
    snprintf(f_ffprobe, sizeof(f_ffprobe), "%s/ffprobe", dir);

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
    if (!is_executable_file(f_ffmpeg)) snprintf(f_ffmpeg, sizeof(f_ffmpeg), "%s/ffmpeg.exe", dir);
    if (!is_executable_file(f_ffplay)) snprintf(f_ffplay, sizeof(f_ffplay), "%s/ffplay.exe", dir);
    if (!is_executable_file(f_ffprobe)) snprintf(f_ffprobe, sizeof(f_ffprobe), "%s/ffprobe.exe", dir);
#endif

    bool has_ffmpeg = is_executable_file(f_ffmpeg);
    bool has_ffplay = is_executable_file(f_ffplay);
    bool has_ffprobe = is_executable_file(f_ffprobe);

    if (has_ffmpeg) {
        snprintf(opts->ffmpeg_path, sizeof(opts->ffmpeg_path), "%s", f_ffmpeg);
        if (has_ffplay) snprintf(opts->ffplay_path, sizeof(opts->ffplay_path), "%s", f_ffplay);
        if (has_ffprobe) snprintf(opts->ffprobe_path, sizeof(opts->ffprobe_path), "%s", f_ffprobe);
        return true;
    }
    return false;
}

static bool check_path_binary(const char *binary, char *out_path, size_t out_size) {
    const char *path_env = getenv("PATH");
    if (!path_env) return false;

    char path_copy[4096];
    snprintf(path_copy, sizeof(path_copy), "%s", path_env);

    char *token = strtok(path_copy, ":");
    while (token) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", token, binary);
        if (is_executable_file(full_path)) {
            snprintf(out_path, out_size, "%s", full_path);
            return true;
        }
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
        snprintf(full_path, sizeof(full_path), "%s/%s.exe", token, binary);
        if (is_executable_file(full_path)) {
            snprintf(out_path, out_size, "%s", full_path);
            return true;
        }
#endif
        token = strtok(NULL, ":");
    }
    return false;
}

int find_binaries(anif_options_t *opts) {
    /* 1. Check explicit environment variables */
    const char *env_ffmpeg = getenv("ANIF_FFMPEG_PATH");
    const char *env_ffplay = getenv("ANIF_FFPLAY_PATH");
    const char *env_ffprobe = getenv("ANIF_FFPROBE_PATH");

    if (env_ffmpeg && is_executable_file(env_ffmpeg)) {
        snprintf(opts->ffmpeg_path, sizeof(opts->ffmpeg_path), "%s", env_ffmpeg);
    }
    if (env_ffplay && is_executable_file(env_ffplay)) {
        snprintf(opts->ffplay_path, sizeof(opts->ffplay_path), "%s", env_ffplay);
    }
    if (env_ffprobe && is_executable_file(env_ffprobe)) {
        snprintf(opts->ffprobe_path, sizeof(opts->ffprobe_path), "%s", env_ffprobe);
    }

    if (opts->ffmpeg_path[0] != '\0') {
        return 0;
    }

    /* 2. Check ANIF_BIN_DIR or default static directory (~/.local/share/anif/bin) */
    const char *static_dir = get_default_static_dir();
    if (check_dir_for_binaries(static_dir, opts)) {
        return 0;
    }

    /* 3. Check directory of current executable */
    char exe_dir[1024] = {0};
#if defined(__linux__) || defined(__ANDROID__)
    ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (len > 0) {
        exe_dir[len] = '\0';
        char *slash = strrchr(exe_dir, '/');
        if (slash) *slash = '\0';
        if (check_dir_for_binaries(exe_dir, opts)) {
            return 0;
        }
        char sub_bin[1024];
        snprintf(sub_bin, sizeof(sub_bin), "%s/bin", exe_dir);
        if (check_dir_for_binaries(sub_bin, opts)) {
            return 0;
        }
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(exe_dir);
    if (_NSGetExecutablePath(exe_dir, &size) == 0) {
        char *slash = strrchr(exe_dir, '/');
        if (slash) *slash = '\0';
        if (check_dir_for_binaries(exe_dir, opts)) {
            return 0;
        }
        char sub_bin[1024];
        snprintf(sub_bin, sizeof(sub_bin), "%s/bin", exe_dir);
        if (check_dir_for_binaries(sub_bin, opts)) {
            return 0;
        }
    }
#endif

    /* 4. Check current working directory ./bin and ./ */
    if (check_dir_for_binaries("./bin", opts) || check_dir_for_binaries(".", opts)) {
        return 0;
    }

    /* 5. Check system PATH */
    if (!opts->ffmpeg_path[0]) check_path_binary("ffmpeg", opts->ffmpeg_path, sizeof(opts->ffmpeg_path));
    if (!opts->ffplay_path[0]) check_path_binary("ffplay", opts->ffplay_path, sizeof(opts->ffplay_path));
    if (!opts->ffprobe_path[0]) check_path_binary("ffprobe", opts->ffprobe_path, sizeof(opts->ffprobe_path));

    /* Only ffmpeg is required for anif to operate */
    if (opts->ffmpeg_path[0] != '\0') {
        return 0;
    }

    return -1;
}

int download_static_ffmpeg(const char *target_dir) {
    if (!target_dir || target_dir[0] == '\0') {
        target_dir = get_default_static_dir();
    }

    printf("\033[1;36m[anif]\033[0m Setting up static FFmpeg single binary in: %s\n", target_dir);

    if (mkdir_p(target_dir) != 0) {
        fprintf(stderr, "\033[1;31m[anif Error]\033[0m Failed to create directory: %s\n", target_dir);
        return -1;
    }

    /* Determine platform URL for standalone static single binary */
    const char *url = NULL;

#if defined(__ANDROID__)
    /* On Android/Termux, prefer standalone arm64 / arm static build or pkg */
    #if defined(__aarch64__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-arm-64.zip";
    #elif defined(__arm__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-armhf-32.zip";
    #elif defined(__x86_64__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-64.zip";
    #endif
#elif defined(__linux__)
    #if defined(__x86_64__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-64.zip";
    #elif defined(__aarch64__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-arm-64.zip";
    #elif defined(__arm__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-armhf-32.zip";
    #else
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-linux-32.zip";
    #endif
#elif defined(__APPLE__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-osx-64.zip";
#elif defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
    url = "https://github.com/ffbinaries/ffbinaries-prebuilt/releases/download/v6.1/ffmpeg-6.1-win-64.zip";
#endif

    if (!url) {
        fprintf(stderr, "\033[1;31m[anif Error]\033[0m Automated static download is not available for this architecture.\n");
        return -1;
    }

    char cmd[2048];
    printf("\033[1;32m[anif]\033[0m Downloading static single FFmpeg binary from:\n  %s\n", url);

    char tmp_archive[1024];
    snprintf(tmp_archive, sizeof(tmp_archive), "%s/ffmpeg_static.zip", target_dir);

    /* Download using curl, wget, or system downloader */
    if (system("command -v curl >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -L -f --progress-bar -o \"%s\" \"%s\"", tmp_archive, url);
    } else if (system("command -v wget >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "wget -q --show-progress -O \"%s\" \"%s\"", tmp_archive, url);
    } else {
        fprintf(stderr, "\033[1;31m[anif Error]\033[0m curl or wget is required for downloading.\n");
        return -1;
    }

    int dl_status = system(cmd);
    if (dl_status != 0) {
        fprintf(stderr, "\033[1;31m[anif Error]\033[0m Download failed.\n");
        unlink(tmp_archive);
        return -1;
    }

    printf("\033[1;32m[anif]\033[0m Extracting static ffmpeg binary to %s...\n", target_dir);

    /* Unzip ffmpeg binary only */
    if (system("command -v unzip >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "unzip -o -q \"%s\" -d \"%s\"", tmp_archive, target_dir);
    } else if (system("command -v 7z >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "7z x -y \"%s\" -o\"%s\" >/dev/null", tmp_archive, target_dir);
    } else if (system("command -v busybox >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "busybox unzip -o \"%s\" -d \"%s\"", tmp_archive, target_dir);
    } else {
        /* Fallback python/perl unzip if unzip CLI not installed */
        snprintf(cmd, sizeof(cmd), "python3 -c \"import zipfile; zipfile.ZipFile('%s').extractall('%s')\" 2>/dev/null",
                 tmp_archive, target_dir);
    }

    system(cmd);
    unlink(tmp_archive);

    /* Set executable permissions on ffmpeg */
    char chmod_cmd[1024];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod 755 \"%s\"/ffmpeg* 2>/dev/null", target_dir);
    system(chmod_cmd);

    char test_bin[1024];
    snprintf(test_bin, sizeof(test_bin), "%s/ffmpeg", target_dir);
    if (is_executable_file(test_bin)) {
        printf("\033[1;32m[anif]\033[0m Static single FFmpeg binary installed successfully in %s!\n", target_dir);
        return 0;
    }

    /* Check for .exe on Windows */
    snprintf(test_bin, sizeof(test_bin), "%s/ffmpeg.exe", target_dir);
    if (is_executable_file(test_bin)) {
        printf("\033[1;32m[anif]\033[0m Static single FFmpeg binary installed successfully in %s!\n", target_dir);
        return 0;
    }

    fprintf(stderr, "\033[1;31m[anif Error]\033[0m Extraction did not produce executable ffmpeg.\n");
    return -1;
}
