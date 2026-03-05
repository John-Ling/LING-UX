// Example — queue operations with terminal visualisation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "queues.h"
#include "include/utils.h"

/* ── ANSI escape sequences ─────────────────────────────────────────────
   Monochrome only: no colour codes used.                               */
#define A_CLEAR   "\033[2J\033[H"
#define A_BOLD    "\033[1m"
#define A_DIM     "\033[2m"
#define A_REV     "\033[7m"
#define A_RESET   "\033[0m"
#define A_HIDECUR "\033[?25l"
#define A_SHOWCUR "\033[?25h"

/* ── Timing (microseconds) ─────────────────────────────────────────── */
#define T_FAST    120000   /* 0.12 s */
#define T_MED     300000   /* 0.30 s */
#define T_SLOW    650000   /* 0.65 s */
#define T_PAUSE  1100000   /* 1.10 s */

/* ── Display state ─────────────────────────────────────────────────── */
#define MAX_VIZ 28

static int vq[MAX_VIZ];   /* mirror of queue values       */
static int vn = 0;         /* number of elements displayed */

/* ── Operation log ─────────────────────────────────────────────────── */
#define LOG_CAP 6
#define LOG_W   48

static char log_buf[LOG_CAP][LOG_W];
static int  log_len = 0;

static void log_push(const char *s)
{
    if (log_len == LOG_CAP) {
        for (int i = 0; i < LOG_CAP - 1; i++)
            memcpy(log_buf[i], log_buf[i + 1], LOG_W);
        log_len = LOG_CAP - 1;
    }
    strncpy(log_buf[log_len], s, LOG_W - 1);
    log_buf[log_len][LOG_W - 1] = '\0';
    log_len++;
}

/* ── Core renderer ─────────────────────────────────────────────────────
 *
 *  msg         – bold status line at top  (empty = suppress)
 *  hl_front    – reverse-video the FRONT element (dequeue preview)
 *  hl_back     – reverse-video the BACK element  (enqueue preview)
 *  has_pending – draw one extra dim cell at the right of the queue
 *  pend_val    – value to show in the pending cell
 *  pend_arrow  – 1 = show "-->" connecting pending to the queue
 */
static void draw(const char *msg,
                 int hl_front, int hl_back,
                 int has_pending, int pend_val, int pend_arrow)
{
    printf(A_CLEAR);

    /* status line */
    if (msg && *msg)
        printf("  " A_BOLD "%s" A_RESET "\n\n", msg);
    else
        printf("\n\n");

    int total = vn + (has_pending ? 1 : 0);

    if (total == 0) {
        printf("  " A_DIM "(empty)" A_RESET "\n");
        goto log_section;
    }

    /* ── top border ── */
    printf("  ");
    for (int i = 0; i < total; i++) {
        int p = has_pending && i == vn;
        if (p) printf(A_DIM);
        printf("+-------+");
        if (p) printf(A_RESET);
        if (i < total - 1) printf("   ");
    }
    putchar('\n');

    /* ── values ── */
    printf("  ");
    for (int i = 0; i < total; i++) {
        int p   = has_pending && i == vn;
        int val = p ? pend_val : vq[i];

        if (p)                         printf(A_DIM);
        else if (i == 0 && hl_front)   printf(A_REV);
        else if (i == vn-1 && hl_back) printf(A_REV);

        printf("| %5d |", val);
        printf(A_RESET);

        if (i < total - 1) {
            if (i == vn - 1 && has_pending)
                printf(pend_arrow ? "-->" : "   ");
            else
                printf("-->");
        }
    }
    putchar('\n');

    /* ── bottom border ── */
    printf("  ");
    for (int i = 0; i < total; i++) {
        int p = has_pending && i == vn;
        if (p) printf(A_DIM);
        printf("+-------+");
        if (p) printf(A_RESET);
        if (i < total - 1) printf("   ");
    }
    putchar('\n');

    /* ── FRONT / BACK labels (real elements only) ── */
    if (vn > 0) {
        /* total width of real cells: vn * 9 (cells) + (vn-1) * 3 (arrows) */
        int w = vn * 9 + (vn - 1) * 3;
        if (vn == 1) {
            printf("  ^ FRONT / BACK ^\n");
        } else {
            /* "^ FRONT" = 7 chars,  "BACK ^" = 6 chars */
            int pad = w - 7 - 6;
            printf("  ^ FRONT");
            for (int i = 0; i < pad; i++) putchar(' ');
            printf("BACK ^\n");
        }
    }

log_section:
    putchar('\n');

    /* dim older entries, bold the most recent */
    for (int i = 0; i < log_len; i++) {
        if (i == log_len - 1)
            printf("  " A_BOLD "> %s" A_RESET "\n", log_buf[i]);
        else
            printf("  " A_DIM  "  %s" A_RESET "\n", log_buf[i]);
    }
    putchar('\n');

    fflush(stdout);
}

/* ── Animated operations ────────────────────────────────────────────── */

static void viz_enqueue(Queue *q, int val)
{
    char msg[48], log_line[LOG_W];
    snprintf(msg,      sizeof msg,      "ENQUEUE  %d", val);
    snprintf(log_line, sizeof log_line, "ENQUEUE  %d", val);

    draw(msg, 0, 0, 1, val, 0); usleep(T_MED);   /* element floats in dim  */
    draw(msg, 0, 0, 1, val, 1); usleep(T_MED);   /* arrow snaps in         */

    LibQueue.enqueue_int(q, val);
    vq[vn++] = val;

    draw(msg, 0, 1, 0, 0, 0);   usleep(T_SLOW);  /* new back highlighted   */

    log_push(log_line);
    draw("", 0, 0, 0, 0, 0);    usleep(T_FAST);  /* settle                 */
}

static void viz_dequeue(Queue *q)
{
    if (vn == 0) {
        draw("DEQUEUE  (queue is empty)", 0, 0, 0, 0, 0);
        log_push("DEQUEUE  (queue is empty)");
        usleep(T_SLOW);
        return;
    }

    int removed = vq[0];
    char msg[48], log_line[LOG_W];
    snprintf(msg,      sizeof msg,      "DEQUEUE  ->  %d", removed);
    snprintf(log_line, sizeof log_line, "DEQUEUE  ->  %d", removed);

    draw(msg, 1, 0, 0, 0, 0);   usleep(T_MED);   /* front highlighted      */

    LibQueue.dequeue(q, NULL);
    for (int i = 0; i < vn - 1; i++) vq[i] = vq[i + 1];
    vn--;

    log_push(log_line);
    draw(msg, 0, 0, 0, 0, 0);   usleep(T_SLOW);  /* element gone           */
}

/* ── Entry ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    printf(A_HIDECUR);

    int n = (argc > 1) ? argc - 1 : 5;
    if (n > MAX_VIZ) n = MAX_VIZ;

    int arr[n];
    if (argc > 1) {
        for (int i = 0; i < n; i++)
            arr[i] = atoi(argv[i + 1]);
    } else {
        int def[] = {3, 7, 2, 9, 4};
        memcpy(arr, def, n * sizeof(int));
    }

    Queue *q = LibQueue.create(CONVERT_ARRAY(arr, n), n, sizeof(int));
    memcpy(vq, arr, n * sizeof(int));
    vn = n;

    log_push("initial state");
    draw("", 0, 0, 0, 0, 0);
    usleep(T_PAUSE);

    for (int i = 0; i < 7; i++) {
        viz_enqueue(q, i);
    }

    /* dequeue all elements (including extras to demo empty state) */
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);
    viz_dequeue(q);

    /* re-fill */
    viz_enqueue(q, 1);
    viz_enqueue(q, 1);
    viz_enqueue(q, 1);
    viz_enqueue(q, 1);

    usleep(T_PAUSE);
    printf(A_SHOWCUR A_CLEAR);

    LibQueue.free(q, NULL);
    return EXIT_SUCCESS;
}