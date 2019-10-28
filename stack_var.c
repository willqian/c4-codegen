#include <stdio.h>

struct small {
  char a;
};

struct big {
  char a;
  int b;
  long c;
};

int main()
{
  struct small m;
  struct big g;
  char a,b;
  char a1;
  char b1;
  char a2;
  char b2;
  int c;
  int *d;
  int e;
  long j;
  printf("hello, world %p %p %p %p %p %p %p %p %p %p %p %p\n", &a, &b, &a1, &b1, &a2, &b2, &c, &d, &e, &j, &m, &g);
  return 0;
}
