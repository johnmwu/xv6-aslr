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
  uint val = (ticksc - old_ticks) + intr_count + prev_rand;
  uint square = (val) * (val);
  uint hashed = hash(square);
  uint64 mod = 0x100000000;
  prev_rand = (uint64)hashed % mod;
  return prev_rand;
}

  
