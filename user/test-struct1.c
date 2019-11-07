#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

struct two_int {
  uint64 a;
  uint64 b;
};

struct two_int
main()
{
  struct two_int ti;
  ti.a = 1;
  ti.b = 256;
  return ti;
}
