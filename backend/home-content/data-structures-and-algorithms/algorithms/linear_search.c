#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

// implementation of linear search for learning purposes

int linear_search(int arr[], const int n, const int search);

// --- ANSI helpers (no colour, monochrome only) ---
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_UNDERLINE   "\033[4m"
#define ANSI_INVERT      "\033[7m"
#define ANSI_RESET       "\033[0m"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"

#define SLEEP_MS(ms) usleep((ms) * 1000)

// Box-drawing characters
#define BOX_TL  "\xe2\x94\x8c"   // ┌
#define BOX_TR  "\xe2\x94\x90"   // ┐
#define BOX_BL  "\xe2\x94\x94"   // └
#define BOX_BR  "\xe2\x94\x98"   // ┘
#define BOX_H   "\xe2\x94\x80"   // ─
#define BOX_V   "\xe2\x94\x82"   // │
#define BOX_HU  "\xe2\x94\xb4"   // ┴
#define BOX_HD  "\xe2\x94\xac"   // ┬
#define ARROW_U "\xe2\x96\xb2"   // ▲  (3 bytes UTF-8, 1 terminal column)
#define DOT     "\xe2\x80\xa2"   // •

#define CELL_PAD 2  // spaces on each side of the number

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Number of characters needed to print an integer (including '-' sign)
static int num_width(int v)
{
    if (v == 0) return 1;
    int w = (v < 0) ? 1 : 0;
    if (v < 0) v = -v;
    while (v > 0) { w++; v /= 10; }
    return w;
}

// Compute cell width from the widest value in the array (and the search term)
static int compute_cell_width(int arr[], int n, int search)
{
    int max_w = num_width(search);
    for (int i = 0; i < n; i++) {
        int w = num_width(arr[i]);
        if (w > max_w) max_w = w;
    }
    return max_w + CELL_PAD * 2;
}

// ---------------------------------------------------------------------------
// Visualisation state
// ---------------------------------------------------------------------------

typedef struct {
    int *arr;
    int  n;
    int  search;
    int  cell_width;            // computed once, used everywhere
    int  cursor;                // index currently being inspected (-1 = none)
    int  found;                 // index of match (-1 = not yet found)
    char label[128];
    int  vis_lines;
} VisState;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void print_top_border(int n, int cw)
{
    printf("%s", BOX_TL);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < cw; j++) printf("%s", BOX_H);
        if (i < n - 1) printf("%s", BOX_HD);
    }
    printf("%s\n", BOX_TR);
}

static void print_bottom_border(int n, int cw)
{
    printf("%s", BOX_BL);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < cw; j++) printf("%s", BOX_H);
        if (i < n - 1) printf("%s", BOX_HU);
    }
    printf("%s\n", BOX_BR);
}

// Print string s centred within cw terminal columns.
// extra_bytes: byte overhead of multi-byte UTF-8 chars (e.g. 2 for ARROW_U).
static void print_centred(const char *s, int cw, int extra_bytes)
{
    int vis_width = (int)strlen(s) - extra_bytes;
    int left  = (cw - vis_width) / 2;
    int right = cw - vis_width - left;
    printf("%*s%s%*s", left, "", s, right, "");
}

static int draw_array(VisState *vs)
{
    int lines = 0;
    int cw    = vs->cell_width;
    int vw    = cw - CELL_PAD * 2;   // field width for the number itself

    // ── top border ──
    print_top_border(vs->n, cw);
    lines++;

    // ── cell values ──
    for (int i = 0; i < vs->n; i++) {
        int is_cursor = (i == vs->cursor);
        int is_found  = (i == vs->found && vs->found >= 0);
        int is_past   = (vs->cursor >= 0 && i < vs->cursor && !is_found);

        printf("%s", BOX_V);

        if (is_found)
            printf(ANSI_INVERT ANSI_BOLD);
        else if (is_cursor)
            printf(ANSI_BOLD);
        else if (is_past)
            printf(ANSI_DIM);

        printf("%*s%*d%*s", CELL_PAD, "", vw, vs->arr[i], CELL_PAD, "");
        printf(ANSI_RESET);
    }
    printf("%s\n", BOX_V);
    lines++;

    // ── bottom border ──
    print_bottom_border(vs->n, cw);
    lines++;

    // ── index labels (centred, dimmed) ──
    // Each cell in the value row is preceded by a │ (1 col). To align indices
    // under their cells we lead with 1 space (for the first │) and print a
    // 1-space separator between each index (for the inner │ separators).
    printf(" ");
    for (int i = 0; i < vs->n; i++) {
        char idx[16];
        snprintf(idx, sizeof(idx), "%d", i);
        printf(ANSI_DIM);
        print_centred(idx, cw, 0);
        printf(ANSI_RESET);
        if (i < vs->n - 1) printf(" ");
    }
    printf("\n");
    lines++;

    // ── arrow row ──
    printf(" ");
    for (int i = 0; i < vs->n; i++) {
        int active = (i == vs->cursor) || (i == vs->found && vs->found >= 0);
        if (active) {
            printf(ANSI_BOLD);
            print_centred(ARROW_U, cw, 2);  // ARROW_U: 3 bytes, 1 column -> extra=2
            printf(ANSI_RESET);
        } else {
            printf("%*s", cw, "");
        }
        if (i < vs->n - 1) printf(" ");
    }
    printf("\n");
    lines++;

    // ── pointer label row ──
    printf(" ");
    for (int i = 0; i < vs->n; i++) {
        int active = (i == vs->cursor) || (i == vs->found && vs->found >= 0);
        if (active) {
            const char *tag = (i == vs->found && vs->found >= 0) ? "*" : "i";
            printf(ANSI_BOLD);
            print_centred(tag, cw, 0);
            printf(ANSI_RESET);
        } else {
            printf("%*s", cw, "");
        }
        if (i < vs->n - 1) printf(" ");
    }
    printf("\n");
    lines++;

    // ── step label ──
    printf("\n  " ANSI_UNDERLINE "%s" ANSI_RESET "\n", vs->label);
    lines += 2;

    // ── legend ──
    printf("\n  " ANSI_INVERT ANSI_BOLD "  val  " ANSI_RESET " = match   "
           ANSI_BOLD "  val  " ANSI_RESET " = checking   "
           ANSI_DIM "  val  " ANSI_RESET " = visited   "
           "  val   = unseen\n");
    printf("  " ANSI_BOLD "i" ANSI_RESET " = current index   "
           ANSI_BOLD "*" ANSI_RESET " = found   "
           "searching for: " ANSI_BOLD "%d" ANSI_RESET "\n", vs->search);
    lines += 3;

    return lines;
}

static void redraw(VisState *vs)
{
    if (vs->vis_lines > 0) {
        printf("\033[%dA", vs->vis_lines);
        for (int i = 0; i < vs->vis_lines; i++) printf("\033[2K\n");
        printf("\033[%dA", vs->vis_lines);
    }
    vs->vis_lines = draw_array(vs);
}

// ---------------------------------------------------------------------------
// Visualised linear search
// ---------------------------------------------------------------------------

int linear_search(int arr[], const int n, const int search)
{
    VisState vs = {
        .arr        = arr,
        .n          = n,
        .search     = search,
        .cell_width = compute_cell_width(arr, n, search),
        .cursor     = -1,
        .found      = -1,
    };

    printf("\n" ANSI_BOLD "  [ linear search ]" ANSI_RESET "\n\n");

    snprintf(vs.label, sizeof(vs.label), "Starting scan for %d", search);
    vs.vis_lines = 0;
    vs.vis_lines = draw_array(&vs);
    SLEEP_MS(800);

    for (int i = 0; i < n; i++)
    {
        vs.cursor = i;
        snprintf(vs.label, sizeof(vs.label),
                 "Checking index %d: arr[%d] = %d", i, i, arr[i]);
        redraw(&vs);
        SLEEP_MS(700);

        if (arr[i] == search)
        {
            vs.found = i;
            snprintf(vs.label, sizeof(vs.label),
                     "arr[%d] = %d == %d  ->  found!  " ANSI_BOLD DOT ANSI_RESET,
                     i, arr[i], search);
            redraw(&vs);
            SLEEP_MS(600);
            printf("\n");
            return EXIT_SUCCESS;
        }

        snprintf(vs.label, sizeof(vs.label),
                 "arr[%d] = %d != %d  ->  continue", i, arr[i], search);
        redraw(&vs);
        SLEEP_MS(500);
    }

    vs.cursor = -1;
    snprintf(vs.label, sizeof(vs.label), "Reached end of array: %d not found", search);
    redraw(&vs);
    SLEEP_MS(600);
    printf("\n");
    return EXIT_FAILURE;
}

// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <numbers...> <search_value>\n", argv[0]);
        printf("  e.g: %s 4 2 7 1 9 3 7\n", argv[0]);
        return EXIT_FAILURE;
    }

    int n = argc - 2;
    int arr[n];

    for (int i = 1; i <= n; i++)
    {
        arr[i - 1] = atoi(argv[i]);
    }

    const int search = atoi(argv[argc - 1]);

    printf(ANSI_HIDE_CURSOR);
    linear_search(arr, n, search);
    printf(ANSI_SHOW_CURSOR);

    return EXIT_SUCCESS;
}