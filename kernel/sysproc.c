#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define MAX_ISRAELI_LOCKS 15
#define MAX_QUEUE 16

struct israeli_lock {
  struct spinlock lk;
  int active;
  int locked;
  int favoritism;
  struct proc *owner; 
  
  struct proc *queue[MAX_QUEUE]; 
  int head;
  int tail;
  int wait_count;
};

struct israeli_lock ilocks[MAX_ISRAELI_LOCKS];

void israeli_init(void) {
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    initlock(&ilocks[i].lk, "israeli_lock");
    ilocks[i].active = 0;
    ilocks[i].locked = 0;
    ilocks[i].favoritism = 0;
    ilocks[i].owner = 0;
    ilocks[i].head = 0;
    ilocks[i].tail = 0;
    ilocks[i].wait_count = 0;
  }
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
struct spinlock randlock;        // Lock to protect the global state [cite: 54, 55, 66]
static uint next_random = 1;     // Global state variable for PRNG [cite: 53, 61]

// Internal implementation of srand
void
lcg_srand(uint seed)
{
  acquire(&randlock);            // Protect access to next_random [cite: 66]
  next_random = seed;            // Initialize the state with seed [cite: 55, 63]
  release(&randlock);
}

// Internal implementation of rand
uint
lcg_rand(void)
{
  uint res;
  acquire(&randlock);            // Protect access to next_random [cite: 66]
  
  // LCG formula: Xn+1 = (a * Xn + b) mod m [cite: 50, 51]
  // a = 1664525, b = 1013904223, m = 2^32 [cite: 65]
  // Arithmetic overflow in uint (32-bit) handles the "mod 2^32" automatically
  next_random = (1664525 * next_random + 1013904223);
  res = next_random;
  
  release(&randlock);
  return res;                    // Return the new pseudo-random value [cite: 55, 64]
}
// Function to be called during kernel bootup
void
lcgrandinit(void)
{
  initlock(&randlock, "randlock"); // Initialize the spinlock for the PRNG [cite: 55, 66]
}

// System call wrapper for lcg_srand
uint64
sys_lcg_srand(void)
{
  int seed;
  argint(0, &seed);              // Get the 'seed' argument from user space
  lcg_srand((uint)seed);
  return 0;
}

// System call wrapper for lcg_rand
uint64
sys_lcg_rand(void)
{
  return lcg_rand();
}

// Process Groups Syscalls
uint64 sys_setgid(void) {
  int gid;
  argint(0, &gid);
  myproc()->gid = gid;
  return 0;
}

uint64 sys_getgid(void) {
  return myproc()->gid;
}

// Lock Management Syscalls

uint64 sys_israeli_create(void) {
  int favoritism;
  
  // Call argint directly (it returns void in xv6-riscv)
  argint(0, &favoritism);
  
  if(favoritism < 0 || favoritism > 100)
    return -1;
    
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    acquire(&ilocks[i].lk);
    if(ilocks[i].active == 0) {
      ilocks[i].active = 1;
      ilocks[i].locked = 0;
      ilocks[i].favoritism = favoritism;
      ilocks[i].owner = 0;
      ilocks[i].head = 0;
      ilocks[i].tail = 0;
      ilocks[i].wait_count = 0;
      release(&ilocks[i].lk);
      return i; 
    }
    release(&ilocks[i].lk);
  }
  return -1; 
}

uint64 sys_israeli_destroy(void) {
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS)
    return -1;
    
  acquire(&ilocks[lock_id].lk);
  ilocks[lock_id].active = 0;
  release(&ilocks[lock_id].lk);
  return 0;
}

uint64 sys_israeli_acquire(void) {
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS)
    return -1;
    
  struct israeli_lock *ilock = &ilocks[lock_id];
  struct proc *p = myproc();
  
  acquire(&ilock->lk);
  if (ilock->active == 0) {
    release(&ilock->lk);
    return -1;
  }
  
  // Enter the queue
  ilock->queue[ilock->tail] = p;
  ilock->tail = (ilock->tail + 1) % MAX_QUEUE;
  ilock->wait_count++;
  release(&ilock->lk);
  
  // Wait loop (Yielding, not busy-waiting)
  while(1) {
    acquire(&ilock->lk);
    
    if (ilock->locked == 0 && ilock->queue[ilock->head] == p) {
      // Atomic locking as required
      __sync_lock_test_and_set(&ilock->locked, 1);
      ilock->owner = p;
      
      ilock->head = (ilock->head + 1) % MAX_QUEUE;
      ilock->wait_count--;
      
      __sync_synchronize(); // Memory barrier
      release(&ilock->lk);
      break;
    }
    
    release(&ilock->lk);
    yield(); 
  }
  return 0;
}

uint64 sys_israeli_release(void) {
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS)
    return -1;
    
  struct israeli_lock *ilock = &ilocks[lock_id];
  struct proc *p = myproc();
  
  acquire(&ilock->lk);
  
  if (ilock->locked == 0 || ilock->owner != p) {
    release(&ilock->lk);
    return -1; 
  }
  
  if (ilock->wait_count > 0) {
    int current_gid = p->gid;
    int found_idx = -1;
    
    // Search the queue for the earliest process with the same gid
    for(int i = 0; i < ilock->wait_count; i++) {
      int idx = (ilock->head + i) % MAX_QUEUE;
      if (ilock->queue[idx]->gid == current_gid) {
        found_idx = idx;
        break; 
      }
    }
    
    // Apply favoritism logic
    if (found_idx != -1 && (lcg_rand() % 100) < ilock->favoritism) {
      struct proc *favored_p = ilock->queue[found_idx];
      
      // Shift processes backward to move favored process to head
      int curr = found_idx;
      while(curr != ilock->head) {
        int prev = (curr - 1 + MAX_QUEUE) % MAX_QUEUE;
        ilock->queue[curr] = ilock->queue[prev];
        curr = prev;
      }
      ilock->queue[ilock->head] = favored_p;
    }
  }
  
  ilock->owner = 0;
  
  // Atomic release as required
  __sync_lock_release(&ilock->locked);
  __sync_synchronize(); 
  
  release(&ilock->lk);
  return 0;
}