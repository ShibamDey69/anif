#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>

volatile sig_atomic_t g_signal_received = 0;
volatile sig_atomic_t g_terminal_resized = 0;

static void print_usage(const char *prog) {
    printf("anif v%s — Play video as live color ASCII / ANSI art in your terminal\n\n", ANIF_VERSION);
    printf("Usage: %s [options] <video_file>\n\n", prog);
    printf("Options:\n");
    printf("  -b, --block             Use half-block TrueColor mode (2x vertical resolution)\n");
    printf("  -c, --char <str>        Character(s) to use for ASCII art (default: \"#\")\n");
    printf("  -L, --luminance         Use ASCII luminance ramp instead of fixed character\n");
    printf("  -W, --width <cols>      Override render width in terminal columns\n");
    printf("  -H, --height <rows>     Override render height in terminal rows\n");
    printf("  -f, --fps <rate>        Override playback frame rate\n");
    printf("  -1, --once              Play video once without looping\n");
    printf("  -l, --loop [N]          Loop playback (default: infinite, or N times)\n");
    printf("      --no-audio          Disable audio playback\n");
    printf("      --check             Check and display detected FFmpeg binary paths\n");
    printf("      --download-ffmpeg   Download standalone static FFmpeg binary\n");
    printf("      --ffmpeg <path>     Explicit path to ffmpeg binary\n");
    printf("      --ffplay <path>     Explicit path to ffplay binary (optional)\n");
    printf("      --ffprobe <path>    Explicit path to ffprobe binary (optional)\n");
    printf("  -v, --version           Show version information\n");
    printf("  -h, --help              Show this help message\n\n");
    printf("Controls during playback:\n");
    printf("  q, Q, ESC, Ctrl+C       Quit immediately\n");
}

int main(int argc, char **argv) {
    anif_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.loop = -1; /* default: loop infinitely */
    opts.audio_enabled = true;
    opts.custom_char = DEFAULT_CHAR;
    opts.use_half_block = false;
    bool check_only = false;

    static struct option long_options[] = {
        {"block",           no_argument,       0, 'b'},
        {"char",            required_argument, 0, 'c'},
        {"luminance",       no_argument,       0, 'L'},
        {"width",           required_argument, 0, 'W'},
        {"height",          required_argument, 0, 'H'},
        {"fps",             required_argument, 0, 'f'},
        {"once",            no_argument,       0, '1'},
        {"loop",            optional_argument, 0, 'l'},
        {"no-audio",        no_argument,       0, 1001},
        {"download-ffmpeg", no_argument,       0, 1002},
        {"ffmpeg",          required_argument, 0, 1003},
        {"ffplay",          required_argument, 0, 1004},
        {"ffprobe",         required_argument, 0, 1005},
        {"check",           no_argument,       0, 1006},
        {"version",         no_argument,       0, 'v'},
        {"help",            no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int opt_index = 0;
    while ((opt = getopt_long(argc, argv, "bc:LW:H:f:1l::vh", long_options, &opt_index)) != -1) {
        switch (opt) {
            case 'b':
                opts.use_half_block = true;
                break;
            case 'c':
                opts.custom_char = optarg;
                opts.use_half_block = false;
                break;
            case 'L':
                opts.use_luminance = true;
                opts.use_half_block = false;
                break;
            case 'W':
                opts.width_override = atoi(optarg);
                break;
            case 'H':
                opts.height_override = atoi(optarg);
                break;
            case 'f':
                opts.fps_override = atof(optarg);
                break;
            case '1':
                opts.loop = 0;
                break;
            case 'l':
                if (optarg) {
                    opts.loop = atoi(optarg);
                } else if (optind < argc && argv[optind][0] != '-') {
                    opts.loop = atoi(argv[optind++]);
                } else {
                    opts.loop = -1;
                }
                break;
            case 1001: /* --no-audio */
                opts.audio_enabled = false;
                break;
            case 1002: /* --download-ffmpeg */
                opts.auto_download = true;
                break;
            case 1003: /* --ffmpeg */
                snprintf(opts.ffmpeg_path, sizeof(opts.ffmpeg_path), "%s", optarg);
                break;
            case 1004: /* --ffplay */
                snprintf(opts.ffplay_path, sizeof(opts.ffplay_path), "%s", optarg);
                break;
            case 1005: /* --ffprobe */
                snprintf(opts.ffprobe_path, sizeof(opts.ffprobe_path), "%s", optarg);
                break;
            case 1006: /* --check */
                check_only = true;
                break;
            case 'v':
                printf("anif version %s (C native, single static ffmpeg binary)\n", ANIF_VERSION);
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (check_only) {
        int ret = find_binaries(&opts);
        printf("FFmpeg Binary Status:\n");
        printf("  ffmpeg:  %s (%s - REQUIRED)\n", opts.ffmpeg_path[0] ? opts.ffmpeg_path : "NOT FOUND", opts.ffmpeg_path[0] ? "OK" : "MISSING");
        printf("  ffplay:  %s (%s - OPTIONAL for audio)\n", opts.ffplay_path[0] ? opts.ffplay_path : "NOT FOUND", opts.ffplay_path[0] ? "OK" : "MISSING");
        printf("  ffprobe: %s (%s - OPTIONAL for metadata)\n", opts.ffprobe_path[0] ? opts.ffprobe_path : "NOT FOUND", opts.ffprobe_path[0] ? "OK" : "MISSING");
        return (ret == 0 && opts.ffmpeg_path[0] != '\0') ? 0 : 1;
    }

    if (opts.auto_download) {
        int ret = download_static_ffmpeg(NULL);
        if (ret != 0) {
            fprintf(stderr, "Failed to download static ffmpeg binary.\n");
            return 1;
        }
        if (optind >= argc) {
            printf("\nSetup complete. You can now run: anif <video_file>\n");
            return 0;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "\033[1;31mError:\033[0m No video file specified.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    opts.video_path = argv[optind];

    /* Check if video file exists */
    if (access(opts.video_path, R_OK) != 0) {
        fprintf(stderr, "\033[1;31mError:\033[0m Cannot read video file \"%s\": %s\n",
                opts.video_path, strerror(errno));
        return 1;
    }

    /* Locate ffmpeg (single static binary) */
    if (find_binaries(&opts) != 0 || opts.ffmpeg_path[0] == '\0') {
        if (isatty(STDIN_FILENO)) {
            printf("\033[1;33m[anif]\033[0m FFmpeg binary was not found.\n");
            printf("Would you like to automatically download a standalone static FFmpeg binary to %s? [Y/n]: ",
                   get_default_static_dir());
            fflush(stdout);

            char answer[32] = {0};
            if (fgets(answer, sizeof(answer), stdin) && (answer[0] == 'y' || answer[0] == 'Y' || answer[0] == '\n')) {
                if (download_static_ffmpeg(NULL) == 0) {
                    find_binaries(&opts);
                }
            }
        }

        if (opts.ffmpeg_path[0] == '\0') {
            fprintf(stderr, "\n\033[1;31manif needs a static ffmpeg binary to run.\033[0m\n\n");
            fprintf(stderr, "Run: anif --download-ffmpeg\n");
            fprintf(stderr, "Or install ffmpeg using your package manager.\n");
            return 1;
        }
    }

    /* Setup signal handling and terminal raw mode */
    term_setup_signals();
    term_init();
    term_enter_alt_screen();
    term_hide_cursor();

    /* Probe video using single ffmpeg binary */
    anif_meta_t meta;
    memset(&meta, 0, sizeof(meta));

    if (video_probe(&opts, &meta) != 0) {
        term_restore();
        fprintf(stderr, "\033[1;31mError:\033[0m Failed to probe video stream from \"%s\".\n", opts.video_path);
        return 1;
    }

    volatile bool should_quit = false;
    int loops_remaining = opts.loop;

    while (!should_quit && g_signal_received == 0) {
        if (video_calculate_dimensions(&opts, &meta) != 0) break;

        if (render_init(meta.render_w, meta.render_h, opts.custom_char, opts.use_luminance, opts.use_half_block) != 0) {
            term_restore();
            fprintf(stderr, "Memory allocation error for renderer.\n");
            return 1;
        }

        term_clear_screen();

        if (video_play_stream(&opts, &meta, &should_quit) != 0) {
            break;
        }

        if (loops_remaining > 0) {
            loops_remaining--;
            if (loops_remaining == 0) break;
        } else if (loops_remaining == 0) {
            break;
        }
    }

    render_free();
    term_restore();
    return 0;
}
