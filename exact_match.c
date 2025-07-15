#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALPHABET_SIZE 256

typedef struct ACNode {
    struct ACNode *children[ALPHABET_SIZE];
    struct ACNode *fail;
    int is_end;
} ACNode;

// Convert to lowercase for case-insensitive matching
char to_lower(char c) {
    return (char)tolower((unsigned char)c);
}

// Create a new node
ACNode* ac_create_node() {
    ACNode *node = (ACNode *)calloc(1, sizeof(ACNode));
    return node;
}

// Build automaton from a single pattern
void ac_build_trie(ACNode *root, const char *pattern) {
    ACNode *node = root;
    for (int i = 0; pattern[i]; ++i) {
        unsigned char c = (unsigned char)to_lower(pattern[i]);
        if (!node->children[c]) {
            node->children[c] = ac_create_node();
        }
        node = node->children[c];
    }
    node->is_end = 1;
}

// Build failure links (trivial for single pattern)
void ac_build_failures(ACNode *root) {
    root->fail = NULL;
    for (int c = 0; c < ALPHABET_SIZE; ++c) {
        if (root->children[c]) {
            root->children[c]->fail = root;
        }
    }
}

// Search a line using the automaton
int ac_search_line(ACNode *root, const char *line) {
    ACNode *node = root;
    for (int i = 0; line[i]; ++i) {
        unsigned char c = (unsigned char)to_lower(line[i]);

        while (node && !node->children[c])
            node = node->fail;

        if (!node)
            node = root;
        else
            node = node->children[c];

        if (node && node->is_end)
            return 1;
    }
    return 0;
}

// Free memory
void ac_free(ACNode *node) {
    for (int c = 0; c < ALPHABET_SIZE; ++c) {
        if (node->children[c])
            ac_free(node->children[c]);
    }
    free(node);
}

// Final exact match function with Aho-Corasick and case-insensitivity
int exact_match(const char *filepath, const char *pattern) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;

    ACNode *root = ac_create_node();
    ac_build_trie(root, pattern);
    ac_build_failures(root);

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (ac_search_line(root, line)) {
            found = 1;
            break;
        }
    }

    fclose(fp);
    ac_free(root);
    return found;
}
