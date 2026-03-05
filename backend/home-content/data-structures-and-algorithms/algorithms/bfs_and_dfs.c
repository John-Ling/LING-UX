#include "bfs_and_dfs.h"
#include <unistd.h>

// ============================================================
//  ANSI escape helpers  (monochrome -- no colour codes)
// ============================================================
#define ANSI_CLEAR_SCREEN   "\033[2J\033[H"
#define ANSI_CURSOR_HOME    "\033[H"
#define ANSI_BOLD_ON        "\033[1m"
#define ANSI_BOLD_OFF       "\033[22m"
#define ANSI_REVERSE_ON     "\033[7m"   // white-on-black highlight
#define ANSI_REVERSE_OFF    "\033[27m"
#define ANSI_UNDERLINE_ON   "\033[4m"
#define ANSI_UNDERLINE_OFF  "\033[24m"
#define ANSI_RESET          "\033[0m"

// Move cursor to row r, col c (1-based)
#define ANSI_GOTO(r,c)      printf("\033[%d;%dH", (r), (c))

// ============================================================
//  Layout constants  (all rows/cols are 1-based terminal coords)
// ============================================================
#define TITLE_ROW        1
#define GRAPH_ROW        3   // top-left of graph ASCII art block
#define GRAPH_COL        2
#define STATUS_ROW       14  // "Frontier / Visited" legend
#define VIS_ROW          16  // node-state visualisation bar
#define ORDER_ROW        20  // traversal order line
#define LOG_ROW          22  // step log (single scrolling line)
#define FOOTER_ROW       24

// Column positions for the node-state bar
#define NODE_COL_START   4   // left margin for node cells
#define NODE_CELL_W      5   // chars per cell  "[nn]"

// us delay between steps (adjust to taste)
#define STEP_DELAY_US   400000   // 400 ms in microseconds

// ============================================================
//  Graph ASCII-art layout  (node pixel centres, 0-indexed)
//  We draw edges then node labels on top.
// ============================================================

/*  Rough layout for 7-node graph:

        0 --- 3
       /|    /|\
      / |   / | \
     1--+--2  |  (nothing)
     |\ | /|\ |
     | \|/ | \|
     5  4   6
      \ |  /
       \| /
        (edges only -- see get_adjacent for truth)

   We encode each node as a (row, col) in terminal coordinates
   relative to GRAPH_ROW / GRAPH_COL.
*/

static const int NODE_ROW[N] = { 0, 2, 4, 2, 6, 4, 6 };  // rel rows
static const int NODE_COL[N] = { 8, 4,10, 14, 12, 2, 18 }; // rel cols

// ============================================================
//  Forward declarations
// ============================================================
static void draw_graph_frame(int graph[N][N], int visited[N], int inFrontier[N], int current);
static void draw_node_bar(int visited[N], int inFrontier[N], int current);
static void draw_order(int order[], int orderLen);
static void draw_log(const char *msg);

// ============================================================
//  main
// ============================================================
int main(void)
{
    int graph[N][N] = {
    //   0  1  2  3  4  5  6
        {0, 1, 0, 1, 0, 0, 0}, // 0
        {1, 0, 1, 1, 0, 1, 1}, // 1
        {0, 1, 0, 1, 1, 1, 0}, // 2
        {1, 1, 1, 0, 1, 0, 0}, // 3
        {0, 0, 1, 1, 0, 0, 1}, // 4
        {0, 1, 1, 0, 0, 0, 0}, // 5
        {0, 1, 0, 0, 1, 0, 0}  // 6
    };

    printf(ANSI_CLEAR_SCREEN);

    puts("DFS");
    dfs(graph, 0);

    puts("\nPress ENTER for BFS...");
    getchar();

    printf(ANSI_CLEAR_SCREEN);

    puts("BFS");
    bfs(graph, 0);

    return EXIT_SUCCESS;
}

// ============================================================
//  DFS
// ============================================================
int dfs(int graph[N][N], int start)
{
    if (start >= N || start < 0) return EXIT_FAILURE;

    int visited[N]    = {0};
    int inFrontier[N] = {0};
    int order[N];
    int orderLen = 0;

    visited[start]    = 1;
    inFrontier[start] = 1;

    Stack *frontier = LibStack.create(NULL, 0, sizeof(int));
    LibStack.push_int(frontier, start);
    int currentNode = start;

    // --- initial draw ---
    printf(ANSI_CLEAR_SCREEN);
    ANSI_GOTO(TITLE_ROW, 1);
    printf(ANSI_BOLD_ON "  DEPTH-FIRST SEARCH  (DFS)  --  starting node: %d" ANSI_RESET, start);
    draw_graph_frame(graph, visited, inFrontier, currentNode);
    draw_node_bar(visited, inFrontier, currentNode);
    draw_order(order, orderLen);
    draw_log("Initialised -- pushing start node onto stack");
    usleep(STEP_DELAY_US);

    while (!LibStack.is_empty(frontier))
    {
        LibStack.pop(frontier, &currentNode);
        inFrontier[currentNode] = 0;

        char msg[80];
        snprintf(msg, sizeof(msg), "Popped node %d from stack", currentNode);
        draw_log(msg);
        draw_graph_frame(graph, visited, inFrontier, currentNode);
        draw_node_bar(visited, inFrontier, currentNode);
        usleep(STEP_DELAY_US / 2);

        visited[currentNode] = 1;
        order[orderLen++]    = currentNode;

        draw_order(order, orderLen);
        draw_graph_frame(graph, visited, inFrontier, currentNode);
        draw_node_bar(visited, inFrontier, currentNode);

        int adjacent[N];
        int adjacentCount = 0;
        get_adjacent(graph, adjacent, &adjacentCount, currentNode);

        for (int i = 0; i < adjacentCount; i++)
        {
            if (visited[adjacent[i]] || inFrontier[adjacent[i]]) continue;
            LibStack.push_int(frontier, adjacent[i]);
            inFrontier[adjacent[i]] = 1;

            snprintf(msg, sizeof(msg), "  Pushed neighbour %d onto stack", adjacent[i]);
            draw_log(msg);
            draw_graph_frame(graph, visited, inFrontier, currentNode);
            draw_node_bar(visited, inFrontier, currentNode);
            usleep(STEP_DELAY_US / 3);
        }
        usleep(STEP_DELAY_US);
    }

    // Final display
    draw_log("DFS complete!");
    draw_graph_frame(graph, visited, inFrontier, -1);
    draw_node_bar(visited, inFrontier, -1);
    draw_order(order, orderLen);

    ANSI_GOTO(FOOTER_ROW, 1);
    printf(ANSI_BOLD_ON "Traversal order: ");
    for (int i = 0; i < orderLen; i++)
        printf("%d%s", order[i], i < orderLen - 1 ? " -> " : "");
    printf(ANSI_RESET "\n");

    LibStack.free(frontier, free);
    return EXIT_SUCCESS;
}

// ============================================================
//  BFS
// ============================================================
int bfs(int graph[N][N], int start)
{
    if (start >= N || start < 0) return EXIT_FAILURE;

    int visited[N]    = {0};
    int inFrontier[N] = {0};
    int order[N];
    int orderLen = 0;

    visited[start]    = 1;
    inFrontier[start] = 1;

    Queue *frontier = LibQueue.create(NULL, 0, sizeof(int));
    LibQueue.enqueue_int(frontier, start);
    int currentNode = start;

    printf(ANSI_CLEAR_SCREEN);
    ANSI_GOTO(TITLE_ROW, 1);
    printf(ANSI_BOLD_ON "  BREADTH-FIRST SEARCH  (BFS)  --  starting node: %d" ANSI_RESET, start);
    draw_graph_frame(graph, visited, inFrontier, currentNode);
    draw_node_bar(visited, inFrontier, currentNode);
    draw_order(order, orderLen);
    draw_log("Initialised -- enqueuing start node");
    usleep(STEP_DELAY_US);

    while (!LibQueue.is_empty(frontier))
    {
        LibQueue.dequeue(frontier, &currentNode);
        inFrontier[currentNode] = 0;

        char msg[80];
        snprintf(msg, sizeof(msg), "Dequeued node %d from queue", currentNode);
        draw_log(msg);
        draw_graph_frame(graph, visited, inFrontier, currentNode);
        draw_node_bar(visited, inFrontier, currentNode);
        usleep(STEP_DELAY_US / 2);

        visited[currentNode] = 1;
        order[orderLen++]    = currentNode;

        draw_order(order, orderLen);
        draw_graph_frame(graph, visited, inFrontier, currentNode);
        draw_node_bar(visited, inFrontier, currentNode);

        int adjacent[N];
        int adjacentCount = 0;
        get_adjacent(graph, adjacent, &adjacentCount, currentNode);

        for (int i = 0; i < adjacentCount; i++)
        {
            if (visited[adjacent[i]] || inFrontier[adjacent[i]]) continue;
            LibQueue.enqueue_int(frontier, adjacent[i]);
            inFrontier[adjacent[i]] = 1;

            snprintf(msg, sizeof(msg), "  Enqueued neighbour %d into queue", adjacent[i]);
            draw_log(msg);
            draw_graph_frame(graph, visited, inFrontier, currentNode);
            draw_node_bar(visited, inFrontier, currentNode);
            usleep(STEP_DELAY_US / 3);
        }
        usleep(STEP_DELAY_US);
    }

    draw_log("BFS complete!");
    draw_graph_frame(graph, visited, inFrontier, -1);
    draw_node_bar(visited, inFrontier, -1);
    draw_order(order, orderLen);

    ANSI_GOTO(FOOTER_ROW, 1);
    printf(ANSI_BOLD_ON "  Traversal order: ");
    for (int i = 0; i < orderLen; i++)
        printf("%d%s", order[i], i < orderLen - 1 ? " -> " : "");
    printf(ANSI_RESET "\n");


    LibQueue.free(frontier, free);
    return EXIT_SUCCESS;
}

// ============================================================
//  get_adjacent  (unchanged logic, bounds fix applied)
// ============================================================
static int get_adjacent(int graph[N][N], int adjacent[N], int *count, int node)
{
    if (node >= N || node < 0) return EXIT_FAILURE;

    int j = 0;
    for (int i = 0; i < N; i++)
    {
        if (graph[node][i] == 1)
        {
            adjacent[j++] = i;
            (*count)++;
        }
    }
    return EXIT_SUCCESS;
}

// ============================================================
//  draw_graph_frame
//  Renders an ASCII-art graph.  Nodes are shown as:
//    [n]   normal
//    [n]   (reverse video) = current node being processed
//    (n)   = in frontier
//    {n}   = visited
// ============================================================
static void draw_graph_frame(int graph[N][N], int visited[N], int inFrontier[N], int current)
{
    // Clear the graph region (rows GRAPH_ROW .. GRAPH_ROW+10)
    for (int r = 0; r <= 10; r++)
    {
        ANSI_GOTO(GRAPH_ROW + r, GRAPH_COL);
        printf("%-60s", "");   // blank out 60 chars
    }

    // Draw edges first (so nodes paint over them)
    for (int u = 0; u < N; u++)
    {
        for (int v = u + 1; v < N; v++)
        {
            if (!graph[u][v]) continue;

            int r1 = GRAPH_ROW + NODE_ROW[u];
            int c1 = GRAPH_COL + NODE_COL[u];
            int r2 = GRAPH_ROW + NODE_ROW[v];
            int c2 = GRAPH_COL + NODE_COL[v];

            // Simple midpoint dot for edges (keeps it clean)
            int mr = (r1 + r2) / 2;
            int mc = (c1 + c2) / 2;
            ANSI_GOTO(mr, mc);
            putchar('*');

            // Draw a line of dots between the two nodes
            // Use Bresenham-ish simple step
            int dr = r2 - r1, dc = c2 - c1;
            int steps = abs(dr) > abs(dc) ? abs(dr) : abs(dc);
            if (steps == 0) continue;
            for (int s = 1; s < steps; s++)
            {
                int pr = r1 + (dr * s) / steps;
                int pc = c1 + (dc * s) / steps;
                ANSI_GOTO(pr, pc);
                putchar('.');
            }
        }
    }

    // Draw nodes on top
    for (int n = 0; n < N; n++)
    {
        int r = GRAPH_ROW + NODE_ROW[n];
        int c = GRAPH_COL + NODE_COL[n];
        ANSI_GOTO(r, c - 1);

        if (n == current)
        {
            printf(ANSI_REVERSE_ON ANSI_BOLD_ON "[%d]" ANSI_RESET, n);
        }
        else if (visited[n])
        {
            printf(ANSI_BOLD_ON "{%d}" ANSI_RESET, n);
        }
        else if (inFrontier[n])
        {
            printf("(%d)", n);
        }
        else
        {
            printf(" %d ", n);
        }
    }

    // Legend
    ANSI_GOTO(GRAPH_ROW + 9, GRAPH_COL);
    printf(ANSI_REVERSE_ON ANSI_BOLD_ON "[n]" ANSI_RESET " current  ");
    printf(ANSI_BOLD_ON "{n}" ANSI_RESET " visited  ");
    printf("(n) frontier  ");
    printf(" n  unvisited");
    fflush(stdout);
}

// ============================================================
//  draw_node_bar
//  A compact row showing every node with its current state.
// ============================================================
static void draw_node_bar(int visited[N], int inFrontier[N], int current)
{
    ANSI_GOTO(STATUS_ROW, 1);
    printf(ANSI_UNDERLINE_ON "NODE STATES" ANSI_UNDERLINE_OFF
           "                                              ");

    ANSI_GOTO(VIS_ROW, NODE_COL_START - 2);
    printf("  ");

    for (int n = 0; n < N; n++)
    {
        int col = NODE_COL_START + n * (NODE_CELL_W + 1);
        ANSI_GOTO(VIS_ROW, col);

        if (n == current)
            printf(ANSI_REVERSE_ON ANSI_BOLD_ON " [%d] " ANSI_RESET, n);
        else if (visited[n])
            printf(ANSI_BOLD_ON " {%d} " ANSI_RESET, n);
        else if (inFrontier[n])
            printf(" (%d) ", n);
        else
            printf("  %d  ", n);
    }

    // State key below the bar
    ANSI_GOTO(VIS_ROW + 2, NODE_COL_START - 2);
    printf(ANSI_REVERSE_ON "[n]" ANSI_RESET "=current  "
           ANSI_BOLD_ON "{n}" ANSI_RESET "=visited  "
           "(n)=frontier   n=unvisited      ");
    fflush(stdout);
}

// ============================================================
//  draw_order
// ============================================================
static void draw_order(int order[], int orderLen)
{
    ANSI_GOTO(ORDER_ROW, 1);
    printf(ANSI_BOLD_ON "  Order: " ANSI_RESET);
    for (int i = 0; i < orderLen; i++)
        printf("%d%s", order[i], i < orderLen - 1 ? " -> " : "");
    // Pad to clear any previous longer line
    printf("                    ");
    fflush(stdout);
}

// ============================================================
//  draw_log
// ============================================================
static void draw_log(const char *msg)
{
    ANSI_GOTO(LOG_ROW, 1);
    printf(ANSI_UNDERLINE_ON "Step" ANSI_UNDERLINE_OFF ": %-55s", msg);
    fflush(stdout);
}
