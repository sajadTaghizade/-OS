#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  int a = 5, b = 3;
  int r = simple_arithmetic_syscall(5, 3);
  printf(1, "user: simple_arith(%d,%d) returned %d\n", a, b, r);
  exit();
}
