#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int __attribute__((noinline))
store_int(uint64 *dst, uint64 st, int offset)
{
  *(dst+offset) = st;
  return 0;
}

int
vuln_func(void)
{
  uint64 vulnbuf[2];
  memset(vulnbuf, 0, 2*8);
  store_int(vulnbuf, 0x198, 3);
  // printf("", vulnbuf[1]);
  return vulnbuf[0];
}

int
main(void)
{
  uint64 regbuf[12];
  memset(regbuf, 0, 12*8);
  regbuf[11] = 0x1ae;
  // printf("", regbuf[11]);
  vuln_func();

  exit(0);
}
