#!/usr/bin/env python3

import sys
import subprocess

def run(cmd):
  subprocess.check_call(cmd, shell=True)

def main():
  if len(sys.argv) != 4:
    print('usage: run_lit.py <source> <temp> <suite_dir>')
    sys.exit(1)

  src = sys.argv[1]      # %s
  tmp = sys.argv[2]      # %t
  suite = sys.argv[3]    # %S

  tmp_s = tmp + '.s'
  tmp2_o = tmp + '.tmp2.o'

  run(f'riscv64-unknown-linux-gnu-gcc -c -xc {suite}/common -o {tmp2_o}')
  run(f'rcc -S -o {tmp_s} {src}')
  run(f'riscv64-unknown-linux-gnu-gcc -static -o {tmp} {tmp_s} {tmp2_o}')
  #run(f'qemu-riscv64 {tmp} | FileCheck {src}')
  run(f'qemu-riscv64 {tmp}')

if __name__ == '__main__':
  main()
