#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

extern volatile sig_atomic_t g_signal_received;
extern volatile sig_atomic_t g_terminal_resized;

#include <ctype.h>

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000ULL);
}

static bool parse_ffmpeg_video_info(const char *line, int *out_w, int *out_h, double *out_fps) {
    const char *v = strstr(line, "Video:");
    if (!v) return false;

    /* 1. Extract resolution: look for [0-9]+x[0-9]+ */
    const char *p = v + 6;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            char *end1 = NULL;
            long w = strtol(p, &end1, 10);
            if (end1 && *end1 == 'x' && isdigit((unsigned char)*(end1 + 1))) {
                if (p > line && (p[-1] == 'x' || p[-1] == 'X' || isalnum((unsigned char)p[-1]))) {
                    p = end1 + 1;
                    continue;
                }
                char *end2 = NULL;
                long h = strtol(end1 + 1, &end2, 10);
                if (w >= 16 && h >= 16) {
                    *out_w = (int)w;
                    *out_h = (int)h;
                    break;
                }
                p = end1 + 1;
                continue;
            }
            p = end1;
        } else {
            p++;
        }
    }

    /* 2. Extract fps or tbr */
    p = v + 6;
    while (*p) {
        if (isdigit((unsigned char)*p) || *p == '.') {
            char *end = NULL;
            double val = strtod(p, &end);
            if (end && end != p) {
                while (*end == ' ') end++;
                if (strncmp(end, "fps", 3) == 0 && (end[3] == '\0' || isspace((unsigned char)end[3]) || end[3] == ',')) {
                    *out_fps = val;
                    break;
                } else if (*out_fps <= 0.0 && strncmp(end, "tbr", 3) == 0 && (end[3] == '\0' || isspace((unsigned char)end[3]) || end[3] == ',')) {
                    *out_fps = val;
                }
                p = end;
                continue;
            }
        }
        p++;
    }

    return (*out_w > 0 && *out_h > 0);
}

int video_probe(const anif_options_t *opts, anif_meta_t *meta) {
    int w = 0, h = 0;
    double fps = 0.0;

    /* 1. Try ffprobe if available */
    if (opts->ffprobe_path[0] != '\0') {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate -of csv=p=0 \"%s\"",
                 opts->ffprobe_path, opts->video_path);

        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[512] = {0};
            if (fgets(line, sizeof(line), fp)) {
                int fps_num = 0, fps_den = 1;
                double direct_fps = 0.0;
                if (sscanf(line, "%d,%d,%d/%d", &w, &h, &fps_num, &fps_den) == 4 && fps_den > 0) {
                    fps = (double)fps_num / (double)fps_den;
                } else if (sscanf(line, "%d,%d,%lf", &w, &h, &direct_fps) == 3 && direct_fps > 0.0) {
                    fps = direct_fps;
                } else if (sscanf(line, "%d,%d", &w, &h) == 2) {
                    fps = 30.0;
                }
            }
            pclose(fp);
        }
    }

    /* 2. Fallback to probing directly with single static ffmpeg binary */
    if (w <= 0 || h <= 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "\"%s\" -hide_banner -i \"%s\" 2>&1", opts->ffmpeg_path, opts->video_path);

        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                if (parse_ffmpeg_video_info(line, &w, &h, &fps)) {
                    break;
                }
            }
            pclose(fp);
        }
    }

    /* Set defaults if probing failed to extract dimensions */
    if (w <= 0) w = 1280;
    if (h <= 0) h = 720;
    if (fps <= 0.1 || fps > 240.0) fps = 30.0;

    meta->src_width = w;
    meta->src_height = h;
    meta->fps = (opts->fps_override > 0.0) ? opts->fps_override : fps;
    meta->frame_duration_us = (uint64_t)(1000000.0 / meta->fps);

    return 0;
}

int video_calculate_dimensions(const anif_options_t *opts, anif_meta_t *meta) {
    term_get_size(&meta->term_cols, &meta->term_rows);

    int max_cols = (meta->term_cols > 2) ? (meta->term_cols - 1) : 2;
    int max_rows = (meta->term_rows > 2) ? (meta->term_rows - 1) : 1;

    if (opts->width_override > 0 && opts->height_override > 0) {
        meta->render_w = opts->width_override;
        meta->render_h = opts->height_override;
    } else if (opts->use_half_block) {
        /* Half-block mode: 1 char cell = 2 vertical pixels (1:1 square pixel aspect ratio) */
        double ratio = (double)meta->src_width / (double)meta->src_height;
        int W = (opts->width_override > 0) ? opts->width_override : max_cols;
        int H = (int)(W / ratio);

        int max_pixel_h = max_rows * 2;
        if (opts->height_override > 0) {
            H = opts->height_override * 2;
            W = (int)(H * ratio);
        } else if (H > max_pixel_h) {
            H = max_pixel_h;
            W = (int)(H * ratio);
        }

        if (opts->width_override <= 0 && W > max_cols) {
            W = max_cols;
        }

        meta->render_w = W;
        meta->render_h = (H + 1) & ~1; /* Ensure even height for top+bottom half blocks */
    } else {
        /* Character / luminance mode: terminal char cell is ~1:2 aspect ratio */
        double ratio = (double)meta->src_width / (double)meta->src_height;
        if (opts->width_override > 0) {
            meta->render_w = opts->width_override;
            meta->render_h = (int)(meta->render_w / ratio / 2.0);
        } else if (opts->height_override > 0) {
            meta->render_h = opts->height_override;
            meta->render_w = (int)(meta->render_h * ratio * 2.0);
        } else {
            int W = max_cols;
            int H = (int)(W / ratio / 2.0);
            if (H > max_rows) {
                H = max_rows;
                W = (int)(H * ratio * 2.0);
            }
            if (W > max_cols) {
                W = max_cols;
            }
            meta->render_w = W;
            meta->render_h = H;
        }
    }

    if (meta->render_w < 2) meta->render_w = 2;
    if (meta->render_h < 1) meta->render_h = 1;

    meta->frame_bytes = (size_t)meta->render_w * (size_t)meta->render_h * 3;
    return 0;
}

/* Helper to read exact count of bytes from file descriptor */
static ssize_t read_exact(int fd, uint8_t *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, buf + total, count - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            /* EOF */
            return total;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

/* Helper to discard exact count of bytes from file descriptor */
static ssize_t discard_bytes(int fd, size_t count) {
    uint8_t dummy[8192];
    size_t remaining = count;
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(dummy)) ? sizeof(dummy) : remaining;
        ssize_t n = read(fd, dummy, chunk);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return 0;
        remaining -= (size_t)n;
    }
    return (ssize_t)count;
}

int video_play_stream(const anif_options_t *opts, const anif_meta_t *meta, volatile bool *should_quit) {
    pid_t audio_pid = -1;
    pid_t ffmpeg_pid = -1;
    int pipefd[2];

    if (pipe(pipefd) != 0) {
        perror("pipe failed");
        return -1;
    }

    /* 1. Spawn Audio Player (ffplay) if audio is enabled */
    if (opts->audio_enabled && opts->ffplay_path[0] != '\0') {
        audio_pid = fork();
        if (audio_pid == 0) {
            /* In child audio process */
            int devnull = open("/dev/null", O_RDWR);
            if (devnull != -1) {
                dup2(devnull, STDIN_FILENO);
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            close(pipefd[0]);
            close(pipefd[1]);

            execlp(opts->ffplay_path, opts->ffplay_path,
                   "-nodisp", "-autoexit", "-loglevel", "quiet",
                   opts->video_path, (char *)NULL);
            _exit(1);
        }
    }

    /* 2. Spawn Video Decoder (ffmpeg) */
    ffmpeg_pid = fork();
    if (ffmpeg_pid == 0) {
        /* In child video process */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        char scale_arg[64];
        snprintf(scale_arg, sizeof(scale_arg), "scale=%d:%d", meta->render_w, meta->render_h);

        execlp(opts->ffmpeg_path, opts->ffmpeg_path,
               "-loglevel", "error",
               "-i", opts->video_path,
               "-vf", scale_arg,
               "-pix_fmt", "rgb24",
               "-f", "rawvideo",
               "pipe:1", (char *)NULL);
        _exit(1);
    }

    /* In parent process */
    close(pipefd[1]);
    int video_fd = pipefd[0];

    uint8_t *frame_buffer = (uint8_t *)malloc(meta->frame_bytes);
    if (!frame_buffer) {
        close(video_fd);
        if (ffmpeg_pid > 0) { kill(ffmpeg_pid, SIGTERM); waitpid(ffmpeg_pid, NULL, 0); }
        if (audio_pid > 0) { kill(audio_pid, SIGTERM); waitpid(audio_pid, NULL, 0); }
        return -1;
    }

    uint64_t start_time_us = 0;
    int64_t current_frame = -1;
    bool finished = false;

    while (!finished && !*should_quit && g_signal_received == 0) {
        /* Check keyboard inputs */
        int key = term_check_key();
        if (key == 'q' || key == 'Q' || key == 3 /* Ctrl+C */ || key == 27 /* ESC */) {
            *should_quit = true;
            break;
        }

        if (g_terminal_resized) {
            g_terminal_resized = 0;
            /* Terminal resize detected; will recompute on next loop or continue */
        }

        if (start_time_us == 0) {
            start_time_us = get_time_us();
        }

        uint64_t now_us = get_time_us();
        uint64_t elapsed_us = now_us - start_time_us;
        int64_t due_frame = (int64_t)(elapsed_us / meta->frame_duration_us);

        /* Catch up if lagging: drop frames */
        if (due_frame > current_frame + 1) {
            int64_t skip_frames = due_frame - current_frame - 1;
            if (skip_frames > 0) {
                ssize_t dropped = discard_bytes(video_fd, (size_t)skip_frames * meta->frame_bytes);
                if (dropped <= 0) {
                    finished = true;
                    break;
                }
            }
        }

        /* Read one frame */
        ssize_t n = read_exact(video_fd, frame_buffer, meta->frame_bytes);
        if ((size_t)n < meta->frame_bytes) {
            finished = true;
            break;
        }

        /* Draw frame */
        render_frame(frame_buffer, meta->render_w, meta->render_h);
        current_frame = (due_frame > current_frame) ? due_frame : current_frame + 1;

        /* Sleep until next frame is due */
        uint64_t next_due_us = (uint64_t)(current_frame + 1) * meta->frame_duration_us;
        now_us = get_time_us();
        elapsed_us = now_us - start_time_us;

        if (elapsed_us < next_due_us) {
            uint64_t sleep_us = next_due_us - elapsed_us;
            /* Sleep in small 1-2ms increments for instant key responsive exit */
            while (sleep_us > 1000 && !*should_quit && g_signal_received == 0) {
                uint64_t step = (sleep_us > 2000) ? 2000 : sleep_us;
                usleep((useconds_t)step);
                int k = term_check_key();
                if (k == 'q' || k == 'Q' || k == 3 || k == 27) {
                    *should_quit = true;
                    break;
                }
                now_us = get_time_us();
                elapsed_us = now_us - start_time_us;
                if (elapsed_us >= next_due_us) break;
                sleep_us = next_due_us - elapsed_us;
            }
        }
    }

    free(frame_buffer);
    close(video_fd);

    /* Terminate subprocesses */
    if (ffmpeg_pid > 0) {
        kill(ffmpeg_pid, SIGTERM);
        waitpid(ffmpeg_pid, NULL, 0);
    }
    if (audio_pid > 0) {
        kill(audio_pid, SIGTERM);
        waitpid(audio_pid, NULL, 0);
    }

    return 0;
}
