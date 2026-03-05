#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── delays (microseconds) ───────────────────────────────────────────── */
#define DELAY_APPROACH  160000   /* before a comparison is made          */
#define DELAY_CHAR_HIT  200000   /* after a single-character match       */
#define DELAY_CHAR_MISS 350000   /* after a mismatch                     */
#define DELAY_FULL_HIT  900000   /* when the whole pattern is found      */

/* ── shared found-index log ──────────────────────────────────────────── */
static int g_found[1024];
static int g_found_count = 0;

/*
 * draw()
 *
 *  result    :  0  = about to compare (neutral)
 *               1  = current character matched
 *              -1  = current character did not match
 *  full_match:  1  = entire pattern just matched at 'offset'
 *
 *  ANSI used (no colour):
 *    \033[7m   reverse video  — active comparison / full match
 *    \033[4m   underline      — characters confirmed matched so far
 *    \033[1m   bold           — mismatch marker
 *    \033[0m   reset
 *    \033[H    cursor home
 *    \033[K    erase to end of line
 *    \033[?25l hide cursor    (set once in main)
 *    \033[?25h show cursor    (restored in main)
 */
static void draw(const char *string, const char *pattern,
                 int offset, int cmp_pos, int result, int full_match)
{
    int slen = (int)strlen(string);
    int plen = (int)strlen(pattern);

    printf("\033[H");   /* jump to top-left without clearing (no flicker) */

    /* ── row 1 : the string ─────────────────────────────────────────── */
    printf("  ");
    for (int i = 0; i < slen; i++) {
        int in_window    = (i >= offset && i < offset + plen);
        int already_seen = (i >= offset && i < offset + cmp_pos);
        int is_current   = (i == offset + cmp_pos);

        if      (full_match && in_window)       printf("\033[7m");
        else if (is_current && result ==  1)    printf("\033[7m\033[4m");
        else if (is_current && result == -1)    printf("\033[1m\033[7m");
        else if (is_current)                    printf("\033[7m");
        else if (already_seen)                  printf("\033[4m");

        printf("%c\033[0m ", string[i]);
    }
    printf("\033[K\n");

    /* ── row 2 : the pattern (offset by 2 chars per string position) ── */
    printf("  ");
    for (int k = 0; k < offset; k++) printf("  ");
    for (int j = 0; j < plen; j++) {
        int already_seen = (j < cmp_pos);
        int is_current   = (j == cmp_pos);

        if      (full_match)                    printf("\033[7m");
        else if (is_current && result ==  1)    printf("\033[7m\033[4m");
        else if (is_current && result == -1)    printf("\033[1m\033[7m");
        else if (is_current)                    printf("\033[7m");
        else if (already_seen)                  printf("\033[4m");

        printf("%c\033[0m ", pattern[j]);
    }
    printf("\033[K\n");

    /* ── row 3 : separator ──────────────────────────────────────────── */
    printf("\033[K\n");

    /* ── row 4 : step status ────────────────────────────────────────── */
    if (full_match) {
        printf("  >> match at index %d <<\033[K\n", offset);
    } else if (result == -1) {
        printf("  mismatch: string[%d]='%c'  pattern[%d]='%c'\033[K\n",
               offset + cmp_pos, string[offset + cmp_pos],
               cmp_pos,          pattern[cmp_pos]);
    } else {
        printf("  offset %-3d  comparing: "
               "string[%d]='%c'  pattern[%d]='%c'\033[K\n",
               offset,
               offset + cmp_pos, string[offset + cmp_pos],
               cmp_pos,          pattern[cmp_pos]);
    }

    /* ── row 5 : accumulated results ───────────────────────────────── */
    printf("  found at: ");
    if (g_found_count == 0) {
        printf("(none)");
    } else {
        for (int f = 0; f < g_found_count; f++) {
            if (f) printf(", ");
            printf("%d", g_found[f]);
        }
    }
    printf("\033[K\n");

    fflush(stdout);
}

/* ── naive search with live visualisation ──────────────────────────── */
int naive_pattern_search(const char *string, const char *pattern)
{
    int slen = (int)strlen(string);
    int plen = (int)strlen(pattern);

    if (plen == 0 || plen > slen) return EXIT_FAILURE;

    printf("\033[2J");  /* clear screen once per string */

    for (int i = 0; i <= slen - plen; i++) {
        int full = 1;

        for (int j = 0; j < plen; j++) {

            /* show the pair about to be compared */
            draw(string, pattern, i, j, 0, 0);
            usleep(DELAY_APPROACH);

            if (string[i + j] == pattern[j]) {
                draw(string, pattern, i, j, 1, 0);
                usleep(DELAY_CHAR_HIT);
            } else {
                draw(string, pattern, i, j, -1, 0);
                usleep(DELAY_CHAR_MISS);
                full = 0;
                break;
            }
        }

        if (full) {
            g_found[g_found_count++] = i;
            draw(string, pattern, i, plen - 1, 1, 1);
            usleep(DELAY_FULL_HIT);
        }
    }

    return EXIT_SUCCESS;
}

/* ── entry point ────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    /* usage: ./naive_search <string1> [string2 ...] <pattern> */
    if (argc < 3) {
        fprintf(stderr, "usage: %s <string> [strings...] <pattern>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("\033[?25l");    /* hide cursor for clean animation */

    int str_count = argc - 2;
    char **strings = (char **)malloc(sizeof(char *) * (size_t)str_count);

    for (int i = 0; i < str_count; i++) {
        size_t len  = strlen(argv[i + 1]);
        strings[i]  = (char *)malloc(len + 1);   /* +1 for '\0' */
        strcpy(strings[i], argv[i + 1]);
    }

    size_t plen  = strlen(argv[argc - 1]);
    char *pattern = (char *)malloc(plen + 1);     /* +1 for '\0' */
    strcpy(pattern, argv[argc - 1]);

    for (int i = 0; i < str_count; i++) {
        g_found_count = 0;
        naive_pattern_search(strings[i], pattern);
    }

    for (int i = 0; i < str_count; i++) free(strings[i]);
    free(strings);
    free(pattern);

    printf("\033[?25h");    /* restore cursor */
    printf("\n");

    return EXIT_SUCCESS;
}