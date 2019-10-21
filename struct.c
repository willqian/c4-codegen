#include <stdio.h>
#include <stdlib.h>

struct mem {
  char a;
  int b;
  int c;
} *ma;

struct mem *test(struct mem *ptr, char a, int b, int c)
{
  ptr->a = a;
  ptr->b = b;
  ptr->c = c;
  return ptr;
}

int main()
{
  ma = (struct mem *)malloc(sizeof(struct mem));
  printf("ma %p\n", ma);
  ma->a = 'h';
  ma->b = 100;
  ma->c = 300;
  printf("ma->a %c ma->b %d ma->c %d\n", ma->a, ma->b, ma->c);
  ma = test(ma, 'k', 500, 700);
  printf("ma->a %c ma->b %d ma->c %d\n", ma->a, ma->b, ma->c);
  return 0;
}
