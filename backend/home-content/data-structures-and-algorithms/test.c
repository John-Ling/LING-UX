#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_WORD_LEN 100
#define MAX_WORDS 10000
#define HASH_SIZE 10007
#define MAX_NEXT_WORDS 100

// Structure to hold a word and its frequency
typedef struct NextWord {
    char word[MAX_WORD_LEN];
    int count;
} NextWord;

// Structure for hash table entries (word transitions)
typedef struct HashEntry {
    char word[MAX_WORD_LEN];
    NextWord next_words[MAX_NEXT_WORDS];
    int num_next;
    struct HashEntry* next;
} HashEntry;

// Hash table
HashEntry* hash_table[HASH_SIZE];

// Simple hash function
unsigned int hash(const char* word) {
    unsigned int hash_val = 0;
    while (*word) {
        hash_val = hash_val * 31 + *word;
        word++;
    }
    return hash_val % HASH_SIZE;
}

// Find or create hash table entry
HashEntry* find_or_create_entry(const char* word) {
    unsigned int index = hash(word);
    HashEntry* entry = hash_table[index];
    
    // Search for existing entry
    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = (HashEntry*)malloc(sizeof(HashEntry));
    strcpy(entry->word, word);
    entry->num_next = 0;
    entry->next = hash_table[index];
    hash_table[index] = entry;
    
    return entry;
}

// Add a word transition
void add_transition(const char* current_word, const char* next_word) {
    HashEntry* entry = find_or_create_entry(current_word);
    
    // Check if next_word already exists
    for (int i = 0; i < entry->num_next; i++) {
        if (strcmp(entry->next_words[i].word, next_word) == 0) {
            entry->next_words[i].count++;
            return;
        }
    }
    
    // Add new next word if there's space
    if (entry->num_next < MAX_NEXT_WORDS) {
        strcpy(entry->next_words[entry->num_next].word, next_word);
        entry->next_words[entry->num_next].count = 1;
        entry->num_next++;
    }
}

// Clean and normalize a word (remove punctuation, convert to lowercase)
void clean_word(char* word) {
    char* src = word;
    char* dst = word;
    
    while (*src) {
        if (isalnum(*src)) {
            *dst = tolower(*src);
            dst++;
        }
        src++;
    }
    *dst = '\0';
}

// Read file and build transition table
int build_markov_chain(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file '%s'\n", filename);
        return 0;
    }
    
    char current_word[MAX_WORD_LEN] = "";
    char word[MAX_WORD_LEN];
    int first_word = 1;
    
    printf("Building Markov chain from '%s'...\n", filename);
    
    while (fscanf(file, "%99s", word) == 1) {
        clean_word(word);
        
        // Skip empty words
        if (strlen(word) == 0) continue;
        
        if (!first_word) {
            add_transition(current_word, word);
        } else {
            first_word = 0;
        }
        
        strcpy(current_word, word);
    }
    
    fclose(file);
    printf("Markov chain built successfully!\n\n");
    return 1;
}

// Select next word based on probabilities
char* select_next_word(const char* current_word) {
    HashEntry* entry = find_or_create_entry(current_word);
    
    if (entry->num_next == 0) {
        return NULL;
    }
    
    // Calculate total count
    int total_count = 0;
    for (int i = 0; i < entry->num_next; i++) {
        total_count += entry->next_words[i].count;
    }
    
    // Select random word based on frequency
    int random_val = rand() % total_count;
    int cumulative = 0;
    
    for (int i = 0; i < entry->num_next; i++) {
        cumulative += entry->next_words[i].count;
        if (random_val < cumulative) {
            return entry->next_words[i].word;
        }
    }
    
    // Fallback (shouldn't reach here)
    return entry->next_words[0].word;
}

// Generate text using the Markov chain
void generate_text(int num_words, const char* start_word) {
    char current_word[MAX_WORD_LEN];
    
    // If start_word is provided, use it; otherwise pick a random word
    if (start_word && strlen(start_word) > 0) {
        strcpy(current_word, start_word);
    } else {
        // Find first non-empty entry in hash table
        for (int i = 0; i < HASH_SIZE; i++) {
            if (hash_table[i] && hash_table[i]->num_next > 0) {
                strcpy(current_word, hash_table[i]->word);
                break;
            }
        }
    }
    
    printf("Generated text:\n");
    printf("%s", current_word);
    
    for (int i = 1; i < num_words; i++) {
        char* next_word = select_next_word(current_word);
        
        if (!next_word) {
            printf("\n[No more transitions available from '%s']\n", current_word);
            break;
        }
        
        printf(" %s", next_word);
        strcpy(current_word, next_word);
        
        // Add line breaks for readability
        if (i % 15 == 0) {
            printf("\n");
        }
    }
    
    printf("\n\n");
}

// Print statistics about the Markov chain
void print_statistics() {
    int total_words = 0;
    int total_transitions = 0;
    
    for (int i = 0; i < HASH_SIZE; i++) {
        HashEntry* entry = hash_table[i];
        while (entry) {
            total_words++;
            for (int j = 0; j < entry->num_next; j++) {
                total_transitions += entry->next_words[j].count;
            }
            entry = entry->next;
        }
    }
    
    printf("Statistics:\n");
    printf("- Unique words: %d\n", total_words);
    printf("- Total transitions: %d\n", total_transitions);
    printf("- Average transitions per word: %.2f\n\n", 
           total_words > 0 ? (float)total_transitions / total_words : 0);
}

// Clean up memory
void cleanup() {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashEntry* entry = hash_table[i];
        while (entry) {
            HashEntry* temp = entry;
            entry = entry->next;
            free(temp);
        }
        hash_table[i] = NULL;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename> [num_words] [start_word]\n", argv[0]);
        printf("  filename   : Text file to analyze\n");
        printf("  num_words  : Number of words to generate (default: 50)\n");
        printf("  start_word : Starting word (optional)\n");
        return 1;
    }
    
    srand(time(NULL));
    
    const char* filename = argv[1];
    int num_words = argc > 2 ? atoi(argv[2]) : 50;
    const char* start_word = argc > 3 ? argv[3] : NULL;
    
    // Validate num_words
    if (num_words <= 0) {
        printf("Number of words must be positive\n");
        return 1;
    }
    
    // Initialize hash table
    for (int i = 0; i < HASH_SIZE; i++) {
        hash_table[i] = NULL;
    }
    
    // Build Markov chain from file
    if (!build_markov_chain(filename)) {
        return 1;
    }
    
    // Print statistics
    print_statistics();
    
    // Generate text
    generate_text(num_words, start_word);
    
    // Cleanup
    cleanup();
    
    return 0;
}