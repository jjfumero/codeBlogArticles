## Vector Addition with large buffers

### How to compile and run? 

```bash
# Setup LEVEL_ZERO_ROOT to the level zero directory

$ export CPLUS_INCLUDE_PATH=$LEVEL_ZERO_ROOT/include:$CPLUS_INCLUDE_PATH
$ export LD_LIBRARY_PATH=$LEVEL_ZERO_ROOT/build/lib:$LD_LIBRARY_PATH 

$ make 

# Generate the SPIR-V for the given OpenCL C kernel 
# This needs CLANG + LLVM to SPIRV 
$ clang -cc1 -triple spir vectorAddition.cl -O2 -finclude-default-header -emit-llvm-bc -o vectorAddition.bc
$ llvm-spirv vectorAddition.bc -o vectorAddition.spv


# Run with buffer of ~6.5GB each
$ ./vectorAddition 1636870912
```