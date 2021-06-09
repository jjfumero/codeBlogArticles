// Example for dispatching a SPIR-V Kernel using Level Zero on the Intel HD Graphics
// Sample based on the test-suite exanples from Level-Zero: 
//      https://github.com/intel/compute-runtime/blob/master/level_zero/core/test/black_box_tests/zello_world_gpu.cpp

#include "ze_api.h"

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

// Sequential Matrix Multiplication to validate results
void matrixMultply(uint32_t *a, uint32_t *b, uint32_t *c, int n) {
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

int main(int argc, char **argv) {

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
    const uint32_t items = 1024;
    constexpr size_t allocSize = items * items * sizeof(int);
    ze_device_mem_alloc_desc_t memAllocDesc;
    memAllocDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    memAllocDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc;
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;


    void *sharedA = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedA));

    void *sharedB = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &sharedB));

    void *dstResult = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &memAllocDesc, &hostDesc, allocSize, 1, device, &dstResult));

    // memory initialization
    constexpr uint8_t val = 2;
    memset(sharedA, val, allocSize);
    memset(sharedB, 3, allocSize);
    memset(dstResult, 0, allocSize);

    // Module Initialization
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("matrixMultiply.spv", std::ios::binary);

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

        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 32u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, items, items, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        std::cout << "Group X: " << groupSizeX << std::endl;
        std::cout << "Group Y: " << groupSizeY << std::endl;

        // Push arguments
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(dstResult), &dstResult));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(sharedA), &sharedA));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(sharedB), &sharedB));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 3, sizeof(int), &items));

        // Kernel thread-dispatch
        ze_group_count_t dispatch;
        dispatch.groupCountX = items / groupSizeX;
        dispatch.groupCountY = items / groupSizeY;
        dispatch.groupCountZ = 1;

        // Launch kernel on the GPU
        VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatch, nullptr, 0, nullptr));

        file.close();
    } else {
        std::cout << "SPIR-V binary file not found\n";
        std::terminate();
    }

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    // Close list abd submit for execution
    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));    
    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();


    // Validate
    bool outputValidationSuccessful = true;

    uint32_t *resultSeq = (uint32_t *)malloc(allocSize);
    uint32_t *dstInt = static_cast<uint32_t *>(dstResult);
    uint32_t *srcA = static_cast<uint32_t *>(sharedA);
    uint32_t *srcB = static_cast<uint32_t *>(sharedB);

    std::chrono::steady_clock::time_point beginSeq = std::chrono::steady_clock::now();
    matrixMultply(srcA, srcB, resultSeq, items);
    std::chrono::steady_clock::time_point endSeq = std::chrono::steady_clock::now();

    auto elapsedParallel = std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count();
    auto elapsedSequential = std::chrono::duration_cast<std::chrono::nanoseconds> (endSeq - beginSeq).count();
    std::cout << "GPU Kernel = " << elapsedParallel << " [ns]" << std::endl;
    std::cout << "SEQ Kernel = " << elapsedSequential << " [ns]" << std::endl;
    auto speedup = elapsedSequential / elapsedParallel;
    std::cout << "Speedup = " << speedup << "x" << std::endl;

    int n = items;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (resultSeq[i * n + j] != dstInt[i * n + j]) {
                outputValidationSuccessful = false;
                break;
            } 
        }
    }


    std::cout << "\nMatrix Multiply validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    // Cleanup
    VALIDATECALL(zeMemFree(context, dstResult));
    VALIDATECALL(zeMemFree(context, sharedA));
    VALIDATECALL(zeMemFree(context, sharedB));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    return 0;
}
