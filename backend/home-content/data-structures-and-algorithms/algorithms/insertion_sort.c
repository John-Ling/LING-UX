#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ------------------------------------------------------------------ //

#define MAX_HEIGHT  20
#define DELAY_US    80000   // 80 ms per frame

static int g_max_val     = 0;
static int g_sorted_upto = 0;  // elements left of this index are fully sorted

// ------------------------------------------------------------------ //

// hi_key   : the element being inserted
// hi_shift : the element currently being shifted right
static void print_state(const int arr[], int n, int hi_key, int hi_shift)
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
            if      (col == hi_key)              fill = "**";  // element being inserted
            else if (col == hi_shift)            fill = "<<";  // element being shifted
            else if (col < g_sorted_upto)        fill = "==";  // sorted region
            else                                 fill = "##";

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

static void insertion_sort_visual(int arr[], int n)
{
    g_max_val     = arr[0];
    g_sorted_upto = 1;   // single element is trivially sorted

    for (int i = 1; i < n; i++)
    {
        // show the element we are about to insert before touching anything
        print_state(arr, n, i, -1);
        usleep(DELAY_US);

        int j = i;
        while (j > 0 && arr[j] < arr[j - 1])
        {
            // highlight the key and the element being shifted right
            print_state(arr, n, j, j - 1);
            usleep(DELAY_US);

            int tmp    = arr[j - 1];
            arr[j - 1] = arr[j];
            arr[j]     = tmp;

            // show the result of the shift before the next comparison
            print_state(arr, n, j - 1, -1);
            usleep(DELAY_US);

            j--;
        }

        // element has landed — expand the sorted region
        g_sorted_upto = i + 1;
        print_state(arr, n, -1, -1);
        usleep(DELAY_US);
    }

    // final frame — fully sorted, no highlights
    g_sorted_upto = n;
    print_state(arr, n, -1, -1);
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
    {
        arr[i - 1] = atoi(argv[i]);
        if (arr[i - 1] > g_max_val) g_max_val = arr[i - 1];
    }

    printf("\033[2J\033[H");
    insertion_sort_visual(arr, n);

    printf("Sorted: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    return EXIT_SUCCESS;
}