#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./bin/rcc "$input" > tmp.s || exit
  riscv64-unknown-linux-gnu-gcc -static -o -static -o tmp tmp.s
  qemu-riscv64 ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 0
assert 42 42
assert 21 '5+20-4'

echo OK