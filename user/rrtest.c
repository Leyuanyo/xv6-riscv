#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  char letras[3] = {'A', 'B', 'C'};

  for(int i = 0; i < 3; i++){
    int pid = fork();
    if(pid == 0){
      volatile unsigned long x = 0;
      for(int r = 0; r < 60; r++){
        for(unsigned long j = 0; j < 2000000; j++)
          x += j;                 // trabajo de CPU, sin I/O
        write(1, &letras[i], 1);  // marca: "yo tengo la CPU"
      }
      exit(0);
    }
  }
  for(int i = 0; i < 3; i++)
    wait(0);
  write(1, "\n", 1);
  exit(0);
}
