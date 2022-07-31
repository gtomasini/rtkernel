
#include <conio.h>
#include <stdio.h>

int main(void)
{
  int c;
  int extended = 0;

  while (1)
  {
	  c = getch();
	  if (!c)
		extended = getch();
	  if (extended)
	  {
		printf("The character is extended %02x\n", extended);
		extended=0;
	  }
	  else
		printf("The character isn't extended %02x\n", c);
  }

  return 0;
}
