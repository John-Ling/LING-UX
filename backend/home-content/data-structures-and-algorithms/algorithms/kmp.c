#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// -------------------------------------------------------
// ANSI escape helpers (no colour — cursor/erase only)
// -------------------------------------------------------
#define ANSI_CLEAR_SCREEN   "\033[2J"
#define ANSI_CURSOR_HOME    "\033[H"
#define ANSI_CURSOR_UP(n)   "\033[" #n "A"
#define ANSI_ERASE_LINE     "\033[2K"
#define ANSI_BOLD_ON        "\033[1m"
#define ANSI_BOLD_OFF       "\033[0m"
#define ANSI_UNDERLINE_ON   "\033[4m"
#define ANSI_UNDERLINE_OFF  "\033[0m"
#define ANSI_INVERT_ON      "\033[7m"
#define ANSI_INVERT_OFF     "\033[27m"
#define ANSI_HIDE_CURSOR    "\033[?25l"
#define ANSI_SHOW_CURSOR    "\033[?25h"

// Move cursor to row, col (1-indexed)
static void cursor_move(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

// -------------------------------------------------------
// Delay helpers
// -------------------------------------------------------
#define STEP_DELAY_US   350000   // 350 ms per step
#define FAST_DELAY_US    80000   //  80 ms for minor updates

static void wait(int us) { usleep(us); }

// -------------------------------------------------------
// LPS / failure function  (fixed: allocates +1, proper build)
// -------------------------------------------------------
static void build_lps(const char *pattern, int m, int lps[])
{
    lps[0] = 0;
    int len = 0;
    int i   = 1;
    while (i < m)
    {
        if (pattern[i] == pattern[len])
        {
            len++;
            lps[i] = len;
            i++;
        }
        else
        {
            if (len != 0)
                len = lps[len - 1];
            else
            {
                lps[i] = 0;
                i++;
            }
        }
    }
}

// -------------------------------------------------------
// UI layout constants  (all row numbers are 1-indexed)
// Label and data share the SAME row; label at col 1, data at col 12
// -------------------------------------------------------
#define ROW_TITLE       1
#define ROW_DIVIDER1    2
#define ROW_TEXT        3
#define ROW_PAT         4
#define ROW_DIVIDER2    5
#define ROW_LPS         6
#define ROW_DIVIDER3    7
#define ROW_STATUS      8
#define ROW_INFO        9
#define ROW_MATCHES    10
#define ROW_DIVIDER4   11
#define ROW_LEGEND     12

// -------------------------------------------------------
// Draw the static chrome
// -------------------------------------------------------
static void draw_chrome(const char *text, const char *pattern __attribute__((unused)), int n, int m, const int lps[])
{
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);

    // Dividers
    cursor_move(ROW_DIVIDER1, 1); printf("  %.*s", 60, "------------------------------------------------------------");
    cursor_move(ROW_DIVIDER2, 1); printf("  %.*s", 60, "------------------------------------------------------------");
    cursor_move(ROW_DIVIDER3, 1); printf("  %.*s", 60, "------------------------------------------------------------");
    cursor_move(ROW_DIVIDER4, 1); printf("  %.*s", 60, "------------------------------------------------------------");

    // Text label + data on the same row
    cursor_move(ROW_TEXT, 1);
    printf("  Text: ");
    cursor_move(ROW_TEXT, 12);
    for (int i = 0; i < n; i++) printf("%c ", text[i]);

    cursor_move(ROW_PAT, 1);

    // LPS label + values on the same row
    cursor_move(ROW_LPS, 1);
    printf("  LPS: ");
    cursor_move(ROW_LPS, 12);
    for (int i = 0; i < m; i++) printf("%d ", lps[i]);

    // Legend
    cursor_move(ROW_LEGEND, 1);
    printf("  " ANSI_INVERT_ON "X" ANSI_INVERT_OFF " active   "
           ANSI_UNDERLINE_ON "X" ANSI_UNDERLINE_OFF " match   "
           ANSI_BOLD_ON "X" ANSI_BOLD_OFF " mismatch   "
           "[ ] pattern window");

    fflush(stdout);
}

// -------------------------------------------------------
// Redraw the pattern row at a given text offset
// -------------------------------------------------------
static void draw_pattern(const char *pattern, int m, int text_offset, int highlight_j, int is_match, int is_mismatch)
{
    cursor_move(ROW_PAT, 11);
    printf(ANSI_ERASE_LINE);

    // Indent to align with the text character at text_offset
    for (int sp = 0; sp < text_offset * 2; sp++) putchar(' ');

    putchar('[');
    for (int j = 0; j < m; j++)
    {
        if (j == highlight_j)
        {
            if (is_mismatch)
                printf(ANSI_BOLD_ON "%c" ANSI_BOLD_OFF, pattern[j]);
            else if (is_match)
                printf(ANSI_UNDERLINE_ON "%c" ANSI_UNDERLINE_OFF, pattern[j]);
            else
                printf(ANSI_INVERT_ON "%c" ANSI_INVERT_OFF, pattern[j]);
        }
        else
        {
            putchar(pattern[j]);
        }

        if (j < m - 1) putchar(' ');
    }
    putchar(']');

    fflush(stdout);
}

// -------------------------------------------------------
// Update the status / info rows
// -------------------------------------------------------
static void draw_status(int ti, int pi, const char *msg, const char *info)
{
    cursor_move(ROW_STATUS, 1);
    printf(ANSI_ERASE_LINE "  " ANSI_BOLD_ON "Step:" ANSI_BOLD_OFF " text[%d] vs pattern[%d]   %s", ti, pi, msg);

    cursor_move(ROW_INFO, 1);
    printf(ANSI_ERASE_LINE "  %s", info);

    fflush(stdout);
}

// -------------------------------------------------------
// Highlight the active text character
// -------------------------------------------------------
static void draw_text_highlight(const char *text, int n, int active)
{
    cursor_move(ROW_TEXT, 12);
    for (int i = 0; i < n; i++)
    {
        if (i == active)
            printf(ANSI_INVERT_ON "%c" ANSI_INVERT_OFF " ", text[i]);
        else
            printf("%c ", text[i]);
    }
    fflush(stdout);
}

// -------------------------------------------------------
// Matches row
// -------------------------------------------------------
static int match_count = 0;

static void record_match(int pos, int m)
{
    match_count++;
    cursor_move(ROW_MATCHES, 1);
    printf(ANSI_ERASE_LINE "  " ANSI_BOLD_ON "Matches:" ANSI_BOLD_OFF);
    // Reprint on same line — keep it simple, just append position
    // (for many matches we truncate gracefully)
    cursor_move(ROW_MATCHES, 12);
    printf("  #%d at index %d (ends at %d)", match_count, pos, pos + m - 1);
    fflush(stdout);
}

// -------------------------------------------------------
// Core KMP with visualisation
// -------------------------------------------------------
static void kmp_visualise(const char *text, const char *pattern)
{
    int n = (int)strlen(text);
    int m = (int)strlen(pattern);

    if (m == 0 || n == 0) { printf("Empty input.\n"); return; }

    int *lps = calloc(m, sizeof(int));
    if (!lps) { perror("calloc"); return; }

    build_lps(pattern, m, lps);

    // Static chrome drawn once
    draw_chrome(text, pattern, n, m, lps);

    int i = 0; // text pointer
    int j = 0; // pattern pointer

    while (i < n)
    {
        // Show current comparison
        draw_text_highlight(text, n, i);
        draw_pattern(pattern, m, i - j, j, 0, 0);
        draw_status(i, j, "comparing...", "");
        wait(STEP_DELAY_US);

        if (text[i] == pattern[j])
        {
            // Match at this position
            draw_pattern(pattern, m, i - j, j, 1, 0);
            draw_status(i, j, "MATCH  :)", "Characters match — advancing both pointers.");
            wait(STEP_DELAY_US);
            i++;
            j++;

            if (j == m)
            {
                // Full pattern found
                int start = i - j;
                draw_text_highlight(text, n, i - 1);
                draw_pattern(pattern, m, start, m - 1, 1, 0);
                draw_status(i - 1, j - 1, "*** FULL MATCH FOUND ***", "");
                record_match(start, m);
                wait(STEP_DELAY_US * 2);

                j = lps[j - 1]; // fall back using LPS
                char info[128];
                snprintf(info, sizeof(info), "Using LPS[%d]=%d to resume at pattern[%d].", m - 1, j, j);
                draw_status(i, j, "Falling back via LPS...", info);
                wait(STEP_DELAY_US);
            }
        }
        else
        {
            // Mismatch
            draw_pattern(pattern, m, i - j, j, 0, 1);

            if (j == 0)
            {
                draw_status(i, j, "MISMATCH  :(", "No prefix to fall back to — moving text pointer forward.");
                wait(STEP_DELAY_US);
                i++;
            }
            else
            {
                char info[128];
                int fallback = lps[j - 1];
                snprintf(info, sizeof(info),
                         "Mismatch! LPS[%d]=%d  ->  skip to pattern[%d], text stays at %d.",
                         j - 1, fallback, fallback, i);
                draw_status(i, j, "MISMATCH  :(", info);
                wait(STEP_DELAY_US);
                j = fallback;
            }
        }
    }

    // Final state
    draw_text_highlight(text, n, -1); // no highlight
    draw_pattern(pattern, m, 0, -1, 0, 0);

    cursor_move(ROW_STATUS, 1);
    printf(ANSI_ERASE_LINE "  " ANSI_BOLD_ON "Search complete." ANSI_BOLD_OFF
           "  Total matches: %d", match_count);

    cursor_move(ROW_INFO, 1);
    printf(ANSI_ERASE_LINE);

    // Leave cursor at a clean position below the UI
    cursor_move(ROW_LEGEND + 2, 1);
    printf("\n");
    fflush(stdout);

    free(lps);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <text> <pattern>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Allocate with +1 for null terminator  (your original code missed this)
    int tlen = (int)strlen(argv[1]);
    int plen = (int)strlen(argv[2]);

    char *text    = malloc(tlen + 1);
    char *pattern = malloc(plen + 1);
    if (!text || !pattern) { perror("malloc"); return EXIT_FAILURE; }

    strcpy(text,    argv[1]);
    strcpy(pattern, argv[2]);

    printf(ANSI_HIDE_CURSOR);
    match_count = 0;
    kmp_visualise(text, pattern);
    printf(ANSI_SHOW_CURSOR);

    free(text);
    free(pattern);
    return EXIT_SUCCESS;
}