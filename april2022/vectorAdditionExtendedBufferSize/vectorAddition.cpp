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
#include <cmath>        

#define VALIDATION 1

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


int main(int argc, char **argv) {

    uint64_t vectorSize = 512;
    if (argc > 1) {
        vectorSize = atoi(argv[1]);
    }

    std::cout << "Vector Size: " << vectorSize << " ---> #bytes: " << (vectorSize * 4) << " -- " << ((vectorSize * 4) * 1e-9 ) << " (GB) " << std::endl;

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


    std::cout << "Max Allocation Size: " << deviceProperties.maxMemAllocSize << " (bytes) " << (deviceProperties.maxMemAllocSize * 1e-9)  << " (GB)" << std::endl;

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
    uint32_t items = vectorSize;
    size_t allocSize = items * sizeof(float);
    ze_device_mem_alloc_desc_t memAllocDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED;

    ze_relaxed_allocation_limits_exp_desc_t exceedCapacity = {
        ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC,
        nullptr, 
        ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE
    };

    // Option A) Shared Memory
    ze_result_t result;
    void *sharedBuffer = nullptr;

    hostDesc.pNext = &exceedCapacity;
    memAllocDesc.pNext = &exceedCapacity;

    void *sharedA = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedA));

    void *sharedB = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedB));

    void *dstResult = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &dstResult));

    std::cout << "[INFO] Allocation done" << std::endl; 


    // memory initialization
    memset(sharedA, 2.5, allocSize);
    memset(sharedB, 3.2, allocSize);
    memset(dstResult, 0.0, allocSize);

    // Module Initialization
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("vectorAddition.spv", std::ios::binary);

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
        kernelDesc.pKernelName = "vectorAdd";
        VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, items, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));


        // Push arguments
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(dstResult), &dstResult));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(sharedA), &sharedA));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(sharedB), &sharedB));

        // Kernel thread-dispatch
        ze_group_count_t dispatch;
        dispatch.groupCountX = items / groupSizeX;
        dispatch.groupCountY = 1;
        dispatch.groupCountZ = 1;


        // Launch kernel on the GPU
        VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatch, nullptr, 0, nullptr));
        VALIDATECALL(zeCommandListAppendBarrier(cmdList, nullptr, 0u, nullptr));

       
        file.close();
    } else {
        std::cout << "SPIR-V binary file not found\n";
        std::terminate();
    }

    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate
    bool outputValidationSuccessful = true;

    float *resultSeq = (float*)malloc(allocSize);
    float *dstFloat = static_cast<float *>(dstResult);
    float *srcA = static_cast<float *>(sharedA);
    float *srcB = static_cast<float *>(sharedB);


    if (VALIDATION) {
        int n = items;
        for (int i = 0; i < n; i++) {
            if (std::abs((srcA[i] + srcB[i]) - dstFloat[i]) > 0.01 ) {
                outputValidationSuccessful = false;
                break;
            }
        }
        std::cout << "\nVector Addition validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";
    }
 
    // Cleanup
    VALIDATECALL(zeMemFree(context, dstResult));
    VALIDATECALL(zeMemFree(context, sharedA));
    VALIDATECALL(zeMemFree(context, sharedB));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    return 0;
}
