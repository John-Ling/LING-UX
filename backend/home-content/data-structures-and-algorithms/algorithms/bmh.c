#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── ANSI escape sequences (no colour; monochrome only) ─────────────── */
#define CLEAR   "\033[2J\033[H"
#define BOLD    "\033[1m"
#define REV     "\033[7m"
#define UL      "\033[4m"
#define RESET   "\033[0m"

/* ── Timing ─────────────────────────────────────────────────────────── */
#define DELAY_STEP   500000   /* 0.5 s between comparison steps  */
#define DELAY_RESULT 1000000  /* 1.0 s to show match / shift     */

/* ── Visualisation state flags ───────────────────────────────────────── */
#define VIS_SEARCHING  0   /* actively comparing a character      */
#define VIS_MATCH     1   /* full match confirmed                 */
#define VIS_MISMATCH -1   /* mismatch found                      */

/* ─────────────────────────────────────────────────────────────────────
 * draw_table
 * Renders the bad character (shift) table.
 * Only the characters that appear in pattern[0 .. m-2] receive a custom
 * entry; everything else uses the default shift of m.
 * ───────────────────────────────────────────────────────────────────── */
static void draw_table(const char *pat, int m, const int bad[256])
{
    int seen[256] = {0};

    printf("  Bad Character Table\n");
    printf("  +----------+-------+\n");
    printf("  | " BOLD "Char    " RESET " | " BOLD "Shift" RESET " |\n");
    printf("  +----------+-------+\n");

    for (int i = 0; i < m - 1; i++) {
        unsigned char c = (unsigned char)pat[i];
        if (!seen[c]) {
            seen[c] = 1;
            printf("  |   ' %c '  | %5d |\n", (char)c, bad[c]);
        }
    }
    printf("  |  (other) | %5d |\n", m);
    printf("  +----------+-------+\n\n");
}

/* ─────────────────────────────────────────────────────────────────────
 * draw_frame
 *
 * Clears the screen and redraws the current search state.
 *
 * Parameters
 *   text       – the haystack
 *   n          – length of text
 *   pat        – the pattern
 *   m          – length of pattern
 *   bad        – shift table
 *   pos        – current window start index
 *   cmp        – pattern index currently being compared (-1 = none)
 *   state      – VIS_SEARCHING / VIS_MATCH / VIS_MISMATCH
 *   shift_pos  – text index of the shift character (-1 = not shown)
 *   shift_amt  – shift amount to display   (-1 = not shown)
 * ───────────────────────────────────────────────────────────────────── */
static void draw_frame(
    const char *text, int n,
    const char *pat,  int m,
    const int   bad[256],
    int pos, int cmp, int state,
    int shift_pos, int shift_amt)
{
    printf(CLEAR);
    draw_table(pat, m, bad);

    /* ── text row ──────────────────────────────────────────────────── */
    printf("  Text:    ");
    for (int i = 0; i < n; i++) {
        int in_win = (i >= pos && i < pos + m);

        if (state == VIS_MATCH && in_win) {
            printf(REV BOLD "[%c]" RESET, text[i]);

        } else if (i == shift_pos) {
            /* the character driving the shift; underlined */
            printf(UL "[%c]" RESET, text[i]);

        } else if (cmp >= 0 && i == pos + cmp) {
            /* the character currently under comparison */
            if (state == VIS_MISMATCH)
                printf(REV " %c " RESET, text[i]);
            else
                printf(BOLD " %c " RESET, text[i]);

        } else if (in_win) {
            printf(BOLD " %c " RESET, text[i]);
        } else {
            printf(" %c ", text[i]);
        }
    }
    printf("\n");

    /* ── pattern row (aligned to pos) ─────────────────────────────── */
    printf("  Pattern: ");
    for (int i = 0; i < pos; i++) printf("   ");   /* 3 chars per cell */

    for (int i = 0; i < m; i++) {
        if (state == VIS_MATCH) {
            printf(REV BOLD "[%c]" RESET, pat[i]);

        } else if (cmp >= 0 && i == cmp) {
            if (state == VIS_MISMATCH)
                printf(REV " %c " RESET, pat[i]);
            else
                printf(BOLD " %c " RESET, pat[i]);

        } else {
            printf(BOLD " %c " RESET, pat[i]);
        }
    }
    printf("\n\n");

    /* ── status line ──────────────────────────────────────────────── */
    printf("  ");
    if (state == VIS_MATCH) {
        printf(BOLD "Match found at index %d" RESET, pos);

    } else if (state == VIS_MISMATCH && shift_amt > 0) {
        printf("Mismatch  |  shift by bad_char['%c'] = %d",
               text[shift_pos], shift_amt);

    } else if (state == VIS_MISMATCH) {
        printf("Mismatch: text[%d]='%c'  !=  pattern[%d]='%c'",
               pos + cmp, text[pos + cmp], cmp, pat[cmp]);

    } else if (cmp >= 0) {
        printf("Comparing text[%d]='%c'  ==?  pattern[%d]='%c'",
               pos + cmp, text[pos + cmp], cmp, pat[cmp]);

    } else {
        printf("Aligning pattern at index %d ...", pos);
    }

    printf("\n");
    fflush(stdout);
}

/* ─────────────────────────────────────────────────────────────────────
 * bmh  –  Boyer-Moore-Horspool search with terminal visualisation
 * ───────────────────────────────────────────────────────────────────── */
int bmh(const char *text, const char *pat)
{
    int n = (int)strlen(text);
    int m = (int)strlen(pat);
    int found = 0;

    if (m == 0 || m > n) {
        printf("Pattern length invalid for this text.\n");
        return EXIT_FAILURE;
    }

    /* ── build bad character (shift) table ────────────────────────── */
    int bad[256];
    for (int i = 0; i < 256; i++)
        bad[i] = m;
    for (int i = 0; i < m - 1; i++)           /* note: excludes last char */
        bad[(unsigned char)pat[i]] = m - 1 - i;

    /* ── search ────────────────────────────────────────────────────── */
    int pos = 0;
    while (pos <= n - m) {

        /* show alignment before comparing */
        draw_frame(text, n, pat, m, bad, pos, -1, VIS_SEARCHING, -1, -1);
        usleep(DELAY_STEP);

        /* compare right-to-left */
        int j = m - 1;
        while (j >= 0) {
            draw_frame(text, n, pat, m, bad, pos, j, VIS_SEARCHING, -1, -1);
            usleep(DELAY_STEP);
            if (text[pos + j] != pat[j]) break;
            j--;
        }

        /* the shift is ALWAYS driven by the last character of the window */
        int shift_pos = pos + m - 1;
        int shift_amt = bad[(unsigned char)text[shift_pos]];

        if (j < 0) {
            /* ── full match ──────────────────────────────────────── */
            draw_frame(text, n, pat, m, bad, pos, -1, VIS_MATCH, -1, -1);
            usleep(DELAY_RESULT);
            found = 1;

        } else {
            /* ── mismatch: show which chars disagreed ─────────────── */
            draw_frame(text, n, pat, m, bad, pos, j, VIS_MISMATCH, -1, -1);
            usleep(DELAY_STEP);

            /* show which window character drives the shift */
            draw_frame(text, n, pat, m, bad, pos, j, VIS_MISMATCH,
                       shift_pos, shift_amt);
            usleep(DELAY_RESULT);
        }

        pos += shift_amt;
    }

    if (!found) {
        printf(CLEAR);
        draw_table(pat, m, bad);
        printf("  Pattern not found.\n");
        fflush(stdout);
    }

    return found ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* ─────────────────────────────────────────────────────────────────────
 * main
 * Usage:  ./bmh <text1> [text2 ...] <pattern>
 * ───────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <text1> [text2 ...] <pattern>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_texts = argc - 2;

    char **texts = (char **)malloc(sizeof(char *) * num_texts);
    if (!texts) return EXIT_FAILURE;

    for (int i = 0; i < num_texts; i++) {
        size_t len = strlen(argv[i + 1]);
        texts[i] = (char *)malloc(len + 1);   /* +1 for null terminator */
        if (!texts[i]) return EXIT_FAILURE;
        strcpy(texts[i], argv[i + 1]);
    }

    size_t plen = strlen(argv[argc - 1]);
    char *pattern = (char *)malloc(plen + 1);
    if (!pattern) return EXIT_FAILURE;
    strcpy(pattern, argv[argc - 1]);

    for (int i = 0; i < num_texts; i++) {
        bmh(texts[i], pattern);
        if (i < num_texts - 1) sleep(2);
    }

    for (int i = 0; i < num_texts; i++) free(texts[i]);
    free(texts);
    free(pattern);

    return EXIT_SUCCESS;
}