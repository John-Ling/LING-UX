#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// implementation of very naive pattern searching using suffix arrays for learning purposes

#include "suffix_array_pattern_search.h"

// ANSI control sequences (no colour -- movement and erase only)
#define ANSI_CLEAR_SCREEN    "\033[2J"
#define ANSI_CURSOR_HOME     "\033[H"
#define ANSI_CURSOR_UP(n)    "\033[" #n "A"
#define ANSI_ERASE_LINE      "\033[2K"
#define ANSI_HIDE_CURSOR     "\033[?25l"
#define ANSI_SHOW_CURSOR     "\033[?25h"
#define ANSI_SAVE_CURSOR     "\033[s"
#define ANSI_RESTORE_CURSOR  "\033[u"

// Animation delay in microseconds
#define ANIM_DELAY_MS 800000

// ---------------------------------------------------------------------------
// Visualisation helpers
// ---------------------------------------------------------------------------

// Print a horizontal rule
static void vis_rule(int width)
{
    for (int i = 0; i < width; i++) putchar('-');
    putchar('\n');
}

// Render the suffix array as a table, highlighting a specific row.
// highlight_row == -1 means no highlight.
// lo, hi mark the current binary search window (inclusive), -1 to disable.
static void vis_draw_suffix_array(const char* string,
                                   const int suffixArray[],
                                   int n,
                                   int highlight_row,
                                   int lo,
                                   int hi,
                                   const char* pattern,
                                   int patternLength)
{
    printf("  Suffix Array  (searching for: \"%s\")\n", pattern);
    vis_rule(50);
    printf("  %-5s %-6s  %s\n", "Rank", "Idx", "Suffix");
    vis_rule(50);
    for (int i = 0; i < n; i++)
    {
        int offset = suffixArray[i];
        // Determine marker
        char marker = ' ';
        if (i == highlight_row) marker = '>';
        else if (lo != -1 && i >= lo && i <= hi) marker = '|';

        printf("%c %-4d  %-5d  ", marker, i, offset);

        // Print the suffix, marking the pattern match portion
        if (i == highlight_row) putchar('[');
        for (int j = 0; string[offset + j] != '\0'; j++)
        {
            putchar(string[offset + j]);
            if (i == highlight_row && j == patternLength - 1)
            {
                putchar(']');
            }
        }
        putchar('\n');
    }
    vis_rule(50);
}

// Show a single binary search step
static void vis_binary_step(const char* string,
                              const int suffixArray[],
                              int n,
                              int lo,
                              int hi,
                              int mid,
                              const char* pattern,
                              int patternLength,
                              int cmp_result)
{
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("=== Binary Search Step ===\n");
    printf("  lo=%d  mid=%d  hi=%d\n\n", lo, hi, mid);
    vis_draw_suffix_array(string, suffixArray, n, mid, lo, hi, pattern, patternLength);
    printf("\n  suffix[\"%s\"]  %s  pattern[\"%s\"]\n",
           string + suffixArray[mid],
           cmp_result > 0 ? ">" : cmp_result < 0 ? "<" : "==",
           pattern);
    fflush(stdout);
    usleep(1500000);
}

// Highlight a found match
static void vis_found_match(const char* string,
                             const int suffixArray[],
                             int n,
                             int rank,
                             const char* pattern,
                             int patternLength)
{
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("=== Match Found! ===\n");
    printf("  rank=%d  index=%d\n\n", rank, suffixArray[rank]);
    vis_draw_suffix_array(string, suffixArray, n, rank, -1, -1, pattern, patternLength);
    fflush(stdout);
    usleep(ANIM_DELAY_MS);
}

// Show the suffix generation step
static void vis_suffix_generation(const char* string, int n, int step)
{
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("=== Building Suffixes ===\n\n");
    vis_rule(40);
    printf("  %-5s  %s\n", "Start", "Suffix");
    vis_rule(40);
    for (int i = 0; i <= step; i++)
    {
        char marker = (i == step) ? '>' : ' ';
        printf("%c %-4d   %s\n", marker, i, string + i);
    }
    vis_rule(40);
    fflush(stdout);
    usleep(400000);
}

// Show the suffix array after sorting
static void vis_sorted_array(const char* string,
                               const int suffixArray[],
                               int n)
{
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("=== Suffix Array After Sort ===\n\n");
    vis_rule(50);
    printf("  %-5s %-6s  %s\n", "Rank", "Idx", "Suffix");
    vis_rule(50);
    for (int i = 0; i < n; i++)
    {
        printf("  %-4d  %-5d  %s\n", i, suffixArray[i], string + suffixArray[i]);
    }
    vis_rule(50);
    printf("\n  Sorted!");
    fflush(stdout);
    usleep(2000000);
}

// ---------------------------------------------------------------------------
// Original logic, with visualisation hooks
// ---------------------------------------------------------------------------

int suffix_search(char* string, const char* pattern, int suffixArray[], const int n)
{
    int upper = n - 1;
    int lower = 0;
    const int patternLength = strlen(pattern);

    while (lower <= upper)
    {
        int mid = (upper + lower) / 2;
        int suffixOffset = suffixArray[mid];
        char* suffix = string + suffixOffset;
        int result = strncmp(suffix, pattern, patternLength);

        // Visualise this binary search step
        vis_binary_step(string, suffixArray, n, lower, upper, mid, pattern, patternLength, result);

        if (result > 0)
        {
            upper = mid - 1;
        }
        else if (result < 0)
        {
            lower = mid + 1;
        }
        else
        {
            // Found -- visualise the hit then scan neighbours
            vis_found_match(string, suffixArray, n, mid, pattern, patternLength);
            printf("Found pattern at index %d\n", suffixOffset);

            int i = mid + 1;
            while (i < n)
            {
                suffixOffset = suffixArray[i];
                suffix = string + suffixOffset;
                if (strncmp(suffix, pattern, patternLength) != 0) break;
                vis_found_match(string, suffixArray, n, i, pattern, patternLength);
                printf("Found pattern at index %d\n", suffixOffset);
                i++;
            }

            i = mid - 1;
            while (i >= 0)
            {
                suffixOffset = suffixArray[i];
                suffix = string + suffixOffset;
                if (strncmp(suffix, pattern, patternLength) != 0) break;
                vis_found_match(string, suffixArray, n, i, pattern, patternLength);
                printf("Found pattern at index %d\n", suffixOffset);
                i--;
            }
            return 0;
        }
    }

    // No match found -- show final state
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("=== Search Complete: No Match Found ===\n\n");
    vis_draw_suffix_array(string, suffixArray, n, -1, -1, -1, pattern, patternLength);
    fflush(stdout);
    usleep(ANIM_DELAY_MS);

    return 0;
}

int generate_suffixes(int suffixArray[], const char* string)
{
    const int length = strlen(string);
    Suffix temp[length];

    for (int i = 0; i < length; i++)
    {
        // Visualise each suffix being added
        vis_suffix_generation(string, length, i);

        Suffix suffix;
        // +1 for null terminator
        suffix.suffix = (char*)malloc(sizeof(char) * (length - i + 1));
        if (suffix.suffix == NULL)
        {
            return 1;
        }
        suffix.startIndex = i;

        for (int j = i; j < length; j++)
        {
            suffix.suffix[j - i] = string[j];
        }
        suffix.suffix[length - i] = '\0';
        temp[i] = suffix;
    }

    qsort(temp, length, sizeof(Suffix), compare_suffixes);

    for (int i = 0; i < length; i++)
    {
        suffixArray[i] = temp[i].startIndex;
        free(temp[i].suffix);
    }

    // Visualise the sorted result
    vis_sorted_array(string, suffixArray, length);

    return 0;
}

static int compare_suffixes(const void* a, const void* b)
{
    Suffix A = *(Suffix*)a;
    Suffix B = *(Suffix*)b;
    return strcmp(A.suffix, B.suffix);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Run using ./suffix_array_pattern_search <string> <pattern>\n");
        return 1;
    }

    printf(ANSI_HIDE_CURSOR);

    // +1 for null terminator on all mallocs below
    char* strings[argc - 2];
    for (int i = 1; i < argc - 1; i++)
    {
        const int length = strlen(argv[i]);
        strings[i - 1] = (char*)malloc(sizeof(char) * (length + 1));
        strncpy(strings[i - 1], argv[i], length);
        strings[i - 1][length] = '\0';
    }

    const int patternLength = strlen(argv[argc - 1]);
    char* pattern = (char*)malloc(sizeof(char) * (patternLength + 1));
    strncpy(pattern, argv[argc - 1], patternLength);
    pattern[patternLength] = '\0';

    for (int i = 0; i < argc - 2; i++)
    {
        printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
        printf("=== String %d: \"%s\" ===\n\n", i + 1, strings[i]);
        fflush(stdout);
        usleep(ANIM_DELAY_MS / 2);

        const int length = strlen(strings[i]);
        int suffixArray[length];
        generate_suffixes(suffixArray, strings[i]);
        suffix_search(strings[i], pattern, suffixArray, length);
    }

    for (int i = 0; i < argc - 2; i++)
    {
        free(strings[i]);
    }
    free(pattern);

    printf(ANSI_SHOW_CURSOR);
    return 0;
}