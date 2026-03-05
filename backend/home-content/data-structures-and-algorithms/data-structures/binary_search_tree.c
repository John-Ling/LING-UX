#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "binary_search_tree.h"

/* ──────────────────────────────────────────────────────────────────────────────
 * PRIVATE FORWARD DECLARATIONS
 * ────────────────────────────────────────────────────────────────────────────── */
static int      insert_node(BSTNode** root, const int value);
static int      delete_node(BSTNode** root, const int value);
static BSTNode* find_min_node(BSTNode* root);
static int      tree_to_array(BSTNode* root, int arr[], int n, int index);
static void     inorder_traversal(BSTNode* root);


/* ══════════════════════════════════════════════════════════════════════════════
 * ANIMATED VISUALISATION
 *
 * Layout engine
 * ─────────────
 * Nodes are assigned horizontal positions by in-order index and vertical
 * positions by depth.  Edges are interpolated diagonal '/' and '\' lines
 * drawn on a shared character canvas.
 *
 * Renderer (render_frame)
 * ───────────────────────
 * Moves the cursor to the top of the screen with \033[H and redraws every
 * row without clearing, so successive frames look like smooth in-place
 * animation.  The canvas holds only edge characters.  Node values are never
 * baked into the canvas; instead they are injected with ANSI attribute codes
 * as each row is printed.  \033[K at the end of every row erases leftover
 * characters from a previous wider frame, and \033[J after the last row
 * erases any leftover rows from a taller previous frame.
 *
 * Attribute legend (monochrome only — no colour codes used anywhere)
 * ──────────────────────────────────────────────────────────────────
 *   \033[7m    reverse video  — active / currently highlighted node
 *   \033[1m    bold           — already visited / compared node
 *   \033[1;4m  bold+underline — inorder successor during two-child delete
 *   (plain)                  — unvisited node
 *
 * ANSI codes used
 * ───────────────
 *   \033[2J    clear entire screen
 *   \033[H     cursor to home (top-left of screen)
 *   \033[K     erase from cursor to end of line
 *   \033[J     erase from cursor to end of screen
 *   \033[?25l  hide cursor
 *   \033[?25h  show cursor
 *   \033[7m    reverse video on
 *   \033[1m    bold on
 *   \033[4m    underline on
 *   \033[0m    reset all attributes
 * ══════════════════════════════════════════════════════════════════════════════ */

/* ── Tuning ──────────────────────────────────────────────────────────────────── */
#define VIS_SLOT_W      6        /* terminal columns per in-order node slot       */
#define VIS_LEVEL_H     3        /* terminal rows between depth levels            */
#define VIS_MAX_NODES   512
#define VIS_CANVAS_H    160
#define VIS_CANVAS_W    512

#define ANIM_REVEAL_US  25000    /* per-row delay for the initial reveal draw     */
#define ANIM_ACTIVE_US  550000   /* time a node stays highlighted as "active"     */
#define ANIM_MOVE_US    120000   /* brief pause before moving to the next node    */
#define ANIM_RESULT_US  1200000  /* hold time on final found / not-found frame    */

/* ── ANSI escape macros ──────────────────────────────────────────────────────── */
#define A_CLEAR  "\033[2J"
#define A_HOME   "\033[H"
#define A_EOLN   "\033[K"
#define A_EOS    "\033[J"
#define A_HIDE   "\033[?25l"
#define A_SHOW   "\033[?25h"
#define A_REV    "\033[7m"      /* active node                                   */
#define A_BOLD   "\033[1m"      /* visited / compared node                       */
#define A_SUCC   "\033[1;4m"    /* inorder successor in two-child delete         */
#define A_RST    "\033[0m"


/* ══════════════════════════════════════════════════════════════════════════════
 * CANVAS
 * ══════════════════════════════════════════════════════════════════════════════ */

static char vis_canvas[VIS_CANVAS_H][VIS_CANVAS_W + 1];
static int  vis_canvas_rows;
static int  vis_canvas_cols;

static void canvas_init(void)
{
    for (int r = 0; r < vis_canvas_rows; r++)
    {
        memset(vis_canvas[r], ' ', vis_canvas_cols);
        vis_canvas[r][vis_canvas_cols] = '\0';
    }
}

static void canvas_put(int row, int col, char c)
{
    if (row >= 0 && row < vis_canvas_rows && col >= 0 && col < vis_canvas_cols)
        vis_canvas[row][col] = c;
}


/* ══════════════════════════════════════════════════════════════════════════════
 * LAYOUT ENGINE
 * ══════════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    int value;
    int x;        /* in-order position index -> horizontal canvas column */
    int depth;    /* tree depth              -> vertical canvas row       */
    int parent;   /* index in vis_nodes[], or -1 for root                */
    int is_right; /* 1 = right child of parent, 0 = left                 */
} VisNode;

static VisNode vis_nodes[VIS_MAX_NODES];
static int     vis_n;
static int     vis_io;

/*
 * vis_collect -- pre-order allocation + in-order x assignment.
 *
 * Allocating the slot pre-order (step 1) means a node's parent index is
 * always valid before we recurse into children.  Assigning the x coordinate
 * in-order (step 3) gives correct left-to-right horizontal placement.
 */
static void vis_collect(BSTNode* node, int depth, int parent_idx, int is_right)
{
    if (!node || vis_n >= VIS_MAX_NODES) return;
    int idx                  = vis_n++;           /* (1) pre-order slot      */
    vis_nodes[idx].value     = node->value;
    vis_nodes[idx].depth     = depth;
    vis_nodes[idx].parent    = parent_idx;
    vis_nodes[idx].is_right  = is_right;
    vis_collect(node->left,  depth + 1, idx, 0);  /* (2) recurse left        */
    vis_nodes[idx].x         = vis_io++;           /* (3) in-order x assign   */
    vis_collect(node->right, depth + 1, idx, 1);  /* (4) recurse right       */
}

/* Canvas coordinates for node i. */
static int vis_col(int i) { return vis_nodes[i].x     * VIS_SLOT_W + VIS_SLOT_W / 2; }
static int vis_row(int i) { return vis_nodes[i].depth * VIS_LEVEL_H; }

/* Rebuild the full layout from a live BST.  Call before any render or animation. */
static void vis_setup(BST* bst)
{
    vis_n  = 0;
    vis_io = 0;
    vis_collect(bst->root, 0, -1, 0);

    int max_depth = 0;
    for (int i = 0; i < vis_n; i++)
        if (vis_nodes[i].depth > max_depth) max_depth = vis_nodes[i].depth;

    vis_canvas_rows = max_depth * VIS_LEVEL_H + 1;
    vis_canvas_cols = vis_n * VIS_SLOT_W + VIS_SLOT_W;
    if (vis_canvas_rows > VIS_CANVAS_H) vis_canvas_rows = VIS_CANVAS_H;
    if (vis_canvas_cols > VIS_CANVAS_W) vis_canvas_cols = VIS_CANVAS_W;
}

/*
 * Draw all edges onto the canvas.  Node text is NOT placed here -- it is
 * injected with ANSI codes by the renderer so column accounting stays clean.
 *
 * Each edge is a straight diagonal line linearly interpolated across the
 * VIS_LEVEL_H rows that separate depth levels:
 *
 *     50             parent at (px, py)
 *      \
 *       \            step = 1 .. VIS_LEVEL_H-1
 *       70           child  at (cx, cy)
 */
static void vis_build_edges(void)
{
    canvas_init();
    for (int i = 0; i < vis_n; i++)
    {
        if (vis_nodes[i].parent < 0) continue;
        int pi  = vis_nodes[i].parent;
        int px  = vis_col(pi), py = vis_row(pi);
        int cx  = vis_col(i),  cy = vis_row(i);
        int dx  = cx - px,     dy = cy - py;
        char ec = vis_nodes[i].is_right ? '\\' : '/';
        for (int step = 1; step < dy; step++)
            canvas_put(py + step, px + dx * step / dy, ec);
    }
}


/* ══════════════════════════════════════════════════════════════════════════════
 * ANIMATION STATE
 * ══════════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    int  active_val;            /* shown in reverse video  (INT_MIN = none)   */
    int  successor_val;         /* shown bold+underline    (INT_MIN = none)   */
    int  visited[VIS_MAX_NODES];
    int  n_visited;
    char status[160];           /* one-line message printed below the tree    */
} AnimState;

static void anim_init(AnimState* s)
{
    s->active_val    = INT_MIN;
    s->successor_val = INT_MIN;
    s->n_visited     = 0;
    s->status[0]     = '\0';
}

static int anim_was_visited(const AnimState* s, int value)
{
    for (int i = 0; i < s->n_visited; i++)
        if (s->visited[i] == value) return 1;
    return 0;
}


/* ══════════════════════════════════════════════════════════════════════════════
 * FRAME RENDERER
 * ══════════════════════════════════════════════════════════════════════════════ */

/*
 * render_frame -- redraw the full tree in-place using ANSI_HOME.
 *
 * For every canvas row:
 *   1. Collect VisNodes whose vis_row() == r, sorted left-to-right.
 *   2. Walk left-to-right, printing plain canvas text between nodes and
 *      ANSI-wrapped node values at their positions.
 *   3. Append \033[K to erase characters left over from a wider frame.
 *
 * After all rows, \033[J erases rows left over from a taller frame.
 * The status line is printed last.
 */
static void render_frame(const AnimState* s)
{
    vis_build_edges();
    printf(A_HOME);

    for (int r = 0; r < vis_canvas_rows; r++)
    {
        /* Collect nodes on this row, sorted by column (insertion sort). */
        int ni[VIS_MAX_NODES];
        int nc[VIS_MAX_NODES];   /* canvas column of text start for each node */
        int nn = 0;

        for (int i = 0; i < vis_n; i++)
        {
            if (vis_row(i) != r) continue;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", vis_nodes[i].value);
            int col = vis_col(i) - (int)strlen(buf) / 2;

            int j  = nn++;
            ni[j]  = i;
            nc[j]  = col;
            while (j > 0 && nc[j] < nc[j - 1])
            {
                int t;
                t = ni[j]; ni[j] = ni[j-1]; ni[j-1] = t;
                t = nc[j]; nc[j] = nc[j-1]; nc[j-1] = t;
                j--;
            }
        }

        /* Print row as segments, wrapping each node's text in ANSI codes. */
        int canvas_col = 0;

        for (int k = 0; k < nn; k++)
        {
            int  idx     = ni[k];
            int  val     = vis_nodes[idx].value;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", val);
            int txt_col  = nc[k];
            int txt_len  = (int)strlen(buf);

            /* plain canvas characters up to the start of this node's text */
            for (int c = canvas_col; c < txt_col && c < vis_canvas_cols; c++)
                putchar(vis_canvas[r][c]);
            canvas_col = txt_col + txt_len;

            /* choose attribute based on state */
            if      (val == s->active_val)     printf(A_REV);
            else if (val == s->successor_val)  printf(A_SUCC);
            else if (anim_was_visited(s, val)) printf(A_BOLD);

            printf("%s" A_RST, buf);
        }

        /* remaining canvas characters on this row */
        for (int c = canvas_col; c < vis_canvas_cols; c++)
            putchar(vis_canvas[r][c]);

        printf(A_EOLN "\n");
    }

    printf(A_EOS);

    if (s->status[0])
        printf("  %s" A_EOLN "\n", s->status);
    else
        printf(A_EOLN "\n");

    fflush(stdout);
}


/* ══════════════════════════════════════════════════════════════════════════════
 * STATIC SNAPSHOT  (non-animated, row-by-row reveal)
 * ══════════════════════════════════════════════════════════════════════════════ */

void visualize_tree(BST* bst)
{
    if (!bst || bst->itemCount == 0) { puts("(empty tree)"); return; }

    vis_setup(bst);
    vis_build_edges();

    for (int i = 0; i < vis_n; i++)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", vis_nodes[i].value);
        int col = vis_col(i) - (int)strlen(buf) / 2;
        for (int k = 0; buf[k]; k++)
            canvas_put(vis_row(i), col + k, buf[k]);
    }

    printf(A_HIDE);
    for (int r = 0; r < vis_canvas_rows; r++)
    {
        int len = vis_canvas_cols;
        while (len > 0 && vis_canvas[r][len - 1] == ' ') len--;
        vis_canvas[r][len] = '\0';
        puts(vis_canvas[r]);
        fflush(stdout);
        usleep(ANIM_REVEAL_US);
    }
    printf(A_SHOW "\n");
    fflush(stdout);
}


/* ══════════════════════════════════════════════════════════════════════════════
 * ANIMATION HELPERS
 * ══════════════════════════════════════════════════════════════════════════════ */

static void anim_step(AnimState* s, int active_val,
                      const char* msg, useconds_t dur)
{
    s->active_val = active_val;
    snprintf(s->status, sizeof(s->status), "%s", msg);
    render_frame(s);
    usleep(dur);
}

/* ── In-order traversal: left -> root -> right ───────────────────────────────── */
static void inorder_anim_helper(BSTNode* node, AnimState* s)
{
    if (!node) return;

    inorder_anim_helper(node->left, s);

    char msg[128];
    snprintf(msg, sizeof(msg), "Visiting: %d", node->value);
    anim_step(s, node->value, msg, ANIM_ACTIVE_US);

    s->visited[s->n_visited++] = node->value;
    s->active_val = INT_MIN;
    snprintf(s->status, sizeof(s->status), "Visited:  %d", node->value);
    render_frame(s);
    usleep(ANIM_MOVE_US);

    inorder_anim_helper(node->right, s);
}

/* ── Search: highlight each comparison, mark compared nodes bold ─────────────── */
static int search_anim_helper(BSTNode* node, int value, AnimState* s)
{
    if (!node)
    {
        s->active_val = INT_MIN;
        snprintf(s->status, sizeof(s->status), "Not found: %d", value);
        render_frame(s);
        usleep(ANIM_RESULT_US);
        return 0;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Comparing: %d == %d ?", value, node->value);
    anim_step(s, node->value, msg, ANIM_ACTIVE_US);

    if (value == node->value)
    {
        s->visited[s->n_visited++] = node->value;
        snprintf(s->status, sizeof(s->status), "Found: %d  (*)", value);
        render_frame(s);
        usleep(ANIM_RESULT_US);
        return 1;
    }

    s->visited[s->n_visited++] = node->value;

    if (value < node->value)
    {
        snprintf(s->status, sizeof(s->status),
                 "%d < %d  ->  go left", value, node->value);
        s->active_val = INT_MIN;
        render_frame(s);
        usleep(1000000);
        return search_anim_helper(node->left, value, s);
    }
    else
    {
        snprintf(s->status, sizeof(s->status),
                 "%d > %d  ->  go right", value, node->value);
        s->active_val = INT_MIN;
        render_frame(s);
        usleep(1000000);
        return search_anim_helper(node->right, value, s);
    }
}


/* ══════════════════════════════════════════════════════════════════════════════
 * PUBLIC ANIMATION FUNCTIONS
 * ══════════════════════════════════════════════════════════════════════════════ */

void animate_inorder(BST* bst)
{
    if (!bst || bst->itemCount == 0) { puts("(empty tree)"); return; }

    vis_setup(bst);

    AnimState s;
    anim_init(&s);
    snprintf(s.status, sizeof(s.status),
             "In-order traversal  (left -> root -> right)");

    printf(A_HIDE A_CLEAR);
    render_frame(&s);
    usleep(ANIM_ACTIVE_US);

    inorder_anim_helper(bst->root, &s);

    s.active_val = INT_MIN;
    snprintf(s.status, sizeof(s.status), "Traversal complete.");
    render_frame(&s);
    usleep(ANIM_RESULT_US);

    printf(A_SHOW);
    fflush(stdout);
}

void animate_search(BST* bst, int value)
{
    if (!bst || bst->itemCount == 0) { puts("(empty tree)"); return; }

    vis_setup(bst);

    AnimState s;
    anim_init(&s);
    snprintf(s.status, sizeof(s.status), "Searching for: %d", value);

    printf(A_HIDE A_CLEAR);
    render_frame(&s);
    usleep(ANIM_ACTIVE_US);

    search_anim_helper(bst->root, value, &s);

    printf(A_SHOW);
    fflush(stdout);
}

/*
 * animate_delete -- three-phase animated deletion:
 *
 *   Phase 1  Animated search to locate the target (reuses search_anim_helper).
 *   Phase 2  If found:
 *              - highlight the target in reverse video
 *              - if it has two children, also highlight the inorder successor
 *                in bold+underline so the replacement strategy is visible
 *   Phase 3  Perform the actual deletion, re-layout, and display the result.
 */
void animate_delete(BST* bst, int value)
{
    if (!bst || bst->itemCount == 0) { puts("(empty tree)"); return; }

    vis_setup(bst);

    AnimState s;
    anim_init(&s);
    snprintf(s.status, sizeof(s.status), "Delete %d: searching...", value);

    printf(A_HIDE A_CLEAR);
    render_frame(&s);
    usleep(ANIM_ACTIVE_US);

    /* Phase 1: animated search */
    int found = search_anim_helper(bst->root, value, &s);

    if (!found)
    {
        printf(A_SHOW);
        fflush(stdout);
        return;
    }

    /* Phase 2: inspect children, display delete intent */
    BSTNode* cur = bst->root;
    while (cur && cur->value != value)
        cur = (value < cur->value) ? cur->left : cur->right;

    if (cur && cur->left && cur->right)
    {
        BSTNode* succ    = find_min_node(cur->right);
        s.active_val     = value;
        s.successor_val  = succ->value;
        snprintf(s.status, sizeof(s.status),
                 "Deleting %d  (2 children: will swap with successor %d)",
                 value, succ->value);
        render_frame(&s);
        usleep(ANIM_RESULT_US);
    }
    else
    {
        s.active_val = value;
        snprintf(s.status, sizeof(s.status), "Deleting: %d", value);
        render_frame(&s);
        usleep(ANIM_RESULT_US);
    }

    /* Phase 3: delete, re-layout, show result */
    delete(bst, value);
    anim_init(&s);

    if (bst->itemCount == 0)
    {
        printf(A_HOME A_EOS);
        printf("  Deleted %d  (tree is now empty)\n", value);
    }
    else
    {
        vis_setup(bst);
        snprintf(s.status, sizeof(s.status), "Deleted %d.  Tree updated.", value);
        render_frame(&s);
    }
    usleep(ANIM_RESULT_US);

    printf(A_SHOW);
    fflush(stdout);
}


/* ══════════════════════════════════════════════════════════════════════════════
 * CORE BST OPERATIONS
 * ══════════════════════════════════════════════════════════════════════════════ */

static void inorder_traversal(BSTNode* root)
{
    if (!root) return;
    inorder_traversal(root->left);
    printf("%d\n", root->value);
    inorder_traversal(root->right);
}

int print_tree(BST* bst)
{
    if (bst->itemCount == 0) { puts("Tree is empty"); return EXIT_FAILURE; }
    int tree[bst->itemCount];
    tree_to_array(bst->root, tree, bst->itemCount, 0);
    for (int i = 0; i < bst->itemCount; i++) printf("%d ", tree[i]);
    putchar('\n');
    int index = 0;
    while (2 * index + 2 < bst->itemCount)
    {
        printf("parent: %d  left: %d  right: %d\n",
               tree[index], tree[2 * index + 1], tree[2 * index + 2]);
        index++;
    }
    return EXIT_SUCCESS;
}

static int tree_to_array(BSTNode* root, int arr[], int n, int index)
{
    if (!root) return EXIT_SUCCESS;
    arr[index] = root->value;
    if (2 * index + 1 < n) tree_to_array(root->left,  arr, n, 2 * index + 1);
    if (2 * index + 2 < n) tree_to_array(root->right, arr, n, 2 * index + 2);
    return EXIT_SUCCESS;
}

int search_tree(BSTNode* root, const int value)
{
    if (!root) { printf("Could not find %d\n", value); return EXIT_FAILURE; }
    if (value < root->value) return search_tree(root->left,  value);
    if (value > root->value) return search_tree(root->right, value);
    printf("Found %d\n", value);
    return EXIT_SUCCESS;
}

int insert(BST* bst, const int value)
{
    if (bst->itemCount == 0)
    {
        BSTNode* node = (BSTNode*)malloc(sizeof(BSTNode));
        if (!node) return EXIT_FAILURE;
        node->value = value;
        node->left  = NULL;
        node->right = NULL;
        bst->root      = node;
        bst->itemCount = 1;
        return EXIT_SUCCESS;
    }
    int result = insert_node(&bst->root, value);
    if (result == EXIT_SUCCESS) bst->itemCount++;
    return result;
}

static int insert_node(BSTNode** root, const int value)
{
    if (*root == NULL)
    {
        BSTNode* node = (BSTNode*)malloc(sizeof(BSTNode));
        if (!node) return EXIT_FAILURE;
        node->value = value;
        node->left  = NULL;
        node->right = NULL;
        *root = node;
        return EXIT_SUCCESS;
    }
    if (value > (*root)->value) return insert_node(&(*root)->right, value);
    if (value < (*root)->value) return insert_node(&(*root)->left,  value);
    return EXIT_FAILURE;
}

int delete(BST* bst, const int value)
{
    if (bst->itemCount == 0) return EXIT_FAILURE;
    int result = delete_node(&bst->root, value);
    if (result == EXIT_SUCCESS) bst->itemCount--;
    return result;
}

static int delete_node(BSTNode** root, const int value)
{
    if (*root == NULL) return EXIT_FAILURE;
    if (value < (*root)->value) return delete_node(&(*root)->left,  value);
    if (value > (*root)->value) return delete_node(&(*root)->right, value);
    if ((*root)->left == NULL && (*root)->right == NULL)
        { free(*root); *root = NULL; }
    else if ((*root)->left == NULL)
        { BSTNode* t = (*root)->right; free(*root); *root = t; }
    else if ((*root)->right == NULL)
        { BSTNode* t = (*root)->left;  free(*root); *root = t; }
    else
    {
        BSTNode* succ  = find_min_node((*root)->right);
        (*root)->value = succ->value;
        delete_node(&(*root)->right, succ->value);
    }
    return EXIT_SUCCESS;
}

static BSTNode* find_min_node(BSTNode* root)
{
    BSTNode* cur = root;
    while (cur->left) cur = cur->left;
    return cur;
}

int free_tree(BSTNode* root)
{
    if (!root) return EXIT_SUCCESS;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
    return EXIT_SUCCESS;
}

BST* create_tree(int arr[], const int n)
{
    BST* bst = (BST*)malloc(sizeof(BST));
    if (!bst) return NULL;
    bst->itemCount = 0;
    bst->root      = NULL;
    if (!arr) return bst;
    for (int i = 0; i < n; i++) insert(bst, arr[i]);
    return bst;
}


/* ══════════════════════════════════════════════════════════════════════════════
 * DEMO  main()
 * ══════════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    BST* bst = create_tree(NULL, 0);
    insert(bst, 50);
    insert(bst, 30);
    insert(bst, 70);
    insert(bst, 20);
    insert(bst, 40);
    insert(bst, 60);
    insert(bst, 80);
    insert(bst, 10);
    insert(bst, 35);
    insert(bst, 65);

    
    animate_inorder(bst);

    usleep(1000000);
    animate_search(bst, 35);

    usleep(1000000);
    animate_search(bst, 99);

    usleep(1000000);
    animate_delete(bst, 30);

    usleep(1000000);
    animate_delete(bst, 80);

    usleep(1000000);
    animate_delete(bst, 50);
	usleep(1000000);

    free_tree(bst->root);
    free(bst);
    return EXIT_SUCCESS;
}