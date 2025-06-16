#include "matcher.h"
#include "exact_match.h"
#include "approx_match.h"

int do_search(const char *filepath, const char *pattern, int mode)
{
 return (mode == 0) ? exact_match(filepath, pattern) : approx_match(filepath, pattern);
}
