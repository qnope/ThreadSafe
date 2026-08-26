#include <cstdio>
int main(){ int *p = new int(7); delete p; printf("%d\n", *p); }
