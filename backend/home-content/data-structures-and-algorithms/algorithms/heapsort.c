#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ------------------------------------------------------------------ //

#define MAX_HEIGHT  20
#define DELAY_US    60000   // 60 ms per frame

// globals used by print_state so we don't thread them through every call
static int g_max_val     = 0;
static int g_sorted_from = 0;  // elements at/above this index are fully sorted

// ------------------------------------------------------------------ //

static void swap(int *a, int *b)
{
    int tmp = *a; *a = *b; *b = tmp;
}

// hi_parent / hi_left / hi_right: indices to highlight (-1 = none)
static void print_state(const int arr[], int n,
                        int hi_parent, int hi_left, int hi_right)
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
            if      (col == hi_parent)              fill = "**";  // node being sifted
            else if (col == hi_left || col == hi_right) fill = "^^";  // children
            else if (col >= g_sorted_from)          fill = "==";  // sorted region
            else                                    fill = "##";

            printf("%s", bar_h >= row ? fill : "  ");
            printf(" ");
        }
        printf("\033[K\n");
    }

    for (int col = 0; col < n; col++)
        printf("---");
    printf("\033[K\n");

    fflush(stdout);
}

// ------------------------------------------------------------------ //

// heap_n : current heap size (shrinks during extraction)
// full_n : total array size  (needed so print_state can show sorted tail)
static void sift_down_visual(int arr[], int heap_n, int i, int full_n)
{
    int largest = i;
    int left    = 2 * i + 1;
    int right   = 2 * i + 2;

    // show the parent and its children before comparing
    print_state(arr, full_n,
                i,
                left  < heap_n ? left  : -1,
                right < heap_n ? right : -1);
    usleep(DELAY_US);

    if (left  < heap_n && arr[left]  > arr[largest]) largest = left;
    if (right < heap_n && arr[right] > arr[largest]) largest = right;

    if (largest != i)
    {
        swap(&arr[i], &arr[largest]);

        // show the swap before recursing
        print_state(arr, full_n, largest, -1, -1);
        usleep(DELAY_US);

        sift_down_visual(arr, heap_n, largest, full_n);
    }
}

static void heapify_visual(int arr[], int n)
{
    // bottom-up: start at last internal node
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down_visual(arr, n, i, n);
}

static void heapsort_visual(int arr[], int n)
{
    // compute max once for normalisation
    g_max_val = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > g_max_val) g_max_val = arr[i];

    g_sorted_from = n;   // nothing sorted yet

    printf("\033[2J\033[H");

    // ---- phase 1: build max heap ----
    heapify_visual(arr, n);

    // ---- phase 2: extract elements from heap root one by one ----
    for (int i = 0; i < n - 1; i++)
    {
        int last = n - i - 1;

        // highlight root and swap target before the swap
        print_state(arr, n, 0, last, -1);
        usleep(DELAY_US);

        swap(&arr[0], &arr[last]);
        g_sorted_from = last;   // last is now in its final position

        print_state(arr, n, last, -1, -1);
        usleep(DELAY_US);

        // heap is damaged — restore it
        sift_down_visual(arr, last, 0, n);
    }

    g_sorted_from = 0;
    print_state(arr, n, -1, -1, -1);
}

// ------------------------------------------------------------------ //

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("Pass values to form the array\n");
        return EXIT_FAILURE;
    }

    const int n = argc - 1;
    int arr[n];

    for (int i = 1; i < argc; i++)
        arr[i - 1] = atoi(argv[i]);

    heapsort_visual(arr, n);

    printf("Sorted: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");

    return EXIT_SUCCESS;
}