// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#pragma once

#include <CL/cl.h>

#include <string.h>
#include <limits>

#include <sgpp/globaldef.hpp>
#include <sgpp/base/opencl/LinearLoadBalancerMultiPlatform.hpp>
#include <sgpp/base/opencl/OCLClonedBufferMultiPlatform.hpp>
#include <sgpp/base/opencl/OCLOperationConfiguration.hpp>
#include <sgpp/base/opencl/OCLManagerMultiPlatform.hpp>
#include <sgpp/base/opencl/OCLStretchedBufferMultiPlatform.hpp>
#include "SourceBuilderMultTranspose.hpp"
#include <sgpp/base/opencl/OCLClonedBufferSD.hpp>

namespace SGPP {
namespace datadriven {
namespace StreamingOCLMultiPlatform {

template<typename T>
class KernelMultTranspose {
private:
    std::shared_ptr<base::OCLDevice> device;

    size_t dims;

    cl_int err;

    base::OCLClonedBufferSD<T> deviceLevelTranspose;
    base::OCLClonedBufferSD<T> deviceIndexTranspose;

    base::OCLClonedBufferSD<T> deviceDataTranspose;
    base::OCLClonedBufferSD<T> deviceSourceTranspose;

    base::OCLClonedBufferSD<T> deviceResultGridTranspose;

    cl_kernel kernelMultTranspose;

    double deviceTimingMultTranspose;

    SourceBuilderMultTranspose<T> kernelSourceBuilder;
    std::shared_ptr<base::OCLManagerMultiPlatform> manager;
//    std::shared_ptr<base::OCLOperationConfiguration> parameters;
    json::Node &kernelConfiguration;

    std::shared_ptr<base::QueueLoadBalancer> queueLoadBalancerMultTranspose;

    bool verbose;
public:

    KernelMultTranspose(std::shared_ptr<base::OCLDevice> device, size_t dims,
            std::shared_ptr<base::OCLManagerMultiPlatform> manager, json::Node &kernelConfiguration,
            std::shared_ptr<base::QueueLoadBalancer> queueBalancerMultTranpose) :
            device(device), dims(dims), err(CL_SUCCESS), deviceLevelTranspose(device), deviceIndexTranspose(device), deviceDataTranspose(
                    device), deviceSourceTranspose(device), deviceResultGridTranspose(device), kernelMultTranspose(
                    nullptr), kernelSourceBuilder(device, kernelConfiguration, dims), manager(manager), kernelConfiguration(
                    kernelConfiguration), queueLoadBalancerMultTranspose(queueBalancerMultTranpose) {

        //initialize with same timing to enforce equal problem sizes in the beginning
        this->deviceTimingMultTranspose = 1.0;
        this->verbose = kernelConfiguration["VERBOSE"].getBool();
    }

    ~KernelMultTranspose() {
        if (this->kernelMultTranspose != nullptr) {
            clReleaseKernel(this->kernelMultTranspose);
            this->kernelMultTranspose = nullptr;
        }
    }

    void resetKernel() {
        //TODO: fix for splittedkernel -> currently won't work for multiple iterations
        //leads to a reallocation before next kernel execution

        releaseGridBuffersTranspose();
        releaseDataBuffersTranspose();
        releaseGridResultBufferTranspose();
    }

    double multTranspose(std::vector<T> &level, std::vector<T> &index, std::vector<T> &dataset, std::vector<T> &source,
            std::vector<T> &result, const size_t start_index_grid, const size_t end_index_grid,
            const size_t start_index_data, const size_t end_index_data) {

        // check if there is something to do at all
        if (!(end_index_grid > start_index_grid && end_index_data > start_index_data)) {
            return 0.0;
        }

        size_t local = kernelConfiguration["LOCAL_SIZE"].getUInt();

        if (this->kernelMultTranspose == nullptr) {
            std::string program_src = kernelSourceBuilder.generateSource();
            this->kernelMultTranspose = manager->buildKernel(program_src, device, "multTransOCL");
        }

        this->deviceTimingMultTranspose = 0.0;

        while (true) {

            size_t kernelStartData = start_index_data;
            size_t kernelEndData = end_index_data;

            // set kernel arguments
            size_t kernelStartGrid;
            size_t kernelEndGrid;

            //TODO: change after blocking is implemented
            //TODO: don't forget to set padding to DATA_BLOCKING * THREAD_BLOCK_SIZE
            size_t dataBlockingSize = 1;

            size_t scheduleSize = 128 * 20;

            //TODO: start_index_data not considered!
            bool segmentAvailable = queueLoadBalancerMultTranspose->getNextSegment(scheduleSize, dataBlockingSize,
                    kernelStartGrid, kernelEndGrid);
            if (!segmentAvailable) {
                break;
            }

            size_t rangeSizeUnblocked = kernelEndGrid - kernelStartGrid;
            size_t sourceSize = end_index_data - start_index_data;

            if (verbose) {
                std::cout << "device: " << device->platformId << " kernel from: " << kernelStartGrid << " to: "
                        << kernelEndGrid << " -> range: " << rangeSizeUnblocked << std::endl;
            }

            initGridBuffersTranspose(level, index, kernelStartGrid, kernelEndGrid);
            initDatasetBuffersTranspose(dataset, source, kernelStartData, kernelEndData);
            initGridResultBuffersTranspose(kernelStartGrid, kernelEndGrid);

            clFinish(device->commandQueue);

            size_t rangeSizeBlocked = (kernelEndGrid / dataBlockingSize) - (kernelStartGrid / dataBlockingSize);

            if (rangeSizeBlocked > 0) {
                err = clSetKernelArg(kernelMultTranspose, 0, sizeof(cl_mem), this->deviceLevelTranspose.getBuffer());
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 1, sizeof(cl_mem), this->deviceIndexTranspose.getBuffer());
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 2, sizeof(cl_mem), this->deviceDataTranspose.getBuffer());
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 3, sizeof(cl_mem), this->deviceSourceTranspose.getBuffer());
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 4, sizeof(cl_mem),
                        this->deviceResultGridTranspose.getBuffer());
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 5, sizeof(cl_uint), &sourceSize);
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 6, sizeof(cl_uint), &kernelStartData);
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }
                err = clSetKernelArg(kernelMultTranspose, 7, sizeof(cl_uint), &kernelEndData);
                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to create kernel arguments for device " << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }

                cl_event clTiming;

                err = clEnqueueNDRangeKernel(device->commandQueue, kernelMultTranspose, 1, 0, &rangeSizeBlocked, &local,
                        0, nullptr, &clTiming);

                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to enqueue kernel command! Error code: " << err << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }

                clFinish(device->commandQueue);
//                std::cout << "executed kernel: " << device->deviceId << "" << std::endl;

                deviceResultGridTranspose.readFromBuffer();

                clFinish(device->commandQueue);
//                std::cout << "read from buffer: " << device->deviceId << "" << std::endl;

                std::vector<T> &deviceResultGridTransposeHost = deviceResultGridTranspose.getHostPointer();
                for (size_t i = 0; i < rangeSizeUnblocked; i++) {
                    if (kernelStartGrid + i >= end_index_grid) {
                        break;
                    }
                    result[kernelStartGrid + i] = deviceResultGridTransposeHost[i];
                }

                // determine kernel execution time
                cl_ulong startTime = 0;
                cl_ulong endTime = 0;

                err = clGetEventProfilingInfo(clTiming,
                CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, nullptr);

                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString
                            << "OCL Error: Failed to read start-time from command queue (or crash in multTranspose)! Error code: "
                            << err << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }

                err = clGetEventProfilingInfo(clTiming,
                CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, nullptr);

                if (err != CL_SUCCESS) {
                    std::stringstream errorString;
                    errorString << "OCL Error: Failed to read end-time from command queue! Error code: " << err
                            << std::endl;
                    throw SGPP::base::operation_exception(errorString.str());
                }

                double time = 0.0;
                time = (double) (endTime - startTime);
                time *= 1e-9;

                if (verbose) {
                    std::cout << "device: " << device->deviceId << " duration: " << time << std::endl;
                }

                this->deviceTimingMultTranspose += time;

                clReleaseEvent(clTiming);
            }
        }
        return this->deviceTimingMultTranspose;
    }
private:

    void releaseGridBuffersTranspose() {
        this->deviceLevelTranspose.freeBuffer();
        this->deviceIndexTranspose.freeBuffer();
    }

    void releaseDataBuffersTranspose() {
        this->deviceSourceTranspose.freeBuffer();
        this->deviceDataTranspose.freeBuffer();
    }

    void releaseGridResultBufferTranspose() {
        this->deviceResultGridTranspose.freeBuffer();
    }

    void initGridBuffersTranspose(std::vector<T> &level, std::vector<T> &index, size_t kernelStartGrid,
            size_t kernelEndGrid) {

        deviceLevelTranspose.intializeTo(level, dims, kernelStartGrid, kernelEndGrid);
        deviceIndexTranspose.intializeTo(index, dims, kernelStartGrid, kernelEndGrid);
    }

    void initDatasetBuffersTranspose(std::vector<T> &dataset, std::vector<T> &source, size_t kernelStartData,
            size_t kernelEndData) {

        deviceDataTranspose.intializeTo(dataset, dims, kernelStartData, kernelEndData, true);
        deviceSourceTranspose.intializeTo(source, 1, kernelStartData, kernelEndData);
    }

    void initGridResultBuffersTranspose(size_t kernelStartGrid, size_t kernelEndGrid) {

        size_t range = kernelEndGrid - kernelStartGrid;

        std::vector<T> zeros(range);
        for (size_t i = 0; i < range; i++) {
            zeros[i] = 0.0;
        }

        deviceResultGridTranspose.intializeTo(zeros, 1, 0, range);
    }

}
;

}
}
}
