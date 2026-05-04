// RUN: rcc -o %t %s
// RUN: test -f %t
// RUN: riscv64-unknown-linux-gnu-objdump -f %t | grep -q elf64-littleriscv
// RUN: rcc -S -o %t.s %s
// RUN: grep -q 'main:' %t.s
// RUN: rcc -cc1 -o %t.cc1 %s
// RUN: test -f %t.cc1
// RUN: rcc -### -o %t.trace %s > /dev/null 2> %t.log
// RUN: grep -q -- -cc1 %t.log
// RUN: grep -q riscv64-unknown-linux-gnu-as %t.log
// RUN: test -f %t.trace
// RUN: cp %s %t.default.c
// RUN: cd %T && rcc %t.default.c
// RUN: test -f %t.default.o
// RUN: cd %T && rcc -S %t.default.c
// RUN: grep -q 'main:' %t.default.s
// RUN: echo 'int foo;' > %t.foo.c
// RUN: echo 'int bar;' > %t.bar.c
// RUN: cd %T && rcc %t.foo.c %t.bar.c
// RUN: test -f %t.foo.o
// RUN: test -f %t.bar.o
// RUN: cd %T && rcc -S %t.foo.c %t.bar.c
// RUN: test -f %t.foo.s
// RUN: test -f %t.bar.s
// RUN: ! rcc -o %t.multi.o %t.foo.c %t.bar.c 2> %t.error
// RUN: grep -q "cannot specify '-o' with multiple files" %t.error
// RUN: rcc --help

int main() {
  return 0;
}

// CHECK: rcc [-o <output>] input-files