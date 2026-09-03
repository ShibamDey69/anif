#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static char g_static_dir[1024] = {0};

/* Where a user (or another tool) may drop a portable ffmpeg/ffprobe/ffplay
 * set for anif to pick up, in addition to whatever the system package
 * manager installed onto PATH. See find_binaries() below. */
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

static bool is_executable_file(const char *path) {
    return (access(path, X_OK) == 0);
}

/* Fill in whichever of ffmpeg_path/ffplay_path/ffprobe_path are still empty,
 * by checking if they exist as executables in `dir`. Fields already filled
 * in from a higher-priority location are left untouched, and this never
 * stops the overall search early just because one of the three was found
 * here -- the caller keeps going so the other two can still be found in a
 * later tier (e.g. system PATH) if this directory doesn't have them. */
static void check_dir_for_binaries(const char *dir, anif_options_t *opts) {
    if (opts->ffmpeg_path[0] == '\0') {
        char f[1024];
        snprintf(f, sizeof(f), "%s/ffmpeg", dir);
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
        if (!is_executable_file(f)) snprintf(f, sizeof(f), "%s/ffmpeg.exe", dir);
#endif
        if (is_executable_file(f)) {
            snprintf(opts->ffmpeg_path, sizeof(opts->ffmpeg_path), "%s", f);
        }
    }

    if (opts->ffplay_path[0] == '\0') {
        char f[1024];
        snprintf(f, sizeof(f), "%s/ffplay", dir);
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
        if (!is_executable_file(f)) snprintf(f, sizeof(f), "%s/ffplay.exe", dir);
#endif
        if (is_executable_file(f)) {
            snprintf(opts->ffplay_path, sizeof(opts->ffplay_path), "%s", f);
        }
    }

    if (opts->ffprobe_path[0] == '\0') {
        char f[1024];
        snprintf(f, sizeof(f), "%s/ffprobe", dir);
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
        if (!is_executable_file(f)) snprintf(f, sizeof(f), "%s/ffprobe.exe", dir);
#endif
        if (is_executable_file(f)) {
            snprintf(opts->ffprobe_path, sizeof(opts->ffprobe_path), "%s", f);
        }
    }
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
    /* Every tier below only fills in fields that are still empty, and the
     * search always continues to the next tier afterward -- finding one of
     * the three binaries in an earlier/higher-priority location must never
     * stop the search for the other two. */

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

    /* 2. Check ANIF_BIN_DIR or default static directory (~/.local/share/anif/bin) */
    const char *static_dir = get_default_static_dir();
    check_dir_for_binaries(static_dir, opts);

    /* 3. Check directory of current executable */
    char exe_dir[1024] = {0};
#if defined(__linux__) || defined(__ANDROID__)
    ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (len > 0) {
        exe_dir[len] = '\0';
        char *slash = strrchr(exe_dir, '/');
        if (slash) *slash = '\0';
        check_dir_for_binaries(exe_dir, opts);
        char sub_bin[1024];
        snprintf(sub_bin, sizeof(sub_bin), "%s/bin", exe_dir);
        check_dir_for_binaries(sub_bin, opts);
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(exe_dir);
    if (_NSGetExecutablePath(exe_dir, &size) == 0) {
        char *slash = strrchr(exe_dir, '/');
        if (slash) *slash = '\0';
        check_dir_for_binaries(exe_dir, opts);
        char sub_bin[1024];
        snprintf(sub_bin, sizeof(sub_bin), "%s/bin", exe_dir);
        check_dir_for_binaries(sub_bin, opts);
    }
#endif

    /* 4. Check current working directory ./bin and ./ */
    check_dir_for_binaries("./bin", opts);
    check_dir_for_binaries(".", opts);

    /* 5. Check system PATH -- this is where a package-manager-installed
     * ffmpeg/ffprobe/ffplay (apt, dnf, pacman, zypper, apk, brew, pkg,
     * winget, choco, ...) will normally be found. */
    if (!opts->ffmpeg_path[0]) check_path_binary("ffmpeg", opts->ffmpeg_path, sizeof(opts->ffmpeg_path));
    if (!opts->ffplay_path[0]) check_path_binary("ffplay", opts->ffplay_path, sizeof(opts->ffplay_path));
    if (!opts->ffprobe_path[0]) check_path_binary("ffprobe", opts->ffprobe_path, sizeof(opts->ffprobe_path));

    /* Only ffmpeg is required for anif to operate */
    if (opts->ffmpeg_path[0] != '\0') {
        return 0;
    }

    return -1;
}
