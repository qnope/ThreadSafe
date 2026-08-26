#include <threadsafe/threadsafe.h>

struct TwoProblems { int *first; double *second; };

int main() { threadsafe::assert_sendable<TwoProblems>(); }
