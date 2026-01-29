// RUN: rcc -o %t %s
// RUN: test -f %t
// RUN: rcc --help

int main() {
  return 0;
}

// CHECK: rcc [-o <output>] input-files