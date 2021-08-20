// Sample based on the test-suite exanples from Level-Zero: 
//      https://github.com/intel/compute-runtime/blob/master/level_zero/core/test/black_box_tests/zello_timestamp.cpp

#include <ze_api.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#define VALIDATECALL(myZeCall) \
    if (myZeCall != ZE_RESULT_SUCCESS){ \
        std::cout << "Error at "       \
            << #myZeCall << ": "       \
            << __FUNCTION__ << ": "    \
            << __LINE__ << std::endl;  \
        std::cout << "Exit with Error Code: " \
            << "0x" << std::hex \
            << myZeCall \
            << std::dec << std::endl; \
        std::terminate(); \
    }


void init(ze_driver_handle_t &driverHandle, ze_context_handle_t &context, ze_device_handle_t &device ) {
    // Initialization
    VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    // Get the driver
    uint32_t driverCount = 0;
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));

    VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));

    // Create the context
    ze_context_desc_t contextDescription = {};
    contextDescription.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    VALIDATECALL(zeContextCreate(driverHandle, &contextDescription, &context));

    // Get the device
    uint32_t deviceCount = 0;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));

    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, &device));
}

void printBasicInfo(ze_device_handle_t device) {
    // Print basic properties of the device
    ze_device_properties_t deviceProperties = {};
    VALIDATECALL(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device   : " << deviceProperties.name << "\n" 
              << "Type     : " << ((deviceProperties.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "FPGA") << "\n"
              << "Vendor ID: " << std::hex << deviceProperties.vendorId << std::dec << "\n";

}

uint32_t createCommandQueue(ze_device_handle_t device, ze_context_handle_t context, ze_command_queue_handle_t &cmdQueue) {
    // Create a command queue
    uint32_t numQueueGroups = 0;
    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cout << "No queue groups found\n";
        std::terminate();
    } else {
        std::cout << "#Queue Groups: " << numQueueGroups << std::endl;
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, queueProperties.data()));

    //ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {};
    for (uint32_t i = 0; i < numQueueGroups; i++) { 
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
        }
    }

    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    VALIDATECALL(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    return cmdQueueDesc.ordinal;
}

void createCommandList(ze_device_handle_t device, ze_context_handle_t context, ze_command_list_handle_t &cmdList, uint32_t ordinal) {
    // Create a command list
    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.commandQueueGroupOrdinal = ordinal;    
    VALIDATECALL(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
}

int runWithSharedMemory(int argc, char** argv) {

    uint32_t inputElements = 512;
    if (argc > 1) {
        inputElements = atoi(argv[1]);
    }

    std::cout << "#inputElements: " << inputElements << std::endl;

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;

    init(driverHandle, context, device);

    printBasicInfo(device);
   
    ze_command_queue_handle_t cmdQueue;
    uint32_t ordinal = createCommandQueue(device, context, cmdQueue);

    // Create a command list
    ze_command_list_handle_t cmdList;
    createCommandList(device, context, cmdList, ordinal);

    // Create two buffers
    uint32_t items = inputElements;
    size_t allocSize = items * sizeof(float);

    std::cout << "Total ALLOC SIZE: " << allocSize << " (bytes)\n";

    ze_device_mem_alloc_desc_t memAllocDesc;
    memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc;
    hostDesc.flags = memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;

    void *sharedA = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedA));

    void *dstResult = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &dstResult));

    // memory initialization
    memset(sharedA, 2.5, allocSize);
    memset(dstResult, 0.0, allocSize);

    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, dstResult, sharedA, allocSize, nullptr, 0, nullptr));
       
    auto begin = std::chrono::steady_clock::now();
    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    auto end = std::chrono::steady_clock::now();

    auto elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count();
    std::cout << "C++ Timer = " << elapsedTime << " [ns]" << std::endl;

    // Cleanup
    VALIDATECALL(zeMemFree(context, dstResult));
    VALIDATECALL(zeMemFree(context, sharedA));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));
    return 0;
}

int runWithDeviceMemory(int argc, char **argv) {

    uint32_t inputElements = 512;
    if (argc > 1) {
        inputElements = atoi(argv[1]);
    }

    std::cout << "#inputElements: " << inputElements << std::endl;

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;

    init(driverHandle, context, device);

    printBasicInfo(device);
   
    ze_command_queue_handle_t cmdQueue;
    uint32_t ordinal = createCommandQueue(device, context, cmdQueue);

    // Create a command list
    ze_command_list_handle_t cmdList;
    createCommandList(device, context, cmdList, ordinal);

    // Create two buffers
    uint32_t items = inputElements;
    size_t allocSize = items * sizeof(float);

    std::cout << "Total ALLOC SIZE: " << allocSize << " (bytes)\n";

    ze_device_mem_alloc_desc_t memAllocDesc;
    memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc;
    hostDesc.flags = memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;

    void *deviceBuffer = nullptr;
    ze_result_t result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &deviceBuffer);
    if (result == ZE_RESULT_ERROR_UNSUPPORTED_SIZE) {
        std::cout << "Size is too big. Unsupported\n";
    }
    VALIDATECALL(result);

    void *timeStampStartIn = nullptr;
    void *timeStampStopIn = nullptr;
    const size_t allocSizeTimer = 64;
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStartIn));
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStopIn));

    void *timeStampStartOut = nullptr;
    void *timeStampStopOut = nullptr;
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStartOut));
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStopOut));
    
    float *heapBuffer = new float[inputElements];
    for (int i = 0; i < items; i++) {
        heapBuffer[i] = 10.0;
    }

    float *heapBuffer2 = new float[inputElements];

    // Copy from HEAP -> Device Allocated Memory
    VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStartIn, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, heapBuffer, allocSize, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStopIn, nullptr, 0, nullptr));

    VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStartOut, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, heapBuffer2, deviceBuffer, allocSize, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStopOut, nullptr, 0, nullptr));

    // Copy back timestamps
    uint64_t timeStartIn = 0;
    uint64_t timeStopIn = 0;
    uint64_t timeStartOut = 0;
    uint64_t timeStopOut = 0;
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStartIn, timeStampStartIn, sizeof(timeStartIn), nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStopIn, timeStampStopIn, sizeof(timeStopIn), nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStartOut, timeStampStartOut, sizeof(timeStartOut), nullptr, 0, nullptr));
    VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStopOut, timeStampStopOut, sizeof(timeStopOut), nullptr, 0, nullptr));

    auto begin = std::chrono::steady_clock::now();
    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    auto end = std::chrono::steady_clock::now();

    auto elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count();
    std::cout << "C++ Timer = " << elapsedTime << " [ns]" << std::endl;

    ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    VALIDATECALL(zeDeviceGetProperties(device, &devProperties));

    uint64_t copyInDuration = timeStopIn - timeStartIn;
    uint64_t copyOutDuration = timeStopOut - timeStartOut;
    uint64_t timerResolution = devProperties.timerResolution;
    std::cout << "Global timestamp statistics: \n"
              << std::fixed
              << " IN : " << copyInDuration * timerResolution << " ns\n"
              << " OUT: " << copyOutDuration * timerResolution << " ns\n";

    // Cleanup
    VALIDATECALL(zeMemFree(context, deviceBuffer));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));
    return 0;
}

int main(int argc, char**argv) {

    //runWithSharedMemory(argc, argv);

    runWithDeviceMemory(argc, argv);

    return 0;
}