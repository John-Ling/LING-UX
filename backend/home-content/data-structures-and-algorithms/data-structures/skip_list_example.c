/*
 * example.c  --  skip list terminal visualiser
 *
 * Animated insertions, searches, and deletions using ANSI escape
 * sequences only (no colour, monochrome).  Requires unistd.h for usleep.
 *
 * Do NOT modify skip_list.h, skip_list.c, linked_lists.h, or utils.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "skip_list.h"

/* ─────────────────────────────────────────────────────────────────────────
   ANSI escape sequences  (movement + style, no colour)
   ───────────────────────────────────────────────────────────────────────── */
#define ESC          "\033["
#define CLEAR        ESC "2J"
#define HOME         ESC "H"
#define HIDE_CUR     ESC "?25l"
#define SHOW_CUR     ESC "?25h"
#define BOLD         ESC "1m"
#define DIM          ESC "2m"
#define UNDERLINE    ESC "4m"
#define REVERSE      ESC "7m"
#define RESET        ESC "0m"

/* ─────────────────────────────────────────────────────────────────────────
   Timing (microseconds)
   ───────────────────────────────────────────────────────────────────────── */
#define T_FAST    80000   /*  80 ms  -- brief pause between phases        */
#define T_MED    300000   /* 300 ms  -- main frame rate during animation  */
#define T_SLOW   700000   /* 700 ms  -- linger on notable frames          */

/* ─────────────────────────────────────────────────────────────────────────
   Thin integer-array type  (used to track positions in the base layer)
   ───────────────────────────────────────────────────────────────────────── */
#define MAX_BASE 512

typedef struct { int v[MAX_BASE]; int n; } IArr;

static void ia_from_layer(LinkedList *layer, IArr *out)
{
    out->n = 0;
    if (!layer) return;
    for (ListNode *ln = layer->head; ln && out->n < MAX_BASE; ln = ln->next)
        out->v[out->n++] = *(int *)((SkipListNode *)ln->value)->value;
}

static int ia_find(const IArr *a, int val)
{
    for (int i = 0; i < a->n; i++) if (a->v[i] == val) return i;
    return -1;
}

static int in_vis(const int *vis, int vc, int val)
{
    for (int i = 0; i < vc; i++) if (vis[i] == val) return 1;
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────
   Rendering
   ───────────────────────────────────────────────────────────────────────── */

/*
 * Render a single layer row aligned to the base (layer-0) slot grid.
 *
 *   cursor_val  -- node highlighted with REVERSE (current search position).
 *                  Pass INT_MAX to disable.
 *   found_val   -- node highlighted with BOLD (search hit).
 *                  Pass INT_MAX to disable.
 *   vis / vc    -- already-visited values, shown with UNDERLINE.
 *
 * Each slot is exactly 5 chars wide: "[NNN]".
 * Gaps between layer nodes are filled with five dashes "-----".
 * Absent leading/trailing slots are left blank.
 */
static void render_row(int lnum, LinkedList *layer, const IArr *base,
                        int cursor_val, int found_val,
                        const int *vis, int vc)
{
    /* Build a presence bitmap for this layer */
    int here[MAX_BASE];
    memset(here, 0, (size_t)base->n * sizeof(int));
    if (layer) {
        for (ListNode *ln = layer->head; ln; ln = ln->next) {
            int val = *(int *)((SkipListNode *)ln->value)->value;
            int idx = ia_find(base, val);
            if (idx >= 0) here[idx] = 1;
        }
    }

    /* Skip completely empty layers */
    int any = 0;
    for (int i = 0; i < base->n; i++) if (here[i]) { any = 1; break; }
    if (!any) return;

    printf("  L%-2d  ", lnum);

    int in_span = 0;   /* true once we have printed the first node */

    for (int i = 0; i < base->n; i++) {
        int val = base->v[i];

        /* Is there a future layer-node still to come? */
        int future = 0;
        for (int j = i + 1; j < base->n; j++)
            if (here[j]) { future = 1; break; }

        if (here[i]) {
            in_span = 1;

            /* Choose display style */
            if      (val == cursor_val)          printf(REVERSE);
            else if (val == found_val)            printf(BOLD);
            else if (in_vis(vis, vc, val))        printf(UNDERLINE);
            else                                  printf(DIM);

            printf("[%3d]", val);
            printf(RESET);

        } else if (in_span && future) {
            /* Inside the layer's span -- draw a pass-through rail */
            printf("-----");
        } else {
            /* Before first node, or after last node */
            if (!future) in_span = 0;
            printf("     ");
        }
    }

    printf(" --> NULL\n");
}

/* Render every active layer, top to bottom */
static void render(SkipList *list, int cursor_val, int found_val,
                   const int *vis, int vc)
{
    printf("\n");
    if (!list || list->height == 0) { printf("  (empty)\n\n"); return; }

    IArr base;
    ia_from_layer(list->layers[0], &base);

    for (int i = list->height - 1; i >= 0; i--)
        render_row(i, list->layers[i], &base, cursor_val, found_val, vis, vc);

    printf("\n");
}

/* Clear the screen then render */
static void frame(SkipList *list, int cursor_val, int found_val,
                  const int *vis, int vc)
{
    printf(CLEAR HOME);
    render(list, cursor_val, found_val, vis, vc);
    fflush(stdout);
}

/* ─────────────────────────────────────────────────────────────────────────
   Animated insert
   ───────────────────────────────────────────────────────────────────────── */
static void anim_insert(SkipList *list, int val)
{
    /* Before: show current state */
    frame(list, INT_MAX, INT_MAX, NULL, 0);
    printf("  Inserting  " BOLD "%d" RESET "\n", val);
    fflush(stdout);
    usleep(T_MED);

    sl_insert_int(list, val);

    /* After: highlight the newly inserted value across all its layers */
    frame(list, val, INT_MAX, NULL, 0);
    printf("  Inserted   " BOLD "%d" RESET "\n", val);
    fflush(stdout);
    usleep(T_MED);
}

/* ─────────────────────────────────────────────────────────────────────────
   Animated search
   (Re-implements the skip-list traversal so each step can be visualised
    without touching the library source.)
   ───────────────────────────────────────────────────────────────────────── */
static int anim_search(SkipList *list, int target)
{
    if (!list || list->height == 0) return EXIT_FAILURE;

    int vis[MAX_BASE * _MAX_SKIP_LIST_LAYERS];
    int vc = 0;

    int       clayer = list->height - 1;
    ListNode *cur    = NULL;                        /* last accepted node  */
    ListNode *nxt    = list->layers[clayer]->head;  /* node under cursor   */
    int       result = EXIT_FAILURE;

    while (clayer >= 0) {

        /* ── render this step ── */
        int cv = nxt ? *(int *)((SkipListNode *)nxt->value)->value : INT_MAX;
        frame(list, cv, INT_MAX, vis, vc);

        if (cv == INT_MAX)
            printf("  Searching  " BOLD "%d" RESET
                   "   [L%d] reached end of layer -- dropping down\n",
                   target, clayer);
        else
            printf("  Searching  " BOLD "%d" RESET
                   "   [L%d] examining %d\n",
                   target, clayer, cv);
        fflush(stdout);
        /* ── decide what to do ── */
        if (nxt == NULL) {
            /* End of layer: drop down */
            ListNode *below = cur ? ((SkipListNode *)cur->value)->below : NULL;
            clayer--;
            if (clayer < 0) break;
            if (below) { cur = below; nxt = below->next; }
            else        { cur = NULL; nxt = list->layers[clayer]->head; }
            continue;
        }

        int nv = *(int *)((SkipListNode *)nxt->value)->value;

        if (nv == target) {
            /* Found */
            vis[vc++] = nv;
            frame(list, INT_MAX, nv, vis, vc);
            printf("  Found      " BOLD "%d" RESET "\n", target);
            fflush(stdout);
            usleep(T_SLOW);
            result = EXIT_SUCCESS;
            goto done;

        } else if (nv > target) {
            /* nxt is past target: drop down */
            ListNode *below = cur ? ((SkipListNode *)cur->value)->below : NULL;
            clayer--;
            if (clayer < 0) break;
            if (below) { cur = below; nxt = below->next; }
            else        { cur = NULL; nxt = list->layers[clayer]->head; }
            usleep(T_SLOW);

        } else {
            /* nv < target: step right */
            vis[vc++] = nv;
            cur = nxt;
            nxt = nxt->next;
            usleep(T_SLOW);
        }
    }

    /* Not found */
    frame(list, INT_MAX, INT_MAX, vis, vc);
    printf("  Not found  " BOLD "%d" RESET "\n", target);
    fflush(stdout);
    usleep(T_SLOW);

done:
    return result;
}

/* ─────────────────────────────────────────────────────────────────────────
   Manual deletion
   (sl_delete is declared in the header but not implemented in the library.
    We manipulate the layer linked-lists directly so the visualiser can
    animate it without needing the library's internals to be changed.)
   ───────────────────────────────────────────────────────────────────────── */
static void remove_from_layer(LinkedList *layer, int val)
{
    if (!layer || !layer->head) return;

    ListNode *prev = NULL, *cur = layer->head;
    while (cur) {
        if (*(int *)((SkipListNode *)cur->value)->value == val) {
            if (prev) prev->next = cur->next;
            else      layer->head = cur->next;
            free(((SkipListNode *)cur->value)->value);
            free(cur->value);
            free(cur);
            return;
        }
        prev = cur;
        cur  = cur->next;
    }
}

static void anim_delete(SkipList *list, int val)
{
    /* Show the target highlighted */
    frame(list, val, INT_MAX, NULL, 0);
    printf("  Deleting   " BOLD "%d" RESET "\n", val);
    fflush(stdout);
    usleep(T_MED);

    /* Remove from each layer top-to-bottom, animating each step */
    int orig_height = list->height;
    for (int i = orig_height - 1; i >= 0; i--) {
        if (!list->layers[i]) continue;

        /* Check if this layer contains val */
        int found = 0;
        for (ListNode *n = list->layers[i]->head; n; n = n->next)
            if (*(int *)((SkipListNode *)n->value)->value == val)
                { found = 1; break; }
        if (!found) continue;

        remove_from_layer(list->layers[i], val);

        /* Recompute height in case we emptied the top layer(s) */
        while (list->height > 0 &&
               (!list->layers[list->height - 1] ||
                list->layers[list->height - 1]->head == NULL))
            list->height--;

        frame(list, INT_MAX, INT_MAX, NULL, 0);
        printf("  Removing   " BOLD "%d" RESET "   from layer %d\n", val, i);
        fflush(stdout);
        usleep(80000);
    }

    /* Linger on the final state */
    frame(list, INT_MAX, INT_MAX, NULL, 0);
    printf("  Deleted    " BOLD "%d" RESET "\n", val);
    fflush(stdout);
    usleep(T_SLOW);
}

/* ─────────────────────────────────────────────────────────────────────────
   Library callbacks
   ───────────────────────────────────────────────────────────────────────── */
void free_skip_list_node(void *value)
{
    SkipListNode *node = (SkipListNode *)value;
    free(node->value);
    node->value = NULL;
    free(node);
}

/* ─────────────────────────────────────────────────────────────────────────
   main
   ───────────────────────────────────────────────────────────────────────── */
int main(void)
{
    printf(HIDE_CUR CLEAR HOME);

    SkipList *list = sl_create(NULL, 0, sizeof(int));

    /* ── Phase 1: animated build ─────────────────────────────────────── */
    int insert_vals[] = { 5, 2, 8, 1, 9, 3, 7, 4, 6, 0, 13, 11 };
    int n_ins = (int)(sizeof insert_vals / sizeof *insert_vals);
    for (int i = 0; i < n_ins; i++)
        anim_insert(list, insert_vals[i]);

    frame(list, INT_MAX, INT_MAX, NULL, 0);
    usleep(1000000);

    /* ── Phase 2: animated searches ──────────────────────────────────── */
    int search_vals[] = { 4, 8, 2, 1, 30, -1 };
    int n_srch = (int)(sizeof search_vals / sizeof *search_vals);
    for (int i = 0; i < n_srch; i++) {
        anim_search(list, search_vals[i]);
        usleep(T_FAST);
    }

    frame(list, INT_MAX, INT_MAX, NULL, 0);
    usleep(1000000);

    /* ── Phase 3: animated deletions ─────────────────────────────────── */
    int delete_vals[] = { 8, 1, 13 };
    int n_del = (int)(sizeof delete_vals / sizeof *delete_vals);
    for (int i = 0; i < n_del; i++) {
        anim_delete(list, delete_vals[i]);
        usleep(T_FAST);
    }

    /* ── Done ─────────────────────────────────────────────────────────── */
    frame(list, INT_MAX, INT_MAX, NULL, 0);
    usleep(1000000);

    sl_free(list, free_skip_list_node);
    return EXIT_SUCCESS;
}