#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

// implementation of binary search for learning purposes

int binary_search(int arr[], int search, int n);
int recursive_binary_search(int arr[], int search, int upperIndex, int lowerIndex);
int multi_key_binary_search(int arr[], const int search, const int n);

// --- ANSI helpers (no colour, monochrome only) ---
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_UNDERLINE   "\033[4m"
#define ANSI_INVERT      "\033[7m"
#define ANSI_RESET       "\033[0m"
#define ANSI_CLEAR_LINE  "\033[2K"
#define ANSI_CURSOR_UP   "\033[%dA"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"
#define ANSI_SAVE_POS    "\033[s"
#define ANSI_LOAD_POS    "\033[u"

#define SLEEP_MS(ms) usleep((ms) * 1000)

// Box-drawing characters for the array visualisation
#define BOX_TL  "\xe2\x94\x8c"   // ┌
#define BOX_TR  "\xe2\x94\x90"   // ┐
#define BOX_BL  "\xe2\x94\x94"   // └
#define BOX_BR  "\xe2\x94\x98"   // ┘
#define BOX_H   "\xe2\x94\x80"   // ─
#define BOX_V   "\xe2\x94\x82"   // │
#define BOX_HU  "\xe2\x94\xb4"   // ┴  (T pointing up)
#define BOX_HD  "\xe2\x94\xac"   // ┬  (T pointing down)
#define ARROW_U "\xe2\x96\xb2"   // ▲
#define ARROW_D "\xe2\x96\xbc"   // ▼
#define DOT     "\xe2\x80\xa2"   // •

#define CELL_PAD 2   // spaces on each side of the number

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

// Print string s centred within cw terminal columns.
// extra_bytes: byte overhead of multi-byte UTF-8 chars (e.g. 2 for ARROW_U).
static void print_centred(const char *s, int cw, int extra_bytes)
{
    int vis_width = (int)strlen(s) - extra_bytes;
    int left  = (cw - vis_width) / 2;
    int right = cw - vis_width - left;
    printf("%*s%s%*s", left, "", s, right, "");
}

// ---------------------------------------------------------------------------
// Visualisation state
// ---------------------------------------------------------------------------

typedef struct {
    int *arr;
    int  n;
    int  search;
    int  cell_width;   // computed once, used everywhere
    // current markers
    int  lower;
    int  upper;
    int  mid;
    // legend label for current step
    char label[128];
    // how many terminal lines the vis block occupies (for redraw)
    int  vis_lines;
} VisState;

static void print_horiz_rule(int n, int cw)
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

// Draw the array with the current lower/upper/mid pointers.
// Returns the number of lines printed.
static int draw_array(VisState *vs)
{
    int lines = 0;
    int cw    = vs->cell_width;
    int vw    = cw - CELL_PAD * 2;   // field width for the number itself

    // ── top border ──
    print_horiz_rule(vs->n, cw);
    lines++;

    // ── cell values ──
    for (int i = 0; i < vs->n; i++) {
        int is_mid   = (i == vs->mid && vs->mid >= 0);
        int in_range = (i >= vs->lower && i <= vs->upper);

        printf("%s", BOX_V);

        if (is_mid)
            printf(ANSI_INVERT ANSI_BOLD);
        else if (!in_range)
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
    // Lead with 1 space for the opening │, then 1-space separator between
    // each index to account for the inner │ separators.
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

    // ── pointer arrows and labels ──
    // (L = lower, U = upper, M = mid; labels merge when pointers coincide)

    // Build per-cell tag strings first
    char tags[vs->n][8];
    for (int i = 0; i < vs->n; i++) tags[i][0] = '\0';

    for (int i = 0; i < vs->n; i++) {
        int is_lower = (i == vs->lower);
        int is_upper = (i == vs->upper);
        int is_mid   = (i == vs->mid && vs->mid >= 0);
        if (!is_lower && !is_upper && !is_mid) continue;
        if (is_lower) strcat(tags[i], "L");
        if (is_mid)   strcat(tags[i], "M");
        if (is_upper) strcat(tags[i], "U");
    }

    // Arrow row
    printf(" ");
    for (int i = 0; i < vs->n; i++) {
        if (tags[i][0] != '\0') {
            printf(ANSI_BOLD);
            // ARROW_U is 3 bytes but 1 terminal column -> extra_bytes = 2
            print_centred(ARROW_U, cw, 2);
            printf(ANSI_RESET);
        } else {
            printf("%*s", cw, "");
        }
        if (i < vs->n - 1) printf(" ");
    }
    printf("\n");
    lines++;

    // Label row
    printf(" ");
    for (int i = 0; i < vs->n; i++) {
        if (tags[i][0] != '\0') {
            printf(ANSI_BOLD);
            print_centred(tags[i], cw, 0);
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
    printf("\n  " ANSI_INVERT ANSI_BOLD "  val  " ANSI_RESET " = mid   "
           ANSI_DIM "  val  " ANSI_RESET " = excluded   "
           "  val   = in range\n");
    printf("  " ANSI_BOLD "L" ANSI_RESET " = lower   "
           ANSI_BOLD "U" ANSI_RESET " = upper   "
           ANSI_BOLD "M" ANSI_RESET " = mid   "
           "searching for: " ANSI_BOLD "%d" ANSI_RESET "\n", vs->search);
    lines += 3;

    return lines;
}

// Move cursor up `lines` and redraw
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
// Visualised search implementations
// ---------------------------------------------------------------------------

int binary_search(int arr[], int search, int n)
{
    VisState vs = {
        .arr        = arr,
        .n          = n,
        .search     = search,
        .cell_width = compute_cell_width(arr, n, search),
        .lower      = 0,
        .upper      = n - 1,
        .mid        = -1,
    };

    printf("\n" ANSI_BOLD "  [ iterative binary search ]" ANSI_RESET "\n\n");

    snprintf(vs.label, sizeof(vs.label),
             "Initial state: lower=0, upper=%d", n - 1);
    vs.vis_lines = 0;
    vs.vis_lines = draw_array(&vs);
    SLEEP_MS(900);

    int step = 1;
    while (vs.lower <= vs.upper)
    {
        int mid = (vs.upper + vs.lower) / 2;
        vs.mid = mid;

        snprintf(vs.label, sizeof(vs.label),
                 "Step %d: mid=%d  arr[%d]=%d  target=%d",
                 step++, mid, mid, arr[mid], search);
        redraw(&vs);
        SLEEP_MS(900);

        if (search > arr[mid])
        {
            vs.lower = mid + 1;
            vs.mid   = -1;
            snprintf(vs.label, sizeof(vs.label),
                     "arr[%d]=%d < target=%d  ->  lower = %d",
                     mid, arr[mid], search, vs.lower);
        }
        else if (search < arr[mid])
        {
            vs.upper = mid - 1;
            vs.mid   = -1;
            snprintf(vs.label, sizeof(vs.label),
                     "arr[%d]=%d > target=%d  ->  upper = %d",
                     mid, arr[mid], search, vs.upper);
        }
        else
        {
            snprintf(vs.label, sizeof(vs.label),
                     "Found %d at index %d  " ANSI_BOLD DOT ANSI_RESET,
                     search, mid);
            redraw(&vs);
            SLEEP_MS(600);
            printf("\n");
            return EXIT_SUCCESS;
        }

        redraw(&vs);
        SLEEP_MS(700);
    }

    vs.mid = -1;
    snprintf(vs.label, sizeof(vs.label), "lower > upper: %d not found", search);
    redraw(&vs);
    SLEEP_MS(600);
    printf("\n");
    return EXIT_FAILURE;
}

// ---------------------------------------------------------------------------

static int _rec_vis(VisState *vs, int step)
{
    if (vs->lower > vs->upper)
    {
        vs->mid = -1;
        snprintf(vs->label, sizeof(vs->label),
                 "lower > upper: %d not found", vs->search);
        redraw(vs);
        SLEEP_MS(700);
        printf("\n");
        return EXIT_FAILURE;
    }

    int mid = (vs->upper + vs->lower) / 2;
    vs->mid = mid;

    snprintf(vs->label, sizeof(vs->label),
             "Step %d (depth %d): mid=%d  arr[%d]=%d  target=%d",
             step, step, mid, mid, vs->arr[mid], vs->search);
    redraw(vs);
    SLEEP_MS(900);

    if (vs->search > vs->arr[mid])
    {
        vs->lower = mid + 1;
        vs->mid   = -1;
        snprintf(vs->label, sizeof(vs->label),
                 "arr[%d]=%d < target  ->  recurse: lower=%d",
                 mid, vs->arr[mid], vs->lower);
        redraw(vs);
        SLEEP_MS(700);
        return _rec_vis(vs, step + 1);
    }
    else if (vs->search < vs->arr[mid])
    {
        vs->upper = mid - 1;
        vs->mid   = -1;
        snprintf(vs->label, sizeof(vs->label),
                 "arr[%d]=%d > target  ->  recurse: upper=%d",
                 mid, vs->arr[mid], vs->upper);
        redraw(vs);
        SLEEP_MS(700);
        return _rec_vis(vs, step + 1);
    }
    else
    {
        snprintf(vs->label, sizeof(vs->label),
                 "Found %d at index %d  " ANSI_BOLD DOT ANSI_RESET,
                 vs->search, mid);
        redraw(vs);
        SLEEP_MS(600);
        printf("\n");
        return EXIT_SUCCESS;
    }
}

int recursive_binary_search(int arr[], int search, int upperIndex, int lowerIndex)
{
    VisState vs = {
        .arr        = arr,
        .n          = upperIndex,  // upperIndex passed as n from main
        .search     = search,
        .cell_width = compute_cell_width(arr, upperIndex, search),
        .lower      = lowerIndex,
        .upper      = upperIndex - 1,
        .mid        = -1,
    };

    printf("\n" ANSI_BOLD "  [ recursive binary search ]" ANSI_RESET "\n\n");

    snprintf(vs.label, sizeof(vs.label),
             "Initial state: lower=%d, upper=%d", lowerIndex, upperIndex - 1);
    vs.vis_lines = 0;
    vs.vis_lines = draw_array(&vs);
    SLEEP_MS(900);

    return _rec_vis(&vs, 1);
}

// ---------------------------------------------------------------------------

int multi_key_binary_search(int arr[], const int search, const int n)
{
    VisState vs = {
        .arr        = arr,
        .n          = n,
        .search     = search,
        .cell_width = compute_cell_width(arr, n, search),
        .lower      = 0,
        .upper      = n - 1,
        .mid        = -1,
    };

    printf("\n" ANSI_BOLD "  [ multi-key binary search ]" ANSI_RESET "\n\n");

    snprintf(vs.label, sizeof(vs.label),
             "Initial state: lower=0, upper=%d", n - 1);
    vs.vis_lines = 0;
    vs.vis_lines = draw_array(&vs);
    SLEEP_MS(900);

    int found = 0;
    int step  = 1;

    while (vs.lower <= vs.upper)
    {
        int mid = (vs.upper + vs.lower) / 2;
        vs.mid  = mid;

        snprintf(vs.label, sizeof(vs.label),
                 "Step %d: mid=%d  arr[%d]=%d  target=%d",
                 step++, mid, mid, arr[mid], search);
        redraw(&vs);
        SLEEP_MS(900);

        if (search < arr[mid])
        {
            vs.upper = mid - 1;
            vs.mid   = -1;
            snprintf(vs.label, sizeof(vs.label),
                     "arr[%d]=%d > target  ->  upper=%d", mid, arr[mid], vs.upper);
            redraw(&vs);
            SLEEP_MS(700);
        }
        else if (search > arr[mid])
        {
            vs.lower = mid + 1;
            vs.mid   = -1;
            snprintf(vs.label, sizeof(vs.label),
                     "arr[%d]=%d < target  ->  lower=%d", mid, arr[mid], vs.lower);
            redraw(&vs);
            SLEEP_MS(700);
        }
        else
        {
            found = 1;
            snprintf(vs.label, sizeof(vs.label),
                     "Found %d at index %d -- scanning neighbours...",
                     search, mid);
            redraw(&vs);
            SLEEP_MS(700);

            // scan right
            int i = mid + 1;
            while (i < n && arr[i] == search) {
                vs.mid = i;
                snprintf(vs.label, sizeof(vs.label),
                         "Duplicate at index %d (scanning right)", i);
                redraw(&vs);
                SLEEP_MS(500);
                i++;
            }

            // scan left
            i = mid - 1;
            while (i >= 0 && arr[i] == search) {
                vs.mid = i;
                snprintf(vs.label, sizeof(vs.label),
                         "Duplicate at index %d (scanning left)", i);
                redraw(&vs);
                SLEEP_MS(500);
                i--;
            }

            vs.mid = mid;
            snprintf(vs.label, sizeof(vs.label),
                     "Scan complete for value %d  " ANSI_BOLD DOT ANSI_RESET, search);
            redraw(&vs);
            SLEEP_MS(600);
            printf("\n");
            return EXIT_SUCCESS;
        }
    }

    if (!found) {
        vs.mid = -1;
        snprintf(vs.label, sizeof(vs.label), "lower > upper: %d not found", search);
        redraw(&vs);
        SLEEP_MS(600);
    }
    printf("\n");
    return found ? EXIT_SUCCESS : EXIT_FAILURE;
}

// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <sorted numbers...> <search_value>\n", argv[0]);
        printf("  e.g: %s 1 3 5 7 9 11 13 7\n", argv[0]);
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

    binary_search(arr, search, n);
    recursive_binary_search(arr, search, n, 0);
    multi_key_binary_search(arr, search, n);

    printf(ANSI_SHOW_CURSOR);

    return EXIT_SUCCESS;
}