#include <cstdio>

int main()
{
  char c[]="11100111001110011100111001110011100111001110011100111001110011100111001110011100"; 
  int *a = (int*)c; 
  int temp, count = 0; 
  for(int i = 0; i<20; i++) 
  { 
    temp = *(a + i) & 0x1010101; 
    count += (temp & 0xFF)  + ((temp>>8)&0xFF)  + ((temp>>16)&0xFF)  + ((temp>>24)&0xFF); 
  }

  printf("count = %d\n", count);
}
