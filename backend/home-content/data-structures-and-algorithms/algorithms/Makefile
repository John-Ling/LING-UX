CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -I./include
DBG_CFLAGS := -g -O0 -DDEBUG
PROD_CFLAGS := -O2 -flto 
LDFLAGS := -L./lib -Wl,-rpath,'$$ORIGIN/./lib'
OBJ := $(SRC:.c=.o)

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS += $(DBG_CFLAGS)
else
	CFLAGS += $(PROD_CFLAGS)
endif

bfs_and_dfs:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o bfs_and_dfs bfs_and_dfs.c -llinkedlist -lstack -lqueue -lutils

bmh: 
	$(CC) $(CFLAGS) $(LDFLAGS)  -o bmh bmh.c

kmp:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o kmp kmp.c

quicksort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o quicksort quicksort.c

heapsort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o heapsort heapsort.c

mergesort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o mergesort mergesort.c

suffix_array_pattern_search:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o suffix_array suffix_array_pattern_search.c

naive_pattern_search:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o naive_pattern_search naive_pattern_search.c

linear_search:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o linear_search linear_search.c

binary_search:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o binary_search binary_search.c

insertion_sort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o insertion_sort insertion_sort.c

bubble_sort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o bubble_sort bubble_sort.c

bogosort:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o bogosort bogosort.c