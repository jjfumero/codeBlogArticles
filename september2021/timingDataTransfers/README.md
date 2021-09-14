Code for blog article: https://jjfumero.github.io/posts/2021/09/timers-with-level-zero/ 


Example to demonstrate how to measure the elapsed time between two commands within a command list. 
The goal of this example is to measure the data transfers time and the time that it takes to copy data from one buffer to another in Level Zero. 


Key parts:

```cpp
// Alloc buffer tot the time stamp
void *timeStampStartOut = nullptr;
void *timeStampStopOut = nullptr;
constexpr size_t allocSizeTimer = 8;
zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStartOut);
zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStopOut);


// Time start
zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStartOut, nullptr, 0, nullptr);

// Calls to measure
zeCommandListAppendMemoryCopy(cmdList, dstResult, sourceBuffer, allocSize, nullptr, 0, nullptr);
zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr);

// Time stop
zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStopOut, nullptr, 0, nullptr);

// Copy back the timer to the host
uint64_t timeStartOut = 0;
uint64_t timeStopOut = 0;
zeCommandListAppendMemoryCopy(cmdList, &timeStartOut, timeStampStartOut, sizeof(timeStartOut), nullptr, 0, nullptr);
zeCommandListAppendMemoryCopy(cmdList, &timeStopOut, timeStampStopOut, sizeof(timeStopOut), nullptr, 0, nullptr);

// Compute the elapsed time
ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};  
zeDeviceGetProperties(device, &devProperties);

uint64_t copyOutDuration = timeStopOut - timeStartOut;
uint64_t timerResolution = devProperties.timerResolution;
std::cout << "Elapsed Time: " << (copyOutDuration * timerResolution) << std::endl;
```


#### How to compile and run?

```bash
export LEVEL_ZERO_ROOT=/path/to/level-zero-code 
export ZE_SHARED_LOADER=$LEVEL_ZERO_ROOT/build/lib/libze_loader.so
make
./timeDataTransfers <sizeInBytes>
```


#### How to run the benchmarks

```bash
$ python3 runBenchmarks.py --run --db myResultTable.db

// Query database
$ python3 runBenchmarks.py --performance --db myResultTable.db 

//The output format is:
// SIZE TIMER_NAME TIMER_VALUE COUNTER 
```
