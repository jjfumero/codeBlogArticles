
#### How to compile and run?

```bash
export LEVEL_ZERO_ROOT=/path/to/level-zero-code 
. sources.sh
make
./gen-spirv-sh   ## Generate the SPIR-V code from the OpenCL kernel using CLANG and LLVM
./mxm
```
