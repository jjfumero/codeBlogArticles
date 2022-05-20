/*
 * MIT License
 * 
 * Copyright (c) 2022, Juan Fumero
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Example for dispatching a SPIR-V Kernel using Level Zero on the Intel HD Graphics
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

void checkMemoryError(int result) {
    if (result == 0x78000009) {
            std::cout << "size argument is not supported by the device \n";
    } else if (result == ZE_RESULT_SUCCESS) {
        std::cout << "\tAlloc OK" << std::endl;
    }
}

int main(int argc, char **argv) {

    size_t items = 8192;
    std::string version = "s"; // shared by default
    
    if (argc > 2) {
        version = argv[1];
        items = atoll(argv[2]);
    }
    size_t allocSize = items * sizeof(int);

    std::cout << "SIZE: " << items << std::endl;

    // GET THE VERSIONS
    // "s" : shared
    // "d" : device
    // "h" : host
    bool use_shared_memory = false;
    bool use_device_memory = false;
    bool use_host_memory = false;
    bool use_host_only_memory = false;
    if ( version == "s" ) {
        // Shared memory
        std::cout << "Using Shared Memory" << std::endl;
        use_shared_memory = true;
    } else if ( version == "d" ) {
        // Device only memory
        std::cout << "Using Device Memory" << std::endl;
        use_device_memory = true;
    } else if ( version == "h" ) {
        // Use Combined Host/Shared Memory. Mimic scenario for managed runtime programming languages
        // such as Java. 
        std::cout << "Using Combined Host/Device Memory" << std::endl;
        use_host_memory = true;
    } else if ( version == "o" ) {
        // Host Only memory
        std::cout << "Using Host ONLY Memory" << std::endl;
        use_host_only_memory = true;
    }


    // Initialization
    VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    // Get the driver
    uint32_t driverCount = 0;
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));

    ze_driver_handle_t driverHandle;
    VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));

    // Create the context
    ze_context_desc_t contextDescription = {};
    contextDescription.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    ze_context_handle_t context;
    VALIDATECALL(zeContextCreate(driverHandle, &contextDescription, &context));

    // Get the device
    uint32_t deviceCount = 0;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));

    ze_device_handle_t device;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, &device));

    // Print basic properties of the device
    ze_device_properties_t deviceProperties = {};
    VALIDATECALL(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device   : " << deviceProperties.name << "\n" 
              << "Type     : " << ((deviceProperties.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "FPGA") << "\n"
              << "Vendor ID: " << std::hex << deviceProperties.vendorId << std::dec << "\n";

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

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {};
    for (uint32_t i = 0; i < numQueueGroups; i++) { 
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
        }
    }

    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    VALIDATECALL(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    // Create a command list
    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;    
    VALIDATECALL(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));


    ze_device_mem_alloc_desc_t memAllocDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};


    ze_relaxed_allocation_limits_exp_desc_t exceedCapacity = {
        ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC,
        nullptr, 
        ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE
    };

    // Option A) Shared Memory
    ze_result_t result;
    void *computeBufferA = nullptr;
    void *computeBufferB = nullptr;
    int *hostBufferA = nullptr;
    int *hostBufferB = nullptr;
    hostDesc.pNext = &exceedCapacity;
    memAllocDesc.pNext = &exceedCapacity;

    if (use_shared_memory) {  
        std::cout << "Allocating Shared Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 128, device, &computeBufferA);
        checkMemoryError(result);

        std::cout << "Allocating Shared Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 128, device, &computeBufferB);
        checkMemoryError(result);

    } else if (use_device_memory) {
        std::cout << "Allocating Device Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &computeBufferA);
        checkMemoryError(result);

        std::cout << "Allocating Device Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &computeBufferB);
        checkMemoryError(result);
    } else if (use_host_memory) {

        std::cout << "Allocating Device Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &computeBufferA);
        checkMemoryError(result);

        std::cout << "Allocating Device Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &computeBufferB);
        checkMemoryError(result);

        std::cout << "Allocating Host Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocHost(context, &hostDesc, allocSize, 64, &hostBufferA);
        checkMemoryError(result);

        std::cout << "Allocating Host Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocHost(context, &hostDesc, allocSize, 64, &hostBufferB);
        checkMemoryError(result);
    } else if (use_host_only_memory) {
        std::cout << "Allocating Host Only Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocHost(context, &hostDesc, allocSize, 64, &hostBufferA);
        checkMemoryError(result);

        std::cout << "Allocating Host Only Memory: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
        result = zeMemAllocHost(context, &hostDesc, allocSize, 64, &hostBufferB);
        checkMemoryError(result);
    }

    void *deviceBuffer = nullptr;
    void *hostBuffer = nullptr;
    int *heapBuffer = nullptr;
    int *resultBuffer = nullptr;

    if (use_shared_memory) {
        // memory initialization
        constexpr uint8_t val = 100;
        int32_t *srcCharBufferInit = static_cast<int32_t *>(computeBufferA);
        for (size_t i = 0; i < items; i++) {
            srcCharBufferInit[i] = 100;
        }
    } else if (use_device_memory) {
        heapBuffer = new int[items];
        resultBuffer = new int[items];
        for (size_t i = 0; i < items; i++) {
            heapBuffer[i] = 100;
        }
    } else if (use_host_memory || use_host_only_memory) {
         for (size_t i = 0; i < items; ++i) {
            hostBufferA[i] = static_cast<int>(100);
        }
    }

    // Module Initialization
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("vectorAddition.spv", std::ios::binary);

    file.seekg(0, file.end);
    auto length = file.tellg();
    file.seekg(0, file.beg);

    std::unique_ptr<char[]> spirvInput(new char[length]);
    file.read(spirvInput.get(), length);
    file.close(); 

    ze_module_desc_t moduleDesc = {};
    ze_module_build_log_handle_t buildLog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
    moduleDesc.inputSize = length;
    moduleDesc.pBuildFlags = "";

    auto status = zeModuleCreate(context, device, &moduleDesc, &module, &buildLog);
    if (status != ZE_RESULT_SUCCESS) {
        // print log
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildLog, &szLog, nullptr);

        char* stringLog = (char*)malloc(szLog);
        zeModuleBuildLogGetString(buildLog, &szLog, stringLog);
        std::cout << "Build log: " << stringLog << std::endl;
    }
    VALIDATECALL(zeModuleBuildLogDestroy(buildLog));   

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "vectorAddition";
    VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));

    void *timeStampStartOut = nullptr;
    void *timeStampStopOut = nullptr;
    constexpr size_t allocSizeTimer = 8;
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStartOut));
    VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, allocSizeTimer, 1, device, &timeStampStopOut));


    for (int i = 0; i < 10; i++) {

        auto begin = std::chrono::steady_clock::now();
        VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStartOut, nullptr, 0, nullptr));

        // Copy from host to device if needed
        if (use_device_memory) {
            VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, computeBufferA, heapBuffer, allocSize, nullptr, 0, nullptr));
        } else if (use_host_memory) {
            VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, computeBufferA, hostBufferA, allocSize, nullptr, 0, nullptr));
        }
        
        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, items, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        // Push arguments
        if (use_host_only_memory) {
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(hostBufferA), &hostBufferA));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(hostBufferB), &hostBufferB));
        } else {
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(computeBufferA), &computeBufferA));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(computeBufferB), &computeBufferB));
        }

        // Kernel thread-dispatch
        ze_group_count_t dispatch;
        dispatch.groupCountX = items / groupSizeX;
        dispatch.groupCountY = 1;
        dispatch.groupCountZ = 1;

        // Launch kernel on the GPU
        VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatch, nullptr, 0, nullptr));

        // Copy from device to host
        if (use_device_memory) {
            VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, resultBuffer, computeBufferB, allocSize, nullptr, 0, nullptr));
        } else if (use_host_memory) {
            VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, hostBufferB, computeBufferB, allocSize, nullptr, 0, nullptr));
        }

        VALIDATECALL(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)timeStampStopOut, nullptr, 0, nullptr));

        uint64_t timeStartOut = 0;
        uint64_t timeStopOut = 0;
        VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStartOut, timeStampStartOut, sizeof(timeStartOut), nullptr, 0, nullptr));
        VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, &timeStopOut, timeStampStopOut, sizeof(timeStopOut), nullptr, 0, nullptr));

        // Close list and submit for execution
        VALIDATECALL(zeCommandListClose(cmdList));
        VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
        VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        auto end = std::chrono::steady_clock::now();

        auto elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count();
        std::cout << "C++-Timer: " << elapsedTime << " [ns]" << std::endl;

        ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};  
        VALIDATECALL(zeDeviceGetProperties(device, &devProperties));

        uint64_t copyOutDuration = timeStopOut - timeStartOut;
        uint64_t timerResolution = devProperties.timerResolution;
        std::cout << "Timer    : " << copyOutDuration * timerResolution << " [ns]\n";
 
        // Reset command list
        VALIDATECALL(zeCommandListReset(cmdList));
    }

    // Validate
    bool outputValidationSuccessful = true;
    if (use_shared_memory) {
        int32_t *srcCharBuffer = static_cast<int32_t *>(computeBufferA);
        int32_t *dstCharBuffer = static_cast<int32_t *>(computeBufferB);
        for (size_t i = 0; i < items; i++) {
            //std::cout << dstCharBuffer[i] << std::endl;
            if (dstCharBuffer[i] != (srcCharBuffer[i] + 100)) {
                outputValidationSuccessful = false;
                break;
            }
        }
    } else if (use_device_memory) {
        for (size_t i = 0; i < items; i++) {
            //std::cout << resultBuffer[i] << std::endl;
            if (resultBuffer[i] != (heapBuffer[i] + 100)) {
                outputValidationSuccessful = false;
                break;
            }
        }
    } else if (use_host_memory || use_host_only_memory) {
        for (size_t i = 0; i < items; i++) {
            //std::cout << hostBufferB[i] << std::endl;
            if (hostBufferB[i] != (hostBufferA[i] + 100)) {
                outputValidationSuccessful = false;
                break;
            }
        }
    }

    std::cout << "\nResults validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    // Cleanup
    if (computeBufferA != nullptr) {
        VALIDATECALL(zeMemFree(context, computeBufferA));
    }
    if (computeBufferB != nullptr) {
        VALIDATECALL(zeMemFree(context, computeBufferB));
    }
    if (deviceBuffer != nullptr) {
        VALIDATECALL(zeMemFree(context, deviceBuffer));
    }
    if (hostBuffer != nullptr) {
        VALIDATECALL(zeMemFree(context, hostBuffer));
    }
    if (hostBufferA != nullptr) {
        VALIDATECALL(zeMemFree(context, hostBufferA));
    }
    if (hostBufferB != nullptr) {
        VALIDATECALL(zeMemFree(context, hostBufferB));
    }
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    return 0;
}
