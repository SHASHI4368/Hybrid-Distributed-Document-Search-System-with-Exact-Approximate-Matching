#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int levenshtein(const char *s1, const char *s2)
{
  int m = strlen(s1), n = strlen(s2);
  int dp[m + 1][n + 1];
  for (int i = 0; i <= m; i++)
    dp[i][0] = i;
  for (int j = 0; j <= n; j++)
    dp[0][j] = j;

  for (int i = 1; i <= m; i++)
    for (int j = 1; j <= n; j++)
      dp[i][j] = (s1[i - 1] == s2[j - 1]) ? dp[i - 1][j - 1] : 1 + fmin(dp[i - 1][j], fmin(dp[i][j - 1], dp[i - 1][j - 1]));
  return dp[m][n];
}

int approx_match(const char *filepath, const char *pattern)
{
  FILE *fp = fopen(filepath, "r");
  if (!fp)
    return 0;
  char word[256];
  while (fscanf(fp, "%255s", word) == 1)
  {
    if (levenshtein(word, pattern) <= 2)
    {
      fclose(fp);
      return 1;
    }
  }
  fclose(fp);
  return 0;
}
