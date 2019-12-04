#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int test_sh()
{
  char *argx[] = {"sh", 0};
  int pid; 
  pid = fork();
  if(pid == 0){
    
    exec("sh", argx);
  // exit(1);
  }
  else{
    wait(0);
    exit(0);
  }
  return -1;
}

int main(void)
{
  test_sh();
  return -1;
}
