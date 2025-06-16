#include <stdio.h>
#include <string.h>

int exact_match(const char *filepath, const char *pattern)
{
 FILE *fp = fopen(filepath, "r");
 if (!fp)
  return 0;

 char line[1024];
 while (fgets(line, sizeof(line), fp))
 {
  if (strstr(line, pattern))
  {
   fclose(fp);
   return 1;
  }
 }
 fclose(fp);
 return 0;
}
