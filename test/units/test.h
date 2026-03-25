#define ASSERT(x, y) assert(x, y, #y, __LINE__)

// [60] Add function declaration
int printf();

// [69] Report an error on undefined/undeclared functions
void assert(int expected, int actual, char *code, int line);