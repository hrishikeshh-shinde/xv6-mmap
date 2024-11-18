#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "wmap.h"
// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;
extern unsigned char ref_cnt[MAX_PFN];

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  
  case T_PGFLT: //If page fault occurs   
    uint va = rcr2();
    va = PGROUNDDOWN(va);
    int index = MAX_WMMAP_INFO;
    struct proc *currproc = myproc();
    //Check if va within mapping:
    // pte_t* pte = walkpgdir(currproc->pgdir, va, 0);
    
    pte_t* pte = walkpgdir(currproc->pgdir, (const void *)va, 0);

    if(pte!=0){
      uint pa = PTE_ADDR(*pte);
      int pfn = PFN(pa);
      // cprintf("curr %s here 1 rfc %d\n", currproc->name, ref_cnt[pfn]);

      if(ref_cnt[pfn]==0){
          pte = 0;
      }
    }
    
    if(pte != 0) { 
      // cprintf("here 2\n");
      // cprintf("virtual add: %x pte: %d\n", va, *pte);

        uint pa = PTE_ADDR(*pte);
        //allowed to write
        if(*pte & PTE_COW) {
          // PAGE can be written
          int pfn = PFN(pa);
          if(ref_cnt[pfn] == 1) {
            // intit?
            *pte|=PTE_W;

          } else if(ref_cnt[pfn] > 1) {
              char *mem;
              *pte = 0;

            if((mem = kalloc()) == 0) {
              cprintf("Could not allocate memory");
            }
          
            memmove(mem, (char*)P2V(pa), PGSIZE);
            mappages(currproc->pgdir, (char*)va, PGSIZE, V2P(mem), PTE_W | PTE_U);

            ref_cnt[pfn]--;
            lcr3(V2P(currproc->pgdir));
          }
        } else {
          cprintf("Segmentation Fault\n");
          exit();
        }
    } else {
      // page not loaded
      // cprintf("here 3\n");

        for(int i=0; i<MAX_WMMAP_INFO; i++){
        if(currproc->info->startaddr[i]!=-1){
          if(va >= currproc->info->startaddr[i] && va < currproc->info->endaddr[i]){
            index = i;
            break;
          }
        }
      }
      //If within mapping allocate memory else segfault
      if(index!=MAX_WMMAP_INFO){
        char *mem;
        if((mem = kalloc()) == 0) {
          cprintf("Could not allocate memory");
        }
        else {
          memset(mem, 0, PGSIZE); //TC9 FAILED here.
          mappages(currproc->pgdir, (char *)va, PGSIZE, V2P(mem), PTE_W | PTE_U); //Check passed as void* ?
          //If file mapping, read file:
          if(!(currproc->info->flags[index] & MAP_ANONYMOUS)){
            int offset = va - currproc->info->startaddr[index]; //OFFSET
            int fd = currproc->info->fd[index];
            struct file * open_file = currproc->ofile[fd];
            setoffset(open_file, offset); //OFFSET
            fileread(open_file, (char*)va, PGSIZE);
          }
          currproc->info->n_loaded_pages[index]++;
        }
      } else {
        cprintf("Segmentation Fault\n");
        exit();
      }
    }
    break;
  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
