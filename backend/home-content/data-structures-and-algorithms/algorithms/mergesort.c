#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ------------------------------------------------------------------ //

#define MAX_HEIGHT  20
#define DELAY_US    70000

static int g_n       = 0;
static int g_max_val = 0;

// ------------------------------------------------------------------ //

// lo/hi  : active subarray window
// hi_l   : left read pointer  
// hi_r   : right read pointer >>
// hi_w   : write position     ^^
static void print_state(const int arr[],
                        int lo, int hi,
                        int hi_l, int hi_r, int hi_w)
{
    printf("\033[H");

    for (int row = MAX_HEIGHT; row >= 1; row--)
    {
        for (int col = 0; col < g_n; col++)
        {
            int bar_h = (g_max_val > 0)
                      ? (arr[col] * MAX_HEIGHT + g_max_val - 1) / g_max_val
                      : 0;

            const char *fill;
            if      (col == hi_l)                        fill = "<<";
            else if (col == hi_r)                        fill = ">>";
            else if (col == hi_w)                        fill = "^^";
            else if (lo >= 0 && (col < lo || col > hi)) fill = "~~";
            else                                         fill = "##";

            printf("%s", bar_h >= row ? fill : "  ");
            printf(" ");
        }
        printf("\033[K\n");
    }

    for (int col = 0; col < g_n; col++) printf("---");
    printf("\033[K\n");
    fflush(stdout);
}

// ------------------------------------------------------------------ //

static void merge_visual(int arr[], int lo, int mid, int hi)
{
    int left_n  = mid - lo + 1;
    int right_n = hi - mid;

    int left[left_n], right[right_n];

    for (int i = 0; i < left_n; i++)  left[i]  = arr[lo  + i];
    for (int i = 0; i < right_n; i++) right[i] = arr[mid + 1 + i];

    int l = 0, r = 0, w = lo;

    while (l < left_n && r < right_n)
    {
        // show which elements from each half are being compared
        print_state(arr, lo, hi, lo + l, mid + 1 + r, w);
        usleep(DELAY_US);

        if (left[l] <= right[r])
            arr[w++] = left[l++];
        else
            arr[w++] = right[r++];

        // show the write landing in the array
        print_state(arr, lo, hi, lo + l, mid + 1 + r, w - 1);
        usleep(DELAY_US);
    }

    while (l < left_n)
    {
        print_state(arr, lo, hi, lo + l, -1, w);
        usleep(DELAY_US);
        arr[w++] = left[l++];
        print_state(arr, lo, hi, -1, -1, w - 1);
        usleep(DELAY_US);
    }

    while (r < right_n)
    {
        print_state(arr, lo, hi, -1, mid + 1 + r, w);
        usleep(DELAY_US);
        arr[w++] = right[r++];
        print_state(arr, lo, hi, -1, -1, w - 1);
        usleep(DELAY_US);
    }
}

static void merge_sort_visual(int arr[], int lo, int hi)
{
    if (lo >= hi) return;

    // show the window being divided before recursing
    print_state(arr, lo, hi, -1, -1, -1);
    usleep(DELAY_US);

    int mid = (lo + hi) / 2;

    merge_sort_visual(arr, lo, mid);
    merge_sort_visual(arr, mid + 1, hi);

    // show the window about to be merged
    print_state(arr, lo, hi, -1, -1, -1);
    usleep(DELAY_US);

    merge_visual(arr, lo, mid, hi);
}

// ------------------------------------------------------------------ //

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        puts("Pass values to form the array");
        return EXIT_FAILURE;
    }

    g_n = argc - 1;
    int arr[g_n];

    g_max_val = 0;
    for (int i = 1; i < argc; i++)
    {
        arr[i - 1] = atoi(argv[i]);
        if (arr[i - 1] > g_max_val) g_max_val = arr[i - 1];
    }

    printf("\033[2J\033[H");
    merge_sort_visual(arr, 0, g_n - 1);

    // final frame — no active window
    print_state(arr, -1, -1, -1, -1, -1);

    puts("Sorted:");
    for (int i = 0; i < g_n; i++) printf("%d ", arr[i]);
    putchar('\n');

    return EXIT_SUCCESS;
}