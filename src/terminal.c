#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>

static struct termios orig_termios;
static bool term_raw_active = false;
static bool cursor_hidden = false;
static bool alt_screen_active = false;

extern volatile sig_atomic_t g_signal_received;
extern volatile sig_atomic_t g_terminal_resized;

static void signal_handler(int sig) {
    if (sig == SIGWINCH) {
        g_terminal_resized = 1;
    } else {
        g_signal_received = sig;
    }
}

void term_setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);
}

void term_enter_alt_screen(void) {
    if (!alt_screen_active) {
        /* Enter alternate screen buffer, disable auto-wrap (\x1b[?7l), and cursor home */
        const char *enter_alt = "\x1b[?1049h\x1b[?7l\x1b[H";
        (void)write(STDOUT_FILENO, enter_alt, strlen(enter_alt));
        alt_screen_active = true;
    }
}

void term_exit_alt_screen(void) {
    if (alt_screen_active) {
        /* Re-enable auto-wrap (\x1b[?7h) and exit alternate screen buffer */
        const char *exit_alt = "\x1b[?7h\x1b[?1049l";
        (void)write(STDOUT_FILENO, exit_alt, strlen(exit_alt));
        alt_screen_active = false;
    }
}

void term_restore(void) {
    if (cursor_hidden) {
        const char *restore_str = "\x1b[?25h\x1b[?7h\x1b[0m";
        (void)write(STDOUT_FILENO, restore_str, strlen(restore_str));
        cursor_hidden = false;
    }
    if (alt_screen_active) {
        term_exit_alt_screen();
    }
    if (term_raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        term_raw_active = false;
    }
}

void term_init(void) {
    if (!isatty(STDIN_FILENO)) {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return;
    }

    atexit(term_restore);

    struct termios raw = orig_termios;
    /* Disable canonical mode, echo, extended input processing, signal interrupts for raw capture */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1) {
        term_raw_active = true;
    }
}

void term_get_size(int *cols, int *rows) {
    struct winsize ws;
    int c = 100;
    int r = 40;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0 && ws.ws_row > 0) {
        c = ws.ws_col;
        r = ws.ws_row;
    } else {
        const char *col_env = getenv("COLUMNS");
        const char *row_env = getenv("LINES");
        if (col_env) c = atoi(col_env);
        if (row_env) r = atoi(row_env);
        if (c <= 0) c = 100;
        if (r <= 0) r = 40;
    }

    if (cols) *cols = c;
    if (rows) *rows = r;
}

void term_hide_cursor(void) {
    const char *hide_str = "\x1b[?25l";
    (void)write(STDOUT_FILENO, hide_str, strlen(hide_str));
    cursor_hidden = true;
}

void term_show_cursor(void) {
    const char *show_str = "\x1b[?25h";
    (void)write(STDOUT_FILENO, show_str, strlen(show_str));
    cursor_hidden = false;
}

void term_clear_screen(void) {
    const char *clear_str = "\x1b[2J\x1b[H";
    (void)write(STDOUT_FILENO, clear_str, strlen(clear_str));
}

int term_check_key(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return (int)c;
        }
    }
    return -1;
}
