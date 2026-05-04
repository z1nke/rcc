#!/usr/bin/env python3

import subprocess
import sys


def run(cmd):
  subprocess.check_call(cmd)


def main():
  if len(sys.argv) != 3:
    print("usage: check_rcc_pp_run.py <source> <temp>")
    sys.exit(1)

  src = sys.argv[1]  # %s
  tmp = sys.argv[2]  # %t
  tmp_s = tmp + ".s"

  run(["rcc", "-S", "-o", tmp_s, src])
  run(["riscv64-unknown-linux-gnu-gcc", "-static", "-o", tmp, tmp_s])
  run(["qemu-riscv64", tmp])


if __name__ == "__main__":
  main()
