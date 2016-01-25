/*
 * multiEvalPerformance.cpp
 *
 *  Created on: Mar 12, 2015
 *      Author: pfandedd
 */

#if USE_OCL==1

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <random>
#include <fstream>
#include <iostream>

#include <zlib.h>


#include "test_datadrivenCommon.hpp"

#include <sgpp/globaldef.hpp>

#include <sgpp/base/operation/hash/OperationMultipleEval.hpp>
#include <sgpp/datadriven/DatadrivenOpFactory.hpp>
#include <sgpp/base/operation/BaseOpFactory.hpp>
#include <sgpp/datadriven/tools/ARFFTools.hpp>
#include <sgpp/base/grid/generation/functors/SurplusRefinementFunctor.hpp>
#include <sgpp/base/tools/ConfigurationParameters.hpp>
#include <sgpp/base/opencl/OCLOperationConfiguration.hpp>

BOOST_AUTO_TEST_SUITE(TestStreamingOCLMult)

BOOST_AUTO_TEST_CASE(Simple) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 1E-22), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 1E-18) };

    uint32_t level = 5;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsSingleDevice();
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 1ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "array");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 1ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_CASE(Blocking) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 1E-22), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 1E-18) };

    uint32_t level = 6;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsSingleDevice();
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 4ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "register");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_CASE(MultiDevice) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 1E-22), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 1E-18) };

    uint32_t level = 6;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsMultiDevice();
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 4ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "array");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_CASE(SimpleSinglePrecision) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 10E-5), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 10E-1) };

    uint32_t level = 6;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsSingleDevice();
    parameters.replaceTextAttr("INTERNAL_PRECISION", "float");
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 1ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "array");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_CASE(BlockingSinglePrecision) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 10E-5), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 10E-1) };

    uint32_t level = 6;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsSingleDevice();
    parameters.replaceTextAttr("INTERNAL_PRECISION", "float");
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 4ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "array");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_CASE(MultiDeviceSinglePrecision) {

    std::vector<std::tuple<std::string, double> > fileNamesError = { std::tuple<std::string, double>(
            "datadriven/tests/data/friedman_4d.arff.gz", 10E-5), std::tuple<std::string, double>("datadriven/tests/data/friedman_10d.arff.gz", 10E-1) };

    uint32_t level = 6;

    SGPP::base::OCLOperationConfiguration parameters = getConfigurationDefaultsMultiDevice();
    parameters.replaceTextAttr("INTERNAL_PRECISION", "float");
    parameters.replaceIDAttr("KERNEL_USE_LOCAL_MEMORY", false);
    parameters.replaceIDAttr("KERNEL_DATA_BLOCKING_SIZE", 4ul);
    parameters.replaceIDAttr("KERNEL_TRANS_GRID_BLOCKING_SIZE", 1ul);
    parameters.replaceTextAttr("KERNEL_STORE_DATA", "array");
    parameters.replaceIDAttr("KERNEL_MAX_DIM_UNROLL", 10ul);

    SGPP::datadriven::OperationMultipleEvalConfiguration configuration(
    SGPP::datadriven::OperationMultipleEvalType::STREAMING,
    SGPP::datadriven::OperationMultipleEvalSubType::OCL, parameters);

    for (std::tuple<std::string, double> fileNameError : fileNamesError) {
        double mse = compareToReference(SGPP::base::GridType::Linear, std::get<0>(fileNameError), level, configuration);
        BOOST_CHECK(mse < std::get<1>(fileNameError));
    }
}

BOOST_AUTO_TEST_SUITE_END()

#endif