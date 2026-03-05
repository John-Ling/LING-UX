#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ------------------------------------------------------------------ //

#define MAX_HEIGHT  20
#define DELAY_US    70000   // 70 ms per frame

static int g_max_val = 0;

// ------------------------------------------------------------------ //

static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

// lo/hi : active subarray window (-1 = no active window i.e. fully sorted)
// hi_pivot, hi_i, hi_j : element highlights (-1 = none)
static void print_state(const int arr[], int n,
                        int lo, int hi,
                        int hi_pivot, int hi_i, int hi_j)
{
    printf("\033[H");

    for (int row = MAX_HEIGHT; row >= 1; row--)
    {
        for (int col = 0; col < n; col++)
        {
            int bar_h = (g_max_val > 0)
                      ? (arr[col] * MAX_HEIGHT + g_max_val - 1) / g_max_val
                      : 0;

            const char *fill;
            if      (col == hi_pivot)                       fill = "**";  // pivot
            else if (col == hi_i)                           fill = ">>";  // i pointer
            else if (col == hi_j)                           fill = "<<";  // j pointer
            else if (lo >= 0 && (col < lo || col > hi))     fill = "~~";  // outside window
            else                                            fill = "##";  // active/sorted

            printf("%s", bar_h >= row ? fill : "  ");
            printf(" ");
        }
        printf("\033[K\n");
    }

    for (int col = 0; col < n; col++) printf("---");
    printf("\033[K\n");
    fflush(stdout);
}

// ------------------------------------------------------------------ //

static int hoare_partition_visual(int arr[], int lo, int hi, int n)
{
    const int pivotValue = arr[lo];
    int i = lo - 1;
    int j = hi + 1;

    while (1)
    {
        do { i++; } while (arr[i] < pivotValue);
        do { j--; } while (arr[j] > pivotValue);

        if (i >= j) break;

        // show i and j having found their targets — about to swap
        print_state(arr, n, lo, hi, lo, i, j);
        usleep(DELAY_US);

        swap(&arr[i], &arr[j]);

        // show the result of the swap before the next scan begins
        print_state(arr, n, lo, hi, lo, i, j);
        usleep(DELAY_US);
    }

    return j;
}

static void sort_visual(int arr[], int lo, int hi, int n)
{
    if (lo >= hi) return;

    // show the subarray window before partitioning begins
    print_state(arr, n, lo, hi, lo, -1, -1);
    usleep(DELAY_US);

    int split = hoare_partition_visual(arr, lo, hi, n);

    // show the split point before recursing
    print_state(arr, n, lo, hi, split, -1, -1);
    usleep(DELAY_US);

    sort_visual(arr, lo, split, n);
    sort_visual(arr, split + 1, hi, n);
}

static void quicksort_visual(int arr[], int n)
{
    g_max_val = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > g_max_val) g_max_val = arr[i];

    printf("\033[2J\033[H");

    sort_visual(arr, 0, n - 1, n);

    // final frame — no active window, everything shown as sorted
    print_state(arr, n, -1, -1, -1, -1, -1);
}

// ------------------------------------------------------------------ //

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        puts("Pass values to form the array");
        return EXIT_FAILURE;
    }

    const int n = argc - 1;
    int arr[n];

    for (int i = 1; i < argc; i++)
        arr[i - 1] = atoi(argv[i]);

    quicksort_visual(arr, n);

    puts("Sorted:");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    putchar('\n');

    return EXIT_SUCCESS;
}