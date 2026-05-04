// RUN: rcc -o %t %s
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t
// RUN: rcc -c -o %t.o %s
// RUN: riscv64-unknown-linux-gnu-objdump -f %t.o | grep -q elf64-littleriscv
// RUN: rcc -S -o %t.s %s
// RUN: grep -q 'main:' %t.s
// RUN: rcc -cc1 -o %t.cc1 %s
// RUN: test -f %t.cc1
// RUN: rcc -### -o %t.trace %s > /dev/null 2> %t.log
// RUN: grep -q -- -cc1 %t.log
// RUN: grep -q riscv64-unknown-linux-gnu-as %t.log
// RUN: grep -q riscv64-unknown-linux-gnu-ld %t.log
// RUN: test -f %t.trace
// RUN: cp %s %t.default.c
// RUN: cd %T && rcc -c %t.default.c
// RUN: test -f %t.default.o
// RUN: cd %T && rcc -S %t.default.c
// RUN: grep -q 'main:' %t.default.s
// RUN: cd %T && rcc %t.default.c
// RUN: test -f %T/a.out
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %T/a.out
// RUN: echo 'int foo;' > %t.foo.c
// RUN: echo 'int bar;' > %t.bar.c
// RUN: cd %T && rcc -c %t.foo.c %t.bar.c
// RUN: test -f %t.foo.o
// RUN: test -f %t.bar.o
// RUN: cd %T && rcc -S %t.foo.c %t.bar.c
// RUN: test -f %t.foo.s
// RUN: test -f %t.bar.s
// RUN: ! rcc -c -o %t.multi.o %t.foo.c %t.bar.c 2> %t.error
// RUN: grep -q "cannot specify '-o' with '-c' or '-S' with multiple files" %t.error
// RUN: echo 'int bar(); int main() { return bar(); }' > %t.foo.c
// RUN: echo 'int bar() { return 0; }' > %t.bar.c
// RUN: rcc -o %t.multi %t.foo.c %t.bar.c
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t.multi
// RUN: rcc --help

int main() {
  return 0;
}

// CHECK: rcc [-o <output>] input-files