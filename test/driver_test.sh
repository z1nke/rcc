#!/bin/bash

tmp=`mktemp -d /tmp/rcc-test-XXXXXX`
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c

check() {
  if [ $? -eq 0 ]; then
    echo "testing $1 ... passed"
  else
    echo "testing $1 ... failed"
    exit 1
  fi
}

# Testing -o
rm -f $tmp/out
./bin/rcc -o $tmp/out $tmp/empty.c
[ -f $tmp/out ]
check -o

# Testing --help
./rvcc --help 2>&1 | grep -q rcc
check --help

echo OK
