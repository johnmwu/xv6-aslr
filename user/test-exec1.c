#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main()
{
  char *argv[2];
  argv[0] = "s";
  argv[1] = (char *)0;
  exec("sh", argv);
  return 0;
}
