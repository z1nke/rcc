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

# [1] Return value
assert 0 0
assert 42 42

# [2] Support add and sub
assert 21 '5+20-4'

# [3] Support whitespace
assert 41 ' 12 + 34 - 5 '

# [5] Support mul, div and paren
assert 47 '5+6*7'
assert 15 '5*(9-6)'
assert 17 '1-8/(2*2)+3*6'

# [6] Support unary plus and minus
assert 10 '-10+20'
assert 10 '- -10'
assert 10 '- - +10'
assert 48 '------12*+++++----++++++++++4'

echo OK