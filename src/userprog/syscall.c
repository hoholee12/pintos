#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//make sure only one write happens until close(readers & writers problem 1)
struct semaphore writelock;
struct semaphore mutex;
int readcount;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  sema_init(&writelock, 1);
  sema_init(&mutex, 1);
  readcount = 0;
}

//arg0
void nullify(char* tmp){ int i = 0; for(; tmp[i] != ' ' && tmp[i] != NULL; i++); tmp[i] = NULL; }

//lib/user/syscall.h lib/syscall-nr.h
void halt(){
  shutdown_power_off();
}
void exit (int status){
  //Terminates the current user program, returning status to the kernel
  struct thread* target = thread_current();

  //proj1
  char tmp[16] = {0};
  strlcpy(tmp, target->name, strlen(target->name) + 1);
  nullify(tmp);
  int i = 0;
  
  printf("%s: exit(%d)\n", tmp, status);

  //update exit code
  target->exit_code = status;

 
  thread_exit();
  //for exit code
  //printf("release on syscall exit, thread:%d exit:%d\n", target->tid, target->exit_code);
  //if(target->exitwait == NULL) target->exitwait = sema_malloc(0);
  //sema_up(target->exitwait);

  
}
pid_t exec(const char* cmd_line){
  return process_execute(cmd_line);
}
int wait(pid_t pid){
  return process_wait(pid);
}
bool create(const char* file, unsigned initial_size){
  //proj2
  return filesys_create(file, initial_size);

}
bool remove(const char* file){
  //proj2
  return filesys_remove(file);
}
int open(const char* file){
  //proj2
  struct file* fp = filesys_open(file);
  if (fp == NULL) return -1;  //failed
  return insertfd(thread_current(), fp);
}
int filesize(int fd){
  //proj2
  return file_length(fdtofp(thread_current(), fd));
}
int read(int fd, void* buffer, unsigned size){
  uint8_t* buf = buffer;
  unsigned count = size;
  int return_value = 0;

  sema_down(&mutex);
  readcount++;
  if(readcount == 1) sema_down(&writelock);
  sema_up(&mutex);


  if(fd == 0) {
    //no getbuf()
    while(count-- > 0){
      int temp = input_getc();
      if (temp != NULL)
        buf[size - count - 1] = temp;
      else
        break;
    }
    return_value = size - count;
  }
  else{
    //proj2
    return_value = file_read(fdtofp(thread_current(), fd), buffer, size);
  }

  sema_down(&mutex);
  readcount--;
  if(readcount == 0) sema_up(&writelock);
  sema_up(&mutex);


  return return_value;

}
int write(int fd, const void* buffer, unsigned size){
  int return_value = 0;
  
  sema_down(&writelock);

  //stdout
  //putbuf is in /lib/kernel/console.c
  if(fd == 1){
    putbuf(buffer, size);
    return_value = size;
  }
  else{
    //proj2
    return_value = file_write(fdtofp(thread_current(), fd), buffer, size);
  }

  sema_up(&writelock);
  return return_value;
}
void seek(int fd, unsigned position){
  //proj2
  file_seek(fdtofp(thread_current(), fd), position);
}
unsigned tell(int fd){
  //proj2
  return file_tell(fdtofp(thread_current(), fd));
}
void close(int fd){
  //proj2
  file_close(fdtofp(thread_current(), fd));
  deletefd(thread_current(), fd);
}

//custom
int fibonacci(int n){
  int arr[n + 1];
  if (n > 0) arr[0] = 0;
  if (n > 1) arr[1] = 1;
  for(int i = 2; i <= n; i++){
    arr[i] = arr[i - 1] + arr[i - 2];
  }
  return arr[n];
}
int max_of_four_int(int a, int b, int c, int d){
  int arr[4] = {a, b, c, d};
  int max = a;
  for(int i = 1; i < 4; i++){
    if(max < arr[i]) max = arr[i];
  }

  return max;
}

#include"threads/vaddr.h"

void check_arg_sanity(const void* addr){
  if(addr == NULL || is_kernel_vaddr(addr)){

    //printf("sanity check addr: %x\n", addr);
    exit(-1);
  } 
}

static void
syscall_handler (struct intr_frame *f) 
{

  
  //syscall0: pushl %[number]; int $0x30; addl $4, %%esp
  //syscall1: pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp
  //syscall2: pushl %[arg1]; pushl %[arg0]; pushl %[number]; int $0x30; addl $12, %%esp
  //syscall3: pushl %[arg2]; pushl %[arg1]; pushl %[arg0]; pushl %[number]; int $0x30; addl $16, %%esp
  uint32_t* number = (uint32_t*)f->esp;
  uint32_t* arg0 = number + 1;
  uint32_t* arg1 = arg0 + 1;
  uint32_t* arg2 = arg1 + 1;
/*
  struct thread* cur = thread_current();
  printf("inside syscall handler: thread: %d exit: %d esp: %x syscall: %d\n"
  , cur->tid, cur->exit_code, f->esp, *number);
*/
  //printf("esp on syscall.c: %x\n", f->esp);
  //printf("number: %x, arg0: %x, arg1: %x, arg2: %x\n", number, arg0, arg1, arg2);
  //printf("number: %x, arg0: %x, arg1: %x, arg2: %x\n", *number, *arg0, *arg1, *arg2);
  //hex_dump(f->esp, f->esp, PHYS_BASE - f->esp, true);

  int* arr = NULL;
  switch(*number){
  case SYS_HALT:                   /* Halt the operating system. */
    halt();
    break;
  case SYS_EXIT:                   /* Terminate this process. */
  //this needs a direct check because it calls exit right away
    check_arg_sanity(arg0);   //check offset of arg0 instead - fixes sc_bad_arg
    exit(*arg0);
    break;
  case SYS_EXEC:                   /* Start another process. */
    //The 80x86 convention for function return values is to place them in the EAX register.
    //System calls that return a value can do so by modifying the ‘eax’ member of struct intr_
    //frame.
    check_arg_sanity(*arg0);
    f->eax = exec(*arg0);
    break;
  case SYS_WAIT:                   /* Wait for a child process to die. */
    f->eax = wait(*arg0);
    break;
  case SYS_CREATE:                 /* Create a file. */
    check_arg_sanity(*arg0);
    f->eax = create(*arg0, *arg1);
    break;
  case SYS_REMOVE:                 /* Delete a file. */
    check_arg_sanity(*arg0);
    f->eax = remove(*arg0);
    break;
  case SYS_OPEN:                   /* Open a file. */
    check_arg_sanity(*arg0);
    f->eax = open(*arg0);
    break;
  case SYS_FILESIZE:               /* Obtain a file's size. */
    f->eax = filesize(*arg0);
    break;
  case SYS_READ:                   /* Read from a file. */
    check_arg_sanity(*arg1);
    f->eax = read(*arg0, *arg1, *arg2);
    break;
  case SYS_WRITE:                  /* Write to a file. */
    check_arg_sanity(*arg1);
    f->eax = write(*arg0, *arg1, *arg2);
    break;
  case SYS_SEEK:                   /* Change position in a file. */
    seek(*arg0, *arg1);
    break;
  case SYS_TELL:                   /* Report current position in a file. */
    f->eax = tell(*arg0);
    break;
  case SYS_CLOSE:                  /* Close a file. */
    close(*arg0);
    break;
  case SYS_MAXIO4:        //max of four int
    check_arg_sanity(*arg0);
    arr = (int*)*arg0;
    f->eax = max_of_four_int(arr[0], arr[1], arr[2], arr[3]);
    break;
  case SYS_FIBO:        //fibonacci
    f->eax = fibonacci(*arg0);
    break;
  }
  //printf ("system call!\n");
  //thread_exit ();
}
