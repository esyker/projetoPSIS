#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){
  char buff[1024];
  FILE* ptr;
  ptr=fopen("board.txt","r");
  fgets(buff,1024,ptr);
  printf("buff:%s\n",buff);

}
