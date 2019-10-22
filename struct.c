#include <stdio.h>
#include <stdlib.h>

struct empty {

} et;

struct mem {
  char a;
  int b;
  int c;
} *ma, mc;

int ga;

struct mem *test(struct mem *ptr, char a, int b, int c)
{
  int k;
  ptr->a = a;
  ptr->b = b;
  ptr->c = c;
  return ptr;
}

struct mem *test1(struct mem obj)
{
  int i;
  ma->a = obj.a;
  ma->b = obj.b;
  ma->c = obj.c;
  printf("obj.a %c, obj.b %d, obj.c %d\n", obj.a, obj.b, obj.c);
}

int main()
{
  struct mem *mb;
  int bb;
  struct mem md;
  int cc;
  char *str;
  int i;
  str = (char *)malloc(sizeof(char) * 100);
  mb = (struct mem *)malloc(sizeof(struct mem));
  mb->a = 'u';
  mb->b = 5;
  mb->c = 100;
  printf("mb->a %c mb->b %d mb->c %d\n", mb->a, mb->b, mb->c);
  mc.a = 'f';
  mc.b = 125;
  mc.c = 321;
  printf("mc.a %c mc.b %d mc.c %d\n", mc.a, mc.b, mc.c);
  ga = 555;
  printf("mc.a %c mc.b %d mc.c %d\n", mc.a, mc.b, mc.c);
  printf("ga %d\n", ga);
  md.a = 'f';
  md.b = 222;
  md.c = 78;
  printf("md.a %c, md.b %d, md.c %d\n", md.a, md.b, md.c);
  printf("md.a %c md.b %d md.c %d\n", md.a, md.b, md.c);
  bb = 345;
  cc = 543;
  printf("md.a %c md.b %d md.c %d\n", md.a, md.b, md.c);
  printf("&md.a %p &md.b %p &md.c %p\n", &md.a, &md.b, &md.c);
  printf("&bb %p\n", &bb);
  printf("&md %p\n", &md);
  printf("&cc %p\n", &cc);
  printf("sizeof(et) %d\n", sizeof(et));
  i = 0;
  memset(str, 'c', 3);
  memset(str, 'b', 2);
  memset(str, 'a', 1);
  printf("str[0] %c str[1] %c str[2] %c\n", str[0], str[1], str[2]);
  ma = (struct mem *)malloc(sizeof(struct mem));
  test1(md);
  printf("md.a %c, md.b %d, md.c %d\n", md.a, md.b, md.c);
  printf("ma->a %c, ma->b %d, ma->c %d\n", ma->a, ma->b, ma->c);
  printf("ma %p &ma->a %p, &ma->b %p, &ma->c %p\n", ma, &ma->a, &ma->b, &ma->c);
  printf("sizeof(struct mem *) %lu\n", sizeof(struct mem *)); 
  printf("sizeof(ma) %lu\n", sizeof(ma));
  printf("sizeof(ma->a) %lu\n", sizeof(ma->a));
  printf("sizeof((struct mem *)ma) %lu\n", sizeof((struct mem *)ma));
  // printf("sizeof((struct mem *)*ma) %lu\n", sizeof((struct mem *)*ma)); // PASS cast error
  printf("sizeof((struct mem *)(ma)) %lu\n", sizeof((struct mem *)(ma)));
  printf("sizeof((struct mem **)(&ma)) %lu\n", sizeof((struct mem **)(&ma)));
  printf("sizeof((struct mem **)&ma) %lu\n", sizeof((struct mem **)&ma));
  printf("sizeof((struct mem **)(*&*&ma)) %lu\n", sizeof((struct mem **)(*&*&ma)));
  // printf("sizeof((struct mem *)**&ma) %lu\n", sizeof((struct mem *)**&ma)); // PASS cast error
  printf("sizeof((struct mem **)(&&ma)) %lu\n", sizeof((struct mem ***)(&ma)));
  printf("sizeof((*ma)) %lu\n", sizeof((*ma)));
  printf("sizeof(int) %d sizeof(char) %d sizeof(int *) %d sizeof(char *) %d\n", sizeof(int), sizeof(char), sizeof(int *), sizeof(char *));
  // printf("sizeof((int *)ma->a) %d\n", sizeof((int *)ma->a)); // PASS cast error
  printf("sizeof((int *)ma->a) %d sizeof((struct mem *)ma->b) %d\n", sizeof((int *)ma->c), sizeof((struct mem *)ma->b));
  printf("sizeof(str[0]) %d\n", sizeof(str[0]));
  ma->a = 'h';
  ma->b = 100;
  ma->c = 300;
  printf("ma->a %c ma->b %d ma->c %d\n", ma->a, ma->b, ma->c);
  ma = test(ma, 'k', 500, 700);
  printf("ma->a %c ma->b %d ma->c %d\n", ma->a, ma->b, ma->c);
  // printf("sizeof(ma->a++) %lu ma->a %c\n", sizeof(ma->a++), ma->a); FAIL
  printf("(&ma[0])->a %c\n", (&ma[0])->a);
  printf("ma[0].a %c sizeof(ma[0].a) %lu\n", ma[0].a, sizeof(ma[0].a));
  // printf("&ma[0]->a %c\n", &ma[0]->a); // PASS struct -> a error
  printf("&ma[0]->a %c\n", (&ma[0])->a);
  // printf("sizeof(ma[0].a++) %lu ma[0].a %c\n", sizeof(ma[0].a++), ma[0].a); FAIL
  // printf("sizeof(++ma[0].a) %lu ma[0].a %c\n", sizeof(++ma[0].a), ma[0].a); FAIL
  // printf("sizeof(str[i++]) %lu str[i] %c\n", sizeof(str[i++]), str[i]); FAIL
  // printf("sizeof(sizeof(1)) %lu\n", sizeof(sizeof(1))); FAIL
  return 0;
}
