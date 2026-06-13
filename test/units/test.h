#define ASSERT(x, y) assert(x, y, #y, __LINE__)

// [60] Add function declaration
int printf(char *fmt, ...);

// [69] Report an error on undefined/undeclared functions
void assert(int expected, int actual, char *code, int line);

// [107] Handle union initializers for global variable
int strcmp(char *p, char *q);
int memcmp(char *p, char *q, long n);

// [127] Allow to call a variadic function
int sprintf(char *buf, char *fmt, ...);

// [136] Ignore const, volatile, auto, register, restrict or _Noreturn
void exit(int n);

// [204] Allow variadic function to take more than 6 parameters
int vsprintf(char *buf, char *fmt, void *ap);

// [221] Add __DATE__ and __TIME__ macros
long strlen(char *s);

// [271] Add alloca()
void *memcpy(void *dest, void *src, long n);