Code for blog article: https://jjfumero.github.io/posts/2021/09/timers-with-level-zero/

Example to demonstrate how to measure the GPU Kernel Execution time with Level Zero. 


Key parts:

```cpp
// Create Pool of Events and Event handler

ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

eventPoolDesc.count = poolSize;
eventPoolDesc.flags = poolFlag;

zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool);

eventDesc.index = 0;
eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
zeEventCreate(eventPool, &eventDesc, &kernelTsEvent);

// Send event as a signal for the kernel dispatch
zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatch, kernelTsEvent, 0, nullptr);

// Push the event kernel timestamp
zeCommandListAppendQueryKernelTimestamps(cmdList, 1u, &kernelTsEvent, timestampBuffer, nullptr, nullptr, 0u, nullptr);


// Read the timer, once the command queue is executed
ze_kernel_timestamp_result_t *kernelTsResults = reinterpret_cast<ze_kernel_timestamp_result_t *>(timestampBuffer);
uint64_t kernelDuration = kernelTsResults->context.kernelEnd - kernelTsResults->context.kernelStart;
...
```


#### How to compile and run?

```bash
export LEVEL_ZERO_ROOT=/path/to/level-zero-code 
export ZE_SHARED_LOADER=$LEVEL_ZERO_ROOT/build/lib/libze_loader.so
. sources.sh
make
./gen-spirv-sh   ## Generate the SPIR-V code from the OpenCL kernel using CLANG and LLVM
./mxm <size>
```


#### How to run the benchmarks

```bash
$ python3 runBenchmarks.py --run

// Query database
$ python3 runBenchmarks.py --performance

//The output format is:
// SIZE TIMER_NAME TIMER_VALUE COUNTER 

```
