#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // usleep

// ------------------------------------------------------------------ //

#define MAX_HEIGHT   20
#define BAR_WIDTH    2
#define DELAY_US     80000   // 80 ms per frame

// ------------------------------------------------------------------ //

static void print_state(const int arr[], int n, int max_val,
                        int hi_a, int hi_b)
{
    // move cursor to top-left without clearing (avoids flicker)
    printf("\033[H");

    for (int row = MAX_HEIGHT; row >= 1; row--)
    {
        for (int col = 0; col < n; col++)
        {
            int bar_h = (max_val > 0)
                      ? (arr[col] * MAX_HEIGHT + max_val - 1) / max_val
                      : 0;

            const char *fill = (col == hi_a || col == hi_b) ? "**" : "##";
            const char *empty = "  ";

            printf("%s", bar_h >= row ? fill : empty);
            printf(" ");   // gap between columns
        }
        printf("\033[K\n"); // clear rest of line
    }

    // bottom axis
    for (int col = 0; col < n; col++)
        printf("---");
    printf("\033[K\n");

    fflush(stdout);
}

// ------------------------------------------------------------------ //

static void bubble_sort_visual(int arr[], const int n)
{
    // find max for normalisation
    int max_val = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max_val) max_val = arr[i];

    // clear screen once at the start
    printf("\033[2J\033[H");

    int upper = n - 1;
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < upper; j++)
        {
            print_state(arr, n, max_val, j, j + 1);
            usleep(DELAY_US);

            if (arr[j] > arr[j + 1])
            {
                int tmp    = arr[j + 1];
                arr[j + 1] = arr[j];
                arr[j]     = tmp;

                print_state(arr, n, max_val, j, j + 1);
                usleep(DELAY_US);
            }
        }
        upper--;
    }

    // final sorted state — no highlights
    print_state(arr, n, max_val, -1, -1);
}

// ------------------------------------------------------------------ //

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("Pass numbers separated by spaces to form the array\n");
        return EXIT_FAILURE;
    }

    const int n = argc - 1;
    int arr[n];

    for (int i = 1; i < argc; i++)
        arr[i - 1] = atoi(argv[i]);

    bubble_sort_visual(arr, n);

    // print final sorted result below the chart
    printf("Sorted: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");

    return EXIT_SUCCESS;
}