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

int main(int argc, char **argv) {

    size_t allocSize = 2147483648L;
    if (argc > 1) {
        allocSize = atoll(argv[1]);
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
    void *sharedBuffer = nullptr;

    hostDesc.pNext = &exceedCapacity;
    memAllocDesc.pNext = &exceedCapacity;

    std::cout << "Allocating Shared: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
    result = zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 128, device, &sharedBuffer);
    if (result == 0x78000009) {
         std::cout << "size argument is not supported by the device \n";
    } else if (result == ZE_RESULT_SUCCESS) {
        std::cout << "\tAlloc OK" << std::endl;
    }


    // // Option B) Device Memory
    void *deviceBuffer = nullptr;
    std::cout << "Allocating On Device: " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
    result = zeMemAllocDevice(context, &memAllocDesc, allocSize, 64, device, &deviceBuffer);
    if (result == 0x78000009) {
        std::cout << "size argument is not supported by the device \n";
    } else if (result == ZE_RESULT_SUCCESS) {
        std::cout << "\tAlloc OK" << std::endl;
    }
    
    // // Option C) Host Allocated Memory
    void *hostBuffer = nullptr;
    std::cout << "Allocating from Host " << allocSize << " bytes - " << (allocSize * 1e-9 ) << " (GB) " << std::endl;
    result = zeMemAllocHost(context, &hostDesc, allocSize, 64, &hostBuffer);
    if (result == 0x78000009) {
        std::cout << "size argument is not supported by the device \n";
    } else if (result == ZE_RESULT_SUCCESS) {
        std::cout << "\tAlloc OK" << std::endl;
    }
     
    // Cleanup
    if (sharedBuffer != nullptr) {
        VALIDATECALL(zeMemFree(context, sharedBuffer));
    }
    if (deviceBuffer != nullptr) {
        VALIDATECALL(zeMemFree(context, deviceBuffer));
    }
    if (hostBuffer != nullptr) {
        VALIDATECALL(zeMemFree(context, hostBuffer));
    }
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    return 0;
}
