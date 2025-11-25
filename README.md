# rcc

A toy risc-v c11 compiler.


## Build
```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Run
```sh
./rcc 42 > hello.s
riscv64-unknown-linux-gnu-gcc -static -o hello hello.s
qemu-riscv64 ./hello
```