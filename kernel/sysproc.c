#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define MAX_ISRAELI_LOCKS 15
#define MAX_QUEUE 16

// Structure for the Israeli lock
struct israeli_lock {
  struct spinlock lk;   // Spinlock to protect internal lock state
  int active;           // 1 if the lock is created and valid, 0 otherwise
  int locked;           // 1 if currently acquired, 0 if available
  int favoritism;       // Favoritism coefficient (0-100)
  int owner_pid;        // PID of the process currently holding the lock
  
  // Fixed-size FIFO queue for waiting processes
  int queue[MAX_QUEUE]; // Array to store PIDs of waiting processes
  int head;             // Index of the front of the queue
  int tail;             // Index where the next process will be inserted
  int wait_count;       // Number of processes currently in the queue
};

// Global array of Israeli locks
struct israeli_lock ilocks[MAX_ISRAELI_LOCKS];

// Initialize the array of Israeli locks when the kernel starts
void israeli_init(void) {
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    initlock(&ilocks[i].lk, "israeli_lock");
    ilocks[i].active = 0;
    ilocks[i].locked = 0;
    ilocks[i].favoritism = 0;
    ilocks[i].owner_pid = 0;
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

// Set the group ID of the current process
uint64 sys_setgid(void) {
  int gid;
  
  // Retrieve the first argument
  argint(0, &gid);
  
  // Set the gid for the current process
  myproc()->gid = gid;
  
  return 0;
}

// Get the group ID of the current process
uint64 sys_getgid(void) {
  return myproc()->gid;
}