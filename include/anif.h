#ifndef ANIF_H
#define ANIF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ANIF_VERSION "2.1.0"
#define DEFAULT_CHAR "#"

typedef struct {
    const char *video_path;
    const char *custom_char;
    int width_override;
    int height_override;
    double fps_override;
    int loop;               /* -1 = infinite (default), 0 = once, >0 = count */
    bool audio_enabled;
    bool use_luminance;
    bool use_half_block;    /* 2x vertical resolution using upper half block '▀' */
    bool auto_download;
    char ffmpeg_path[1024]; /* Required single binary */
    char ffplay_path[1024]; /* Optional audio player */
    char ffprobe_path[1024];/* Optional metadata probe */
} anif_options_t;

typedef struct {
    int src_width;
    int src_height;
    double fps;
    uint64_t frame_duration_us;
    int term_cols;
    int term_rows;
    int render_w;
    int render_h;
    size_t frame_bytes;
} anif_meta_t;

/* Binary discovery & static ffmpeg management */
int find_binaries(anif_options_t *opts);
int download_static_ffmpeg(const char *target_dir);
const char *get_default_static_dir(void);

/* Terminal management */
void term_init(void);
void term_restore(void);
void term_get_size(int *cols, int *rows);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_clear_screen(void);
void term_enter_alt_screen(void);
void term_exit_alt_screen(void);
int term_check_key(void);
void term_setup_signals(void);

/* Video probing and playback */
int video_probe(const anif_options_t *opts, anif_meta_t *meta);
int video_calculate_dimensions(const anif_options_t *opts, anif_meta_t *meta);
int video_play_stream(const anif_options_t *opts, const anif_meta_t *meta, volatile bool *should_quit);

/* Frame rendering */
int render_init(int render_w, int render_h, const char *ascii_char, bool use_luminance, bool use_half_block);
void render_free(void);
void render_frame(const uint8_t *rgb_data, int render_w, int render_h);

#endif /* ANIF_H */
