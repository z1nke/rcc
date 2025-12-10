#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./bin/rcc "$input" > tmp.s || exit
  riscv64-unknown-linux-gnu-gcc -static -o tmp tmp.s
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
assert 0 '0;'
assert 42 '42;'

# [2] Support add and sub
assert 21 '5+20-4;'

# [3] Support whitespace
assert 41 ' 12 + 34 - 5 ;'

# [5] Support mul, div and paren
assert 47 '5+6*7;'
assert 15 '5*(9-6);'
assert 17 '1-8/(2*2)+3*6;'

# [6] Support unary plus and minus
assert 10 '-10+20;'
assert 10 '- -10;'
assert 10 '- - +10;'
assert 48 '------12*+++++----++++++++++4;'

# [7] Support compare operator
assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'
assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'
assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'
assert 1 '5==2+3;'
assert 0 '6==4+3;'
assert 1 '0*9+5*2==4+4*(6/3)-2;'

# [9] Accept multiple statements
assert 3 '1; 2; 3;'
assert 12 '12+23;12+99/3;78-66;'

# [10] Single-letter local variable.
assert 3 'a=3; a;'
assert 8 'a=3; z=5; a+z;'
assert 6 'a=b=3; a+b;'
assert 5 'a=3;b=4;a=1;a+b;'

echo OK