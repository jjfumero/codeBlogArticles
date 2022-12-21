/*
 * MIT License
 * 
 * Copyright (c) 2021, Juan Fumero
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

#define VALIDATION 0

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

// Sequential Matrix Multiplication to validate results
void matrixMultply(float *a, float *b, float *c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

void createEventPoolAndEvents(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_event_pool_handle_t &eventPool,
                              ze_event_pool_flag_t poolFlag,
                              uint32_t poolSize,
                              ze_event_handle_t *events) {

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    eventPoolDesc.count = poolSize;
    eventPoolDesc.flags = poolFlag;

    VALIDATECALL(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));

    for (uint32_t i = 0; i < poolSize; i++) {
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        VALIDATECALL(zeEventCreate(eventPool, &eventDesc, events + i));
    }

}

int main(int argc, char **argv) {

    uint32_t sizeMatrix = 512;
    if (argc > 1) {
        sizeMatrix = atoi(argv[1]);
    }

    std::cout << "Matrix Size: " << sizeMatrix << " x " << sizeMatrix << std::endl;

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

    // Create two buffers
    uint32_t items = sizeMatrix;
    size_t allocSize = items * items * sizeof(float);
    ze_device_mem_alloc_desc_t memAllocDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    //memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    //hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;


    void *sharedA = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedA));

    void *sharedB = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedB));

    void *dstResult = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &dstResult));


    // memory initialization
    memset(sharedA, 2.5, allocSize);
    memset(sharedB, 3.2, allocSize);
    memset(dstResult, 0.0, allocSize);

    void *timestampBuffer = nullptr;
    VALIDATECALL(zeMemAllocHost(context, &hostDesc, sizeof(ze_kernel_timestamp_result_t), 1, &timestampBuffer));
    memset(timestampBuffer, 0, sizeof(ze_kernel_timestamp_result_t));

    //VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, sizeof(ze_kernel_timestamp_result_t), 1, device, &timestampBuffer));

    //VALIDATECALL(zeMemAllocDevice(context, &memAllocDesc, sizeof(ze_kernel_timestamp_result_t), sizeof(uint32_t), device, &timestampBuffer));
    

    // Module Initialization
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("matrixMultiply.spv", std::ios::binary);

    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;

     // Event description
        ze_event_pool_handle_t eventPool;

        // Event handler
        ze_event_handle_t kernelTsEvent;

    if (file.is_open()) {
        file.seekg(0, file.end);
        auto length = file.tellg();
        file.seekg(0, file.beg);

        std::unique_ptr<char[]> spirvInput(new char[length]);
        file.read(spirvInput.get(), length);

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
        kernelDesc.pKernelName = "mxm";
        VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = 64u;
        uint32_t groupSizeY = 64u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, items, items, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));

        // After suggestion
        std::cout << "GroupSizeX: " << groupSizeX << std::endl;
        std::cout << "GroupSizeY: " << groupSizeY << std::endl;
        std::cout << "GroupSizeX: " << groupSizeZ << std::endl;


        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));


        // Push arguments
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(sharedA), &sharedA));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(sharedB), &sharedB));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(dstResult), &dstResult));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 3, sizeof(int), &items));

        // Kernel thread-dispatch
        ze_group_count_t dispatch;
        dispatch.groupCountX = items / groupSizeX;
        dispatch.groupCountY = items / groupSizeY;
        dispatch.groupCountZ = 1;


        createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1, &kernelTsEvent);

        // Launch kernel on the GPU
        VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatch, kernelTsEvent, 0, nullptr));
        VALIDATECALL(zeCommandListAppendBarrier(cmdList, nullptr, 0u, nullptr));
        VALIDATECALL(zeCommandListAppendQueryKernelTimestamps(cmdList, 1u, &kernelTsEvent, timestampBuffer, nullptr, nullptr, 0u, nullptr));

       
        file.close();
    } else {
        std::cout << "SPIR-V binary file not found\n";
        std::terminate();
    }

    begin = std::chrono::steady_clock::now();
    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    end = std::chrono::steady_clock::now();

   
    ze_kernel_timestamp_result_t *kernelTsResults = reinterpret_cast<ze_kernel_timestamp_result_t *>(timestampBuffer);

    uint64_t timerResolution = deviceProperties.timerResolution;
    uint64_t kernelDuration = kernelTsResults->context.kernelEnd - kernelTsResults->context.kernelStart;
    uint64_t gpuKernelTime;

    std::cout << "Kernel timestamp statistics (V1.2 and later): \n"
                  << std::fixed
                  << "\tGlobal start : " << std::dec << kernelTsResults->global.kernelStart << " cycles\n"
                  << "\tKernel start: " << std::dec << kernelTsResults->context.kernelStart << " cycles\n"
                  << "\tKernel end: " << std::dec << kernelTsResults->context.kernelEnd << " cycles\n"
                  << "\tGlobal end: " << std::dec << kernelTsResults->global.kernelEnd << " cycles\n"
                  << "\ttimerResolution clock: " << std::dec << timerResolution << " cycles/s\n"
                  << "\tKernel duration : " << std::dec << kernelDuration << " cycles, " << kernelDuration * (1000000000.0 / static_cast<double>(timerResolution)) << " ns\n";
                  gpuKernelTime =  kernelDuration * (1000000000.0 / static_cast<double>(timerResolution));

    if (deviceProperties.stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2) {
        std::cout << "Kernel timestamp statistics (V1.2 and later): \n"
                  << std::fixed
                  << "\tGlobal start : " << std::dec << kernelTsResults->global.kernelStart << " cycles\n"
                  << "\tKernel start: " << std::dec << kernelTsResults->context.kernelStart << " cycles\n"
                  << "\tKernel end: " << std::dec << kernelTsResults->context.kernelEnd << " cycles\n"
                  << "\tGlobal end: " << std::dec << kernelTsResults->global.kernelEnd << " cycles\n"
                  << "\ttimerResolution clock: " << std::dec << timerResolution << " cycles/s\n"
                  << "\tKernel duration : " << std::dec << kernelDuration << " cycles, " << kernelDuration * (1000000000.0 / static_cast<double>(timerResolution)) << " ns\n";
                  gpuKernelTime =  kernelDuration * (1000000000.0 / static_cast<double>(timerResolution));
    } else {
        std::cout << "Kernel timestamp statistics (prior to V1.2): \n"
                  << std::fixed
                  << "\tGlobal start : " << std::dec << kernelTsResults->global.kernelStart << " cycles\n"
                  << "\tKernel start: " << std::dec << kernelTsResults->context.kernelStart << " cycles\n"
                  << "\tKernel end: " << std::dec << kernelTsResults->context.kernelEnd << " cycles\n"
                  << "\tGlobal end: " << std::dec << kernelTsResults->global.kernelEnd << " cycles\n"
                  << "\ttimerResolution: " << std::dec << timerResolution << " ns\n"
                  << "\tKernel duration : " << std::dec << kernelDuration << " cycles\n"
                  << "\tKernel Time: " << kernelDuration * timerResolution << " ns\n";
                  gpuKernelTime = kernelDuration * timerResolution;
    }

    // Validate
    bool outputValidationSuccessful = true;

    float *resultSeq = (float *)malloc(allocSize);
    float *dstFloat = static_cast<float *>(dstResult);
    float *srcA = static_cast<float *>(sharedA);
    float *srcB = static_cast<float *>(sharedB);

    std::chrono::steady_clock::time_point beginSeq = std::chrono::steady_clock::now();
    matrixMultply(srcA, srcB, resultSeq, items);
    std::chrono::steady_clock::time_point endSeq = std::chrono::steady_clock::now();

    auto elapsedParallel = std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count();
    auto elapsedSequential = std::chrono::duration_cast<std::chrono::nanoseconds> (endSeq - beginSeq).count();
    std::cout << "GPU-KERNEL = " << gpuKernelTime << " [ns]" << std::endl;
    std::cout << "PARALLEL = " << elapsedParallel << " [ns]" << std::endl;
    std::cout << "SEQ = " << elapsedSequential << " [ns]" << std::endl;
    auto speedup = elapsedSequential / elapsedParallel;
    //std::cout << "Speedup = " << speedup << "x" << std::endl;

    if (VALIDATION) {
        int n = items;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (resultSeq[i * n + j] != dstFloat[i * n + j]) {
                    outputValidationSuccessful = false;
                    break;
                } 
            }
        }
        std::cout << "\nMatrix Multiply validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";
    }
 
    // Cleanup
    VALIDATECALL(zeMemFree(context, timestampBuffer));
    VALIDATECALL(zeMemFree(context, dstResult));
    VALIDATECALL(zeMemFree(context, sharedA));
    VALIDATECALL(zeMemFree(context, sharedB));
    VALIDATECALL(zeEventDestroy(kernelTsEvent));
    VALIDATECALL(zeEventPoolDestroy(eventPool));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    return 0;
}
