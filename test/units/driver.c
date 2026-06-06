// RUN: rcc -o %t %s
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t
// RUN: rcc -c -o %t.o %s
// RUN: riscv64-unknown-linux-gnu-objdump -f %t.o | grep -q elf64-littleriscv
// RUN: rcc -S -o %t.s %s
// RUN: grep -q 'main:' %t.s
// RUN: rcc -E %s > %t.i
// RUN: grep -q 'int main' %t.i
// RUN: rcc -E -o %t.output.i %s
// RUN: grep -q 'int main' %t.output.i
// RUN: echo foo > %t.header
// RUN: echo '#include "%t.header"' | rcc -E - | grep -q foo
// RUN: echo '#include "%t.header"' | rcc -E -o %t.stdin.i -
// RUN: grep -q foo %t.stdin.i
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
// RUN: grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files" %t.error
// RUN: ! rcc -E -o %t.multi.i %t.foo.c %t.bar.c 2> %t.error
// RUN: grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files" %t.error
// RUN: echo 'int bar(); int main() { return bar(); }' > %t.foo.c
// RUN: echo 'int bar() { return 0; }' > %t.bar.c
// RUN: rcc -o %t.multi %t.foo.c %t.bar.c
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t.multi
// RUN: mkdir -p %t.dir
// RUN: echo foo > %t.dir/i-option-test
// RUN: echo '#include "i-option-test"' | rcc -I%t.dir -E - | grep -q foo
// RUN: mkdir -p include
// RUN: echo foo > include/default-include-test
// RUN: echo '#include <default-include-test>' | rcc -E - | grep -q foo
// RUN: ! echo '#error' | rcc -E - 2> %t.error
// RUN: grep -q 'error: error' %t.error
// RUN: echo foo | rcc -Dfoo -E - | grep -q 1
// RUN: echo foo | rcc -Dfoo=bar -E - | grep -q bar
// RUN: echo foo | rcc -Dfoo=bar -Ufoo -E - | grep -q foo
// [216] Ignore -O, -W and -g and other flags
// RUN: echo > %t.empty.c
// RUN: rcc -c -O -Wall -g -std=c11 -ffreestanding -fno-builtin -fno-omit-frame-pointer -fno-stack-protector -fno-strict-aliasing -m64 -mno-red-zone -w -o /dev/null %t.empty.c
// [238] Skip UTF-8 BOM markers
// RUN: printf '\xef\xbb\xbfxyz\n' | rcc -E -o- - | grep -q '^xyz'
// RUN: echo 'inline void foo() {}' > %t.inline1.c
// RUN: echo 'inline void foo() {}' > %t.inline2.c
// RUN: echo 'int main() { return 0; }' > %t.inline3.c
// RUN: rcc -o %t.inline %t.inline1.c %t.inline2.c %t.inline3.c
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t.inline
// RUN: echo 'extern inline void foo() {}' > %t.extinline1.c
// RUN: echo 'int foo(); int main() { foo(); }' > %t.extinline2.c
// RUN: rcc -o %t.extinline %t.extinline1.c %t.extinline2.c
// RUN: qemu-riscv64 -L $(riscv64-unknown-linux-gnu-gcc -print-sysroot) %t.extinline
// RUN: rcc --help

int main() {
  return 0;
}

// CHECK: rcc [-o <output>] input-files
