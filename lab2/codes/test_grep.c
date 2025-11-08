#include "types.h"
#include "user.h"
#include "stat.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf(1, "Usage: test_grep <filename> <keyword>\n");
    exit();
  }

  char buf[128];
  int n = grep(argv[1], argv[2], buf, sizeof(buf));

  if (n < 0)
    printf(1, "grep: not found or error\n");
  else
    printf(1, "Found: %s\n", buf);

  exit();
}
