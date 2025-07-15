#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_WORD 256
#define MAX_DIST 2  // allowed Levenshtein distance

// Convert string to lowercase (in-place)
void to_lower_str(char *s) {
    for (int i = 0; s[i]; i++)
        s[i] = tolower((unsigned char)s[i]);
}

// Duel-and-Sweep style Levenshtein with early abort
int bounded_levenshtein(const char *s1, const char *s2, int max_dist) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    // Early cutoff if lengths differ too much
    if (abs(len1 - len2) > max_dist) return max_dist + 1;

    int dp[MAX_WORD + 1];
    for (int j = 0; j <= len2; j++) dp[j] = j;

    for (int i = 1; i <= len1; i++) {
        int prev = dp[0];
        dp[0] = i;
        int min_in_row = dp[0];

        for (int j = 1; j <= len2; j++) {
            int tmp = dp[j];
            if (tolower(s1[i - 1]) == tolower(s2[j - 1])) {
                dp[j] = prev;
            } else {
                dp[j] = 1 + fmin(fmin(dp[j], dp[j - 1]), prev);
            }
            prev = tmp;

            if (dp[j] < min_in_row) min_in_row = dp[j];
        }

        // Duel phase: give up early if best score already too high
        if (min_in_row > max_dist) return max_dist + 1;
    }

    return dp[len2];
}

int approx_match(const char *filepath, const char *pattern) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;

    char word[MAX_WORD];
    char lower_pattern[MAX_WORD];
    strncpy(lower_pattern, pattern, MAX_WORD - 1);
    lower_pattern[MAX_WORD - 1] = '\0';
    to_lower_str(lower_pattern);  // pattern to lowercase

    while (fscanf(fp, "%255s", word) == 1) {
        to_lower_str(word);  // normalize word too
        if (bounded_levenshtein(word, lower_pattern, MAX_DIST) <= MAX_DIST) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}
