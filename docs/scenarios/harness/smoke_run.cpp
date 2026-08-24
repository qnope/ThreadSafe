#include <threadsafe/threadsafe.h>
#include <cstdio>
int main(){ threadsafe::synchronized_value<int> v{1}; auto g=v.lock(); printf("ok %d\n", *g); }
