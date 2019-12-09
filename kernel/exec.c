#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *pgdir, uint64 addr, struct inode *ip, uint offset, uint sz);

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0;
  struct proc *p = myproc();
  struct vma prog_vma, stack_vma, heap_vma, shadow_vma;
  uint64 prog_aslr, stack_aslr, heap_aslr, r; 

  begin_op(ROOTDEV);

  if((ip = namei(path)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;
  // do_aslr = 1;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Compute prog aslr
  if(p->aslr){
    r = random() + 0x100000; // make sure doesn't conflict w/ shadow vma
    r %= ASLR_MOD;
    prog_aslr = PGROUNDDOWN(r);
  } else {
    prog_aslr = 0;
  }

  prog_vma.base = prog_aslr;
  shadow_vma.base = 0;
  prog_vma.flags |= VMA_VALID;
  shadow_vma.flags |= VMA_VALID;
  prog_vma.sz = 0;
  shadow_vma.sz = 0;
  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(p->aslr) // shadow program
      if((shadow_vma.sz = uvmalloc(pagetable, shadow_vma.sz, ph.vaddr + ph.memsz, 0)) == 0)
        goto bad;
    if((prog_vma.sz = uvmalloc(pagetable, prog_vma.sz, ph.vaddr + ph.memsz, prog_aslr)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(p->aslr) // shadow program
      if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        goto bad;
    if(loadseg(pagetable, ph.vaddr + prog_aslr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  ip = 0;

  // Setup shadow
  if(p->aslr){
    uvmnoexec(pagetable, shadow_vma.base, shadow_vma.sz);
  }

  p = myproc();
  // uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.

  // Compute stack aslr
  if(p->aslr){
    r = random();
    r %= ASLR_MOD;
    stack_aslr = PGROUNDDOWN(r);
  } else {
    stack_aslr = 0;
  }

  // Setup stack
  stack_vma.base = prog_vma.base + PGROUNDUP(prog_vma.sz) + stack_aslr; 
  stack_vma.sz = 0;
  stack_vma.flags |= VMA_VALID;
  if((stack_vma.sz = uvmalloc(pagetable, 0, 2*PGSIZE, stack_vma.base)) == 0)
    goto bad;
  uvmclear(pagetable, stack_vma.base);
  sp = stack_vma.sz + stack_vma.base;
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->tf->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Free up memory
  proc_freepagetable(p);

  // Commit to the user image.
  // oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->tf->epc = elf.entry + prog_vma.base;  // initial program counter = main
  p->tf->sp = sp; // initial stack pointer
  // printf("epc: %p\n", p->tf->epc);
  // printf("sp: %p\n", p->tf->sp);

  // Compute heap aslr
  if(p->aslr){
    r = random();
    r %= ASLR_MOD;
    heap_aslr = PGROUNDDOWN(r);
  } else {
    heap_aslr = 0;
  }
  heap_vma.base = stack_vma.base + PGROUNDUP(stack_vma.sz) + heap_aslr;
  heap_vma.sz = 0;
  heap_vma.flags |= VMA_VALID;

  if(p->aslr){
    p->vmas[HEAP_VMA_IDX] = heap_vma; 
    p->vmas[PROG_VMA_IDX] = prog_vma;
    p->vmas[STACK_VMA_IDX] = stack_vma;
    p->vmas[SHADOW_VMA_IDX] = shadow_vma;
  } else {
    // For just 1 vma, use the heap. 
    heap_vma.sz = heap_vma.base;
    heap_vma.base = 0;
    p->vmas[HEAP_VMA_IDX] = heap_vma; 
    p->vmas[PROG_VMA_IDX].flags &= ~VMA_VALID;
    p->vmas[STACK_VMA_IDX].flags &= ~VMA_VALID;
    p->vmas[SHADOW_VMA_IDX].flags &= ~VMA_VALID;
  }
  // print
  // vmprint(pagetable);
  // printf("ASLR on. Info:\n"); 
  // printf("Heap base: %p\n", heap_vma.base);
  // printf("Prog base: %p\n", prog_vma.base);
  // printf("Stack base: %p\n", stack_vma.base);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable){
    uvmunmap(pagetable, TRAMPOLINE, PGSIZE, 0);
    uvmunmap(pagetable, TRAPFRAME, PGSIZE, 0);

    if(prog_vma.flags & VMA_VALID){
      uvmunmap(pagetable, prog_vma.base, prog_vma.sz, 1);
    }
    if(shadow_vma.flags & VMA_VALID){
      uvmunmap(pagetable, shadow_vma.base, shadow_vma.sz, 1);
    }
    if(stack_vma.flags & VMA_VALID){
      uvmunmap(pagetable, stack_vma.base, stack_vma.sz, 1);
    }
  }
  if(ip){
    iunlockput(ip);
    end_op(ROOTDEV);
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  if((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
