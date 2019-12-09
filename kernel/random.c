#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
//#include "proc.h"
#include "defs.h"


uint64
hash(uint64 a){
  a = (a+0x000000007ed55d16) + (a<<12);
  a = (a^0x11111111c761c23c) ^ (a>>19);
  a = (a+0x22222222165667b1) + (a<<5);
  a = (a+0x33333333d3a2646c) ^ (a<<9);
  a = (a+0x44444444fd7046c5) + (a<<3);
  a = (a^0x55555555b55a4f09) ^ (a>>16);
  return a;
}

uint64
random(void){
  uint ticksc;
  acquire(&tickslock);
  ticksc = ticks;
  wakeup(&ticks);
  release(&tickslock);
  uint64 val = (ticksc - old_ticks) + intr_count + prev_rand;
  uint64 hashed = hash(val*val);
  prev_rand = hashed;
  return prev_rand;
}

  
