
clang -cc1 -triple spir vectorAddition.cl -O2 -finclude-default-header -emit-llvm-bc -o vectorAddition.bc
llvm-spirv vectorAddition.bc -o vectorAddition.spv

