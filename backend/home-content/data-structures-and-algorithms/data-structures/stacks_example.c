#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stacks.h"
#include "utils.h"

/*
 * Terminal visualisation of a stack implemented as a singly linked list.
 * Nodes are drawn vertically – top of stack at the top.
 * Uses only ANSI escape sequences, monochrome.
 */

/* ── ANSI helpers ───────────────────────────────────────────────── */
#define CLEAR    "\033[2J\033[H"
#define HIDE_CUR "\033[?25l"
#define SHOW_CUR "\033[?25h"

static void at(int r, int c) { printf("\033[%d;%dH", r, c); }

/* ── Layout constants ───────────────────────────────────────────── */
#define INNER       10          /* printable chars inside box           */
#define BOX_W       (INNER + 2) /* 12 cols incl. '|' borders            */
#define COL0         4          /* left column of every box             */
#define MID_COL     (COL0 + BOX_W / 2) /* column of vertical arrow     */

/* Rows: status line, then head badge, then list grows downward */
#define R_STATUS     1
#define R_BADGE      3   /* "[ TOP ]"                                  */
#define R_STEM       4   /* "|"                                        */
#define R_TIP        5   /* "v"                                        */
#define R_FIRST      6   /* top edge of first (head) box               */

/* Each node occupies: 3 rows (box) + 2 rows ("|" + "v" arrow) = 5   */
#define NODE_STRIDE  5

#define DELAY  750000u   /* us between animation frames                */

/* ── Helpers ────────────────────────────────────────────────────── */

static int collect(const Stack *s, const char **out, int max)
{
    int n = 0;
    if (!s || !s->items) return 0;
    for (ListNode *p = s->items->head; p && n < max; p = p->next)
        out[n++] = (const char *)p->value;
    return n;
}

/* Draw a box whose top-left corner is at (row, COL0).
 * marked=1 replaces inner padding with '*' to highlight the node. */
static void draw_box(int row, const char *label, int marked)
{
    int len  = (int)strlen(label);
    int lpad = (INNER - len) / 2;
    int rpad = INNER - len - lpad;
    char f   = marked ? '*' : ' ';

    at(row,     COL0); printf("+"); for (int i=0;i<INNER;i++) putchar('-'); printf("+");
    at(row + 1, COL0); printf("|");
    for (int i = 0; i < lpad; i++) putchar(f);
    printf("%s", label);
    for (int i = 0; i < rpad; i++) putchar(f);
    printf("|");
    at(row + 2, COL0); printf("+"); for (int i=0;i<INNER;i++) putchar('-'); printf("+");
}

/* Draw the downward connector: "|" then "v" at the given row. */
static void draw_arrow(int row)
{
    at(row,     MID_COL); printf("|");
    at(row + 1, MID_COL); printf("v");
}

/* Draw a dashed downward connector (push preview). */
static void draw_dashed_arrow(int row)
{
    at(row,     MID_COL); printf(".");
    at(row + 1, MID_COL); printf(".");
}

/* ── Standard full-state redraw ─────────────────────────────────── */
/*
 *   [ TOP ]
 *      |
 *      v
 *  +----------+
 *  |  Jun An  |
 *  +----------+
 *      |
 *      v
 *  +----------+
 *  |  Frank   |
 *  +----------+
 *      |
 *      v
 *  +----------+
 *  |  Andrew  |
 *  +----------+
 *      |
 *      v
 *    NULL  [ TAIL ]
 */
static void redraw(const Stack *s, const char *msg, int mark_top)
{
    printf(CLEAR);
    at(R_STATUS, 2); printf("%s", msg ? msg : "");

    at(R_BADGE, COL0); printf("[ TOP ]");
    draw_arrow(R_STEM);   /* "|" at R_STEM, "v" at R_TIP */

    const char *items[64];
    int n = collect(s, items, 64);

    if (n == 0) {
        at(R_FIRST, MID_COL - 1); printf("NULL");
        fflush(stdout);
        return;
    }

    int row = R_FIRST;
    for (int i = 0; i < n; i++) {
        draw_box(row, items[i], mark_top && i == 0);
        row += 3; /* past the box */

        if (i < n - 1) {
            draw_arrow(row);
        } else {
            /* Last node -> NULL + TAIL label */
            draw_arrow(row);
            at(row + 2, COL0); printf("NULL  [ TAIL ]");
            row += 2;
        }
        row += 2; /* past the arrow */
    }
    fflush(stdout);
}

/* ── Animated push ──────────────────────────────────────────────── */
/*
 * Frame 1 - incoming node floats above the list with a dashed arrow:
 *
 *   (incoming)
 *  +----------+
 *  |  Jun An  |    <- new node, not yet linked
 *  +----------+
 *       .          <- dashed "next" hint
 *       .
 *  +----------+
 *  |  Frank   |    <- existing head
 *  +----------+
 *       ...
 *
 * Frame 2 - node inserted; new head highlighted with '*'.
 */
static void viz_push(Stack *s, const char *val)
{
    char msg[80];

    /* ── frame 1 ── */
    snprintf(msg, sizeof msg, "push(\"%s\")", val);
    printf(CLEAR);
    at(R_STATUS, 2); printf("%s", msg);

    /* Incoming node sits above where R_FIRST normally is */
    int inc_row  = R_FIRST;      /* incoming box top   */
    int dash_row = inc_row + 3;  /* dashed arrow below */
    int exist_row = dash_row + 2;/* existing list start */

    at(inc_row - 1, COL0); printf("(incoming)");
    draw_box(inc_row, val, 0);
    draw_dashed_arrow(dash_row);

    const char *items[64];
    int n = collect(s, items, 64);

    if (n == 0) {
        at(exist_row, MID_COL - 1); printf("NULL");
    } else {
        int row = exist_row;
        for (int i = 0; i < n; i++) {
            draw_box(row, items[i], 0);
            row += 3;
            if (i < n - 1) {
                draw_arrow(row);
            } else {
                draw_arrow(row);
                at(row + 2, COL0); printf("NULL  [ TAIL ]");
            }
            row += 2;
        }
    }
    fflush(stdout);
    usleep(DELAY);

    /* ── frame 2 - push complete ── */
    LibStack.push_str(s, val);
    snprintf(msg, sizeof msg, "push(\"%s\")  --  inserted at top", val);
    redraw(s, msg, 1 /* highlight new head */);
    usleep(DELAY);
}

/* ── Animated pop ───────────────────────────────────────────────── */
/*
 * Frame 1 - highlights the top node with '*' to show what will go.
 * Frame 2 - node removed, list redrawn.
 */
static void viz_pop(Stack *s)
{
    if (LibStack.is_empty(s)) {
        redraw(s, "pop()  --  stack is empty!", 0);
        usleep(DELAY);
        return;
    }

    const char *items[64];
    collect(s, items, 64);

    char msg[80];
    snprintf(msg, sizeof msg, "pop()  --  removing \"%s\"...", items[0]);
    redraw(s, msg, 1 /* highlight top */);
    usleep(DELAY);

    LibStack.pop(s, NULL);
    redraw(s, "pop()  --  done", 0);
    usleep(DELAY);
}
/* ── Entry point ─────────────────────────────────────────────────── */
int main(void)
{
    printf(HIDE_CUR);

    Stack *stack = LibStack.create(NULL, 0, sizeof(char *));

    redraw(stack, "Stack initialised  (empty)", 0);
    usleep(DELAY);

    viz_push(stack, "Jesse");
    viz_push(stack, "Saul");
    viz_push(stack, "Hank");
    viz_push(stack, "Walter");
    viz_push(stack, "Gus");

    viz_pop(stack);
    viz_pop(stack);

    redraw(stack, "Final state", 0);
    usleep(DELAY * 3);

    LibStack.free(stack, NULL);
    printf(SHOW_CUR CLEAR);
    return EXIT_SUCCESS;
}