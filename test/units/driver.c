// RUN: rcc -o %t %s
// RUN: test -f %t
// RUN: rcc -cc1 -o %t.cc1 %s
// RUN: test -f %t.cc1
// RUN: rcc -### -o %t.trace %s 2>&1 | grep -q -- -cc1
// RUN: test -f %t.trace
// RUN: rcc --help

int main() {
  return 0;
}

// CHECK: rcc [-o <output>] input-files