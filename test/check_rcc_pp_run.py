#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path


def run(cmd):
  subprocess.check_call(cmd)


def main():
  if len(sys.argv) != 3:
    print("usage: check_rcc_pp_run.py <source> <temp>")
    sys.exit(1)

  src = sys.argv[1]  # %s
  tmp = sys.argv[2]  # %t
  tmp_s = tmp + ".s"
  tmp_o = tmp + ".o"
  common = Path(src).parent / "common"

  run(["riscv64-unknown-linux-gnu-gcc", "-c", "-xc", common, "-o", tmp_o])
  run(["rcc", f"-I{Path(src).parent}", "-S", "-o", tmp_s, src])
  run(["riscv64-unknown-linux-gnu-gcc", "-static", "-o", tmp, tmp_s, tmp_o])
  run(["qemu-riscv64", tmp])


if __name__ == "__main__":
  main()
