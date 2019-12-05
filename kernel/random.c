#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
//#include "proc.h"
#include "defs.h"


uint hash(uint a){
  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);
  return a;
}
extern uint64 random(void){
  uint ticksc;
  acquire(&tickslock);
  ticksc = ticks;
  wakeup(&ticks);
  release(&tickslock);
  printf("ticksc is: %d\n", ticksc);
  printf("old_ticks is %d\n", old_ticks);
  printf("intr_count is %d\n", intr_count);
  uint val = (ticksc - old_ticks) + intr_count;
  uint square = (val) * (val);
  printf("val is : %d\n", val);
 
  
  //uint64 square =  (uint64)((ticksc - old_ticks) + intr_count) * (uint64)((ticksc - old_ticks) + intr_count);
  printf("square is: %d\n", square);
  //printf("final value: %d\n", square % 1000000000);
  uint hashed = hash(square);
  uint64 mod = 1000000000000000000;
  return (uint64)hashed % mod;
}

  
