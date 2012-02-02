/******************************************************************************
* Copyright (C) 2010 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
******************************************************************************/
// @author Alexander Heinecke (Alexander.Heinecke@mytum.de)

#include "sgpp_base.hpp"
#include "sgpp_pde.hpp"
#include "sgpp_finance.hpp"
#include "sgpp_parallel.hpp"
#include "sgpp_solver.hpp"
#include "sgpp_datadriven.hpp"
#include "base/datatypes/DataVectorSP.hpp"
#include "base/datatypes/DataMatrixSP.hpp"
#include "parallel/datadriven/algorithm/DMSystemMatrixSPVectorizedIdentity.hpp"
#include "parallel/datadriven/algorithm/DMSystemMatrixVectorizedIdentity.hpp"
#include "solver/sle/ConjugateGradientsSP.hpp"
#include "datadriven/tools/ARFFTools.hpp"
#include "base/grid/type/ModLinearGrid.hpp"
#include "base/grid/type/LinearGrid.hpp"
#include "base/grid/type/LinearTrapezoidBoundaryGrid.hpp"
#include "datadriven/operation/DatadrivenOpFactory.hpp"

#include <string>
#include <iostream>
#include <cstdlib>
#include <fstream>

// print grid in gnuplot readable format (1D and 2D only)
//#define GNUPLOT
//#define GRDIRESOLUTION 100

// Use fast iterative methods
#define ITERATIVE

// do Test only after last refinement
#define TEST_LAST_ONLY

// store ROC curve
//#define STORE_ROC_CURVE
#define ROC_POINTS 40

bool bUseFloat;

void storeROCcurve(sg::base::DataMatrix& ROC_curve, std::string tFilename)
{
	std::ofstream fileout;

	// Open filehandle
	fileout.open(tFilename.c_str());

	// plot values
	for (size_t i = 0; i < ROC_curve.getNrows(); i++)
	{
		fileout <<  ROC_curve.get(i, 0) << " " << ROC_curve.get(i, 1) << std::endl;
	}

	// close filehandle
	fileout.close();
}

void calcGFlopsAndGBytes(std::string gridtype, sg::base::Grid* myGrid, size_t nInstancesNo, size_t nGridsize, size_t nDim, size_t nIterations, size_t datatype_size, double& GFlops, double& GBytes)
{
	if (gridtype == "modlinear")
	{
		for (size_t g = 0; g < nGridsize; g++)
		{
			sg::base::GridIndex* curPoint = myGrid->getStorage()->get(g);

			for (size_t h = 0; h < nDim; h++)
			{
				unsigned int level, index;

				curPoint->get(h, level, index);

				if (level == 1)
				{
				}
				else if (index == 1)
				{
					GFlops += 1e-9*8.0*static_cast<double>(nIterations)*static_cast<double>(nInstancesNo);
					GBytes += 1e-9*4.0*static_cast<double>(nIterations)*static_cast<double>(datatype_size)*static_cast<double>(nInstancesNo);
				}
				else if (index == ((1<<level) - 1))
				{
					GFlops += 1e-9*10.0*static_cast<double>(nIterations)*static_cast<double>(nInstancesNo);
					GBytes += 1e-9*6.0*static_cast<double>(nIterations)*static_cast<double>(datatype_size)*static_cast<double>(nInstancesNo);
				}
				else
				{
					GFlops += 1e-9*12.0*static_cast<double>(nIterations)*static_cast<double>(nInstancesNo);
					GBytes += 1e-9*6.0*static_cast<double>(nIterations)*static_cast<double>(datatype_size)*static_cast<double>(nInstancesNo);
				}
			}
		}

		// GBytes for EvalTrans (coefficients)
		GBytes += 1e-9*static_cast<double>(nIterations)
				*((static_cast<double>(nGridsize)*static_cast<double>(nInstancesNo+1)*static_cast<double>(datatype_size)));

		// GBytes for Eval (coefficients)
		GBytes += 1e-9*static_cast<double>(nIterations)
				*((static_cast<double>(nGridsize+1)*static_cast<double>(nInstancesNo)*static_cast<double>(datatype_size)));
	}
	else
	{
		// GFlops
		GFlops += 2.0*1e-9*static_cast<double>(nGridsize)*static_cast<double>(nInstancesNo)*static_cast<double>(nDim)*6.0*static_cast<double>(nIterations);

		// GBytes
		GBytes += 2.0*1e-9*static_cast<double>(nGridsize)*static_cast<double>(nInstancesNo)*static_cast<double>(nDim)*3.0*static_cast<double>(nIterations)*static_cast<double>(datatype_size);

		// GBytes for EvalTrans (coefficients)
		GBytes += 1e-9*static_cast<double>(nIterations)
				*((static_cast<double>(nGridsize)*static_cast<double>(nInstancesNo)*static_cast<double>(datatype_size)));

		// GBytes for Eval (coefficients)
		GBytes += 1e-9*static_cast<double>(nIterations)
				*((static_cast<double>(nGridsize)*static_cast<double>(nInstancesNo)*static_cast<double>(datatype_size)));
	}
}

void printSettings(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		double lambda, size_t cg_max, double cg_eps, size_t refine_count, double refine_thresh, size_t refine_points, std::string gridtype,
		 std::string vectorization, size_t gridsize = 0, double finaltr = 0.0, double finalte = 0.0, double time = 0.0)
{
	std::cout << std::endl;
	std::cout << "Train dataset: " << dataFile << std::endl;
	std::cout << "Test dataset: " << testFile << std::endl;
	std::cout << "Startlevel: " << start_level << std::endl << std::endl;

	std::cout << "Num. Refinements: " << refine_count << std::endl;
	std::cout << "Refine Threshold: " << refine_thresh << std::endl;
	std::cout << "Refine number points: " << refine_points << std::endl << std::endl;

	std::cout << "Max. CG Iterations: " << cg_max << std::endl;
	std::cout << "CG epsilon: " << cg_eps << std::endl << std::endl;

	std::cout << "Lambda: " << lambda << std::endl << std::endl;

	if (bUseFloat)
	{
		std::cout << "Precision: Single Precision (float)" << std::endl << std::endl;
	}
	else
	{
		std::cout << "Precision: Double Precision (double)" << std::endl << std::endl;
	}

	if (vectorization == "X86SIMD")
	{
#if defined(__SSE3__) && !defined(__AVX__)
		std::cout << "Vectorized: X86SIMD (SSE3)" << std::endl << std::endl;
#endif
#if defined(__SSE3__) && defined(__AVX__)
		std::cout << "Vectorized: X86SIMD (AVX)" << std::endl << std::endl;
#endif
	}
	else if (vectorization == "OCL")
	{
		std::cout << "Vectorized: OpenCL (NVIDIA Fermi optimized)" << std::endl << std::endl;
	}
	else if (vectorization == "HYBRID_X86SIMD_OCL")
	{
#if defined(__SSE3__) && !defined(__AVX__)
		std::cout << "Vectorized: Hybrid, SSE3 and OpenCL (NVIDIA Fermi optimized)" << std::endl << std::endl;
#endif
#if defined(__SSE3__) && defined(__AVX__)
		std::cout << "Vectorized: Hybrid, AVX and OpenCL (NVIDIA Fermi optimized)" << std::endl << std::endl;
#endif
	}
	else if (vectorization == "ArBB")
	{
		std::cout << "Vectorized: Intel Array Building Blocks" << std::endl << std::endl;
	}
	else
	{
		std::cout << "Scalar Version" << std::endl << std::endl;
	}

	if (isRegression)
	{
		std::cout << "Mode: Regression" << std::endl << std::endl;
	}
	else
	{
		std::cout << "Mode: Classification" << std::endl << std::endl;
	}

	std::cout << "chosen gridtype: " << gridtype << std::endl << std::endl;

	if (gridsize > 0)
	{
		std::cout << "$" << dataFile << ";" << testFile << ";" << isRegression << ";" << bUseFloat << ";" << gridtype << ";" << start_level
		<< ";" << lambda << ";" << cg_max << ";" << cg_eps << ";" << refine_count << ";"  << refine_thresh
		<< ";" << refine_points << ";" << gridsize << ";" << finaltr << ";" << finalte << ";" << time << std::endl << std::endl;
	}
}

void adaptClassificationTest(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		double lambda, size_t cg_max, double cg_eps, size_t refine_count, double refine_thresh, size_t refine_points,
		std::string gridtype, size_t cg_max_learning, double cg_eps_learning, std::string vectorization)
{
    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
#ifdef ITERATIVE
    std::cout << "Classification Test App (Double Precision)" << std::endl;
#else
    std::cout << "Classification Test App (Double Precision, recursive)" << std::endl;
#endif

    std::cout << "===============================================================" << std::endl << std::endl;

    printSettings(dataFile, testFile, isRegression, start_level,
			lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points, gridtype, vectorization);

    double execTime = 0.0;
    double acc = 0.0;
    double accTest = 0.0;

    double GFlops = 0.0;
    double GBytes = 0.0;

	sg::datadriven::ARFFTools ARFFTool;
	std::string tfileTrain = dataFile;
	std::string tfileTest = testFile;

	size_t nDim = ARFFTool.getDimension(tfileTrain);
	size_t nInstancesNo = ARFFTool.getNumberInstances(tfileTrain);
	size_t nInstancesTestNo = ARFFTool.getNumberInstances(tfileTest);
	size_t nGridsize = 0;

	std::cout << std::endl << "Dims: " << nDim << "; Traininstances: " << nInstancesNo << "; Testinstances: " << nInstancesTestNo << std::endl << std::endl;

	// Create Grid
	sg::base::Grid* myGrid;
	if (gridtype == "linearboundary")
	{
		myGrid = new sg::base::LinearTrapezoidBoundaryGrid(nDim);
	}
	else if (gridtype == "modlinear")
	{
		myGrid = new sg::base::ModLinearGrid(nDim);
	}
	else if (gridtype == "linear")
	{
		myGrid = new sg::base::LinearGrid(nDim);
	}
	else
	{
		std::cout << std::endl << "An unsupported grid type was chosen! Exiting...." << std::endl << std::endl;
		myGrid = NULL;
		return;
	}

	// Generate regular Grid with LEVELS Levels
	sg::base::GridGenerator* myGenerator = myGrid->createGridGenerator();
	myGenerator->regular(start_level);
	delete myGenerator;

	// Define DP data
	sg::base::DataMatrix data(nInstancesNo, nDim);
	sg::base::DataVector classes(nInstancesNo);
	sg::base::DataMatrix testData(nInstancesTestNo, nDim);
	sg::base::DataVector testclasses(nInstancesTestNo);
	sg::base::DataVector result(nInstancesNo);
	sg::base::DataVector alpha(myGrid->getSize());

	// Read data from file
    ARFFTool.readTrainingData(tfileTrain, data);
    ARFFTool.readTrainingData(tfileTest, testData);
    ARFFTool.readClasses(tfileTrain, classes);
    ARFFTool.readClasses(tfileTest, testclasses);

    result.setAll(0.0);
    alpha.setAll(0.0);

    // Variable to save MSE/Acc from former iteration
#ifndef TEST_LAST_ONLY
    double oldAcc = 0.0;
#endif

    // Generate CG to solve System
    sg::solver::ConjugateGradients* myCG = new sg::solver::ConjugateGradients(cg_max_learning, cg_eps_learning);

#ifdef ITERATIVE
    sg::parallel::DMSystemMatrixVectorizedIdentity* mySystem = new sg::parallel::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, sg::parallel::X86SIMD);
#else
    sg::base::OperationMatrix* myC = sg::op_factory::createOperationIdentity(*myGrid);
    //sg::OperationMatrix* myC = myGrid->createOperationLaplace();
    sg::datadriven::DMSystemMatrix* mySystem = new sg::datadriven::DMSystemMatrix(*myGrid, data, *myC, lambda);
#endif

    std::cout << "Starting Learning...." << std::endl;

    // execute adaptsteps
    sg::base::SGppStopwatch* myStopwatch = new sg::base::SGppStopwatch();
    for (size_t i = 0; i < refine_count+1; i++)
    {
    	std::cout << std::endl << "Doing refinement: " << i << std::endl;

    	myStopwatch->start();

    	// Do Refinements
    	if (i > 0)
    	{
    		sg::base::SurplusRefinementFunctor* myRefineFunc = new sg::base::SurplusRefinementFunctor(&alpha, refine_points, refine_thresh);
    		myGrid->createGridGenerator()->refine(myRefineFunc);
    		delete myRefineFunc;

#ifdef ITERATIVE
    		mySystem->rebuildLevelAndIndex();
#endif

    		std::cout << "New Grid Size: " << myGrid->getStorage()->size() << std::endl;
    		alpha.resizeZero(myGrid->getStorage()->size());
    	}
    	else
    	{
    		std::cout << "Grid Size: " << myGrid->getStorage()->size() << std::endl;
    	}

    	sg::base::DataVector b(alpha.getSize());
    	mySystem->generateb(classes, b);

    	if (i == refine_count)
    	{
    		myCG->setMaxIterations(cg_max);
    		myCG->setEpsilon(cg_eps);
    	}
    	myCG->solve(*mySystem, alpha, b, true, false, 0.0);

        execTime += myStopwatch->stop();

    	std::cout << "Needed Iterations: " << myCG->getNumberIterations() << std::endl;
    	std::cout << "Final residuum: " << myCG->getResiduum() << std::endl;

    	// Calc flops and mem bandwidth
    	nGridsize = myGrid->getStorage()->size();

    	calcGFlopsAndGBytes(gridtype, myGrid, nInstancesNo, nGridsize, nDim, myCG->getNumberIterations(), sizeof(double), GFlops, GBytes);

    	std::cout << std::endl;
        std::cout << "Current GFlop/s: " << GFlops/execTime << std::endl;
        std::cout << "Current GByte/s: " << GBytes/execTime << std::endl;
        std::cout << std::endl;

#ifndef TEST_LAST_ONLY
		// Do tests on test data
    	if (isRegression)
    	{
    		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
			acc = myTest->testMSE(alpha, data, classes);
			std::cout << "MSE (train): " << acc << std::endl;
			accTest = myTest->testMSE(alpha, testData, testclasses);
			std::cout << "MSE (test): " << accTest << std::endl << std::endl;
			delete myTest;

			if (((i > 0) && (oldAcc <= accTest)) || accTest == 0.0)
			{
				std::cout << "The grid is becoming worse --> stop learning" << std::endl;
				break;
			}

			oldAcc = accTest;
    	}
    	else
    	{
    		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
    		sg::base::DataVector charNumbers(4);
    		acc = myTest->testWithCharacteristicNumber(alpha, data, classes, charNumbers);
    		acc /= static_cast<double>(classes.getSize());
    		std::cout << "train accuracy: " << acc << std::endl;
    		std::cout << "train sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
    		std::cout << "train specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
    		std::cout << "train precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
    		std::cout << "train true positives: " << charNumbers[0] << std::endl;
    		std::cout << "train true negatives: " << charNumbers[1] << std::endl;
    		std::cout << "train false positives: " << charNumbers[2] << std::endl;
    		std::cout << "train false negatives: " << charNumbers[3] << std::endl << std::endl;
    		accTest = myTest->testWithCharacteristicNumber(alpha, testData, testclasses, charNumbers);
    		accTest /= static_cast<double>(testclasses.getSize());
    		std::cout << "test accuracy: " << accTest << std::endl;
    		std::cout << "test sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
    		std::cout << "test specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
    		std::cout << "test precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
    		std::cout << "test true positives: " << charNumbers[0] << std::endl;
    		std::cout << "test true negatives: " << charNumbers[1] << std::endl;
    		std::cout << "test false positives: " << charNumbers[2] << std::endl;
    		std::cout << "test false negatives: " << charNumbers[3] << std::endl << std::endl;
#ifdef STORE_ROC_CURVE
    		std::cout << "calculating ROC curves ..." << std::endl;
    		sg::base::DataVector thresholds(ROC_POINTS+1);
    		double gap = 2.0/((double)ROC_POINTS);
    		for (size_t t = 0; t < ROC_POINTS+1; t++)
    		{
    			thresholds.set(t, 1.0-(t*gap));
    		}
    		sg::base::DataMatrix ROC_train(ROC_POINTS+1, 2);
    		sg::base::DataMatrix ROC_test(ROC_POINTS+1, 2);
    		myTest->calculateROCcurve(alpha, data, classes, thresholds, ROC_train);
    		myTest->calculateROCcurve(alpha, testData, testclasses, thresholds, ROC_test);
    		std::cout << "calculating ROC curves done!" << std::endl << std::endl;
    		std::stringstream filetrain;
    		filetrain << tfileTrain << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
    		std::stringstream filetest;
    		filetest << tfileTest << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
        	storeROCcurve(ROC_train, filetrain.str());
        	storeROCcurve(ROC_test, filetest.str());
#endif
    		delete myTest;

			if (((i > 0) && (oldAcc >= accTest)) || accTest == 1.0)
			{
				std::cout << "The grid is becoming worse --> stop learning" << std::endl;
				break;
			}

			oldAcc = accTest;
    	}
#endif
    }

    delete myStopwatch;

    std::cout << "Finished Learning!" << std::endl;

#ifdef TEST_LAST_ONLY
    std::cout << std::endl << std::endl;
    if (isRegression)
	{
		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
		acc = myTest->testMSE(alpha, data, classes);
		std::cout << "MSE (train): " << acc << std::endl;
		accTest = myTest->testMSE(alpha, testData, testclasses);
		std::cout << "MSE (test): " << accTest << std::endl << std::endl;
		delete myTest;
	}
	else
	{
		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
		sg::base::DataVector charNumbers(4);
		acc = myTest->testWithCharacteristicNumber(alpha, data, classes, charNumbers);
		acc /= static_cast<double>(classes.getSize());
		std::cout << "train accuracy: " << acc << std::endl;
		std::cout << "train sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
		std::cout << "train specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
		std::cout << "train precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
		std::cout << "train true positives: " << charNumbers[0] << std::endl;
		std::cout << "train true negatives: " << charNumbers[1] << std::endl;
		std::cout << "train false positives: " << charNumbers[2] << std::endl;
		std::cout << "train false negatives: " << charNumbers[3] << std::endl << std::endl;
		accTest = myTest->testWithCharacteristicNumber(alpha, testData, testclasses, charNumbers);
		accTest /= static_cast<double>(testclasses.getSize());
		std::cout << "test accuracy: " << accTest << std::endl;
		std::cout << "test sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
		std::cout << "test specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
		std::cout << "test precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
		std::cout << "test true positives: " << charNumbers[0] << std::endl;
		std::cout << "test true negatives: " << charNumbers[1] << std::endl;
		std::cout << "test false positives: " << charNumbers[2] << std::endl;
		std::cout << "test false negatives: " << charNumbers[3] << std::endl << std::endl;
#ifdef STORE_ROC_CURVE
		std::cout << "calculating ROC curves ..." << std::endl;
		sg::base::DataVector thresholds(ROC_POINTS+1);
		double gap = 2.0/((double)ROC_POINTS);
		for (size_t t = 0; t < ROC_POINTS+1; t++)
		{
			thresholds.set(t, 1.0-(t*gap));
		}
		sg::base::DataMatrix ROC_train(ROC_POINTS+1, 2);
		sg::base::DataMatrix ROC_test(ROC_POINTS+1, 2);
		myTest->calculateROCcurve(alpha, data, classes, thresholds, ROC_train);
		myTest->calculateROCcurve(alpha, testData, testclasses, thresholds, ROC_test);
		std::cout << "calculating ROC curves done!" << std::endl << std::endl;
		std::stringstream filetrain;
		filetrain << tfileTrain << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
		std::stringstream filetest;
		filetest << tfileTest << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
    	storeROCcurve(ROC_train, filetrain.str());
    	storeROCcurve(ROC_test, filetest.str());
#endif
    	delete myTest;
	}
#endif

#ifdef GNUPLOT
	if (nDim <= 2)
	{
		sg::base::GridPrinter* myPrinter = new sg::base::GridPrinter(*myGrid);
		myPrinter->printGrid(alpha, "ClassifyBenchmark.gnuplot", GRDIRESOLUTION);
		delete myPrinter;
	}
#endif

    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
    printSettings(dataFile, testFile, isRegression, start_level,
			lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points, gridtype, vectorization, myGrid->getSize(), acc, accTest, execTime);
#ifdef ITERATIVE
    std::cout << "Needed time: " << execTime << " seconds (Double Precision)" << std::endl;
    std::cout << std::endl << "Timing Details:" << std::endl;
    double computeMult, completeMult, computeMultTrans, completeMultTrans;
    mySystem->getTimers(completeMult, computeMult, completeMultTrans, computeMultTrans);
    std::cout << "         mult (complete): " << completeMult << " seconds" << std::endl;
    std::cout << "         mult (compute) : " << computeMult << " seconds" << std::endl;
    std::cout << "  mult trans. (complete): " << completeMultTrans << " seconds" << std::endl;
    std::cout << "  mult trans. (compute) : " << computeMultTrans << " seconds" << std::endl;
    std::cout << std::endl << std::endl;
    std::cout << "GFlop/s: " << GFlops/execTime << std::endl;
    std::cout << "GByte/s: " << GBytes/execTime << std::endl;
#else
    std::cout << "Needed time: " << execTime << " seconds (Double Precision, recursive)" << std::endl;
#endif
    std::cout << "===============================================================" << std::endl;
    std::cout << std::endl;

#ifndef ITERATIVE
    delete myC;
#endif
    delete myCG;
    delete mySystem;
    delete myGrid;
}

void adaptClassificationTestSP(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		float lambda, size_t cg_max, float cg_eps, size_t refine_count, double refine_thresh, size_t refine_points,
		std::string gridtype, size_t cg_max_learning, float cg_eps_learning, std::string vectorization)
{
    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
    std::cout << "Classification Test App (Single Precision)" << std::endl;
    std::cout << "===============================================================" << std::endl << std::endl;

    printSettings(dataFile, testFile, isRegression, start_level,
			(double)lambda, cg_max, (double)cg_eps, refine_count, refine_thresh, refine_points, gridtype, vectorization);

	double execTime = 0.0;
    double acc = 0.0;
    double accTest = 0.0;
    double GFlops = 0.0;
    double GBytes = 0.0;
	sg::datadriven::ARFFTools ARFFTool;
	std::string tfileTrain = dataFile;
	std::string tfileTest = testFile;

	size_t nDim = ARFFTool.getDimension(tfileTrain);
	size_t nInstancesNo = ARFFTool.getNumberInstances(tfileTrain);
	size_t nInstancesTestNo = ARFFTool.getNumberInstances(tfileTest);
	size_t nGridsize = 0;

	std::cout << std::endl << "Dims: " << nDim << "; Traininstances: " << nInstancesNo << "; Testinstances: " << nInstancesTestNo << std::endl << std::endl;

	// Create Grid
	sg::base::Grid* myGrid;
	if (gridtype == "linearboundary")
	{
		myGrid = new sg::base::LinearTrapezoidBoundaryGrid(nDim);
	}
	else if (gridtype == "modlinear")
	{
		myGrid = new sg::base::ModLinearGrid(nDim);
	}
	else if (gridtype == "linear")
	{
		myGrid = new sg::base::LinearGrid(nDim);
	}
	else
	{
		std::cout << std::endl << "An unsupported grid type was chosen! Exiting...." << std::endl << std::endl;
		myGrid = NULL;
		return;
	}

	// Generate regular Grid with LEVELS Levels
	sg::base::GridGenerator* myGenerator = myGrid->createGridGenerator();
	myGenerator->regular(start_level);
	delete myGenerator;

	// Define DP data
	sg::base::DataMatrix data(nInstancesNo, nDim);
	sg::base::DataVector classes(nInstancesNo);
	sg::base::DataMatrix testData(nInstancesTestNo, nDim);
	sg::base::DataVector testclasses(nInstancesTestNo);
	sg::base::DataVector result(nInstancesNo);
	sg::base::DataVector alpha(myGrid->getSize());

    // Define SP data
	sg::base::DataMatrixSP dataSP(nInstancesNo, nDim);
	sg::base::DataVectorSP classesSP(nInstancesNo);
	sg::base::DataVectorSP resultSP(nInstancesNo);
	sg::base::DataVectorSP alphaSP(myGrid->getSize());

	// Read data from file
    ARFFTool.readTrainingData(tfileTrain, data);
    ARFFTool.readTrainingData(tfileTest, testData);
    ARFFTool.readClasses(tfileTrain, classes);
    ARFFTool.readClasses(tfileTest, testclasses);

    result.setAll(0.0);
    alpha.setAll(0.0);

    sg::base::PrecisionConverter::convertDataMatrixToDataMatrixSP(data, dataSP);
    sg::base::PrecisionConverter::convertDataVectorToDataVectorSP(alpha, alphaSP);
    sg::base::PrecisionConverter::convertDataVectorToDataVectorSP(classes, classesSP);
    sg::base::PrecisionConverter::convertDataVectorToDataVectorSP(result, resultSP);
    sg::base::PrecisionConverter::convertDataVectorToDataVectorSP(alpha, alphaSP);

    // Variable to save MSE/Acc from former iteration
#ifndef TEST_LAST_ONLY
    double oldAcc = 0.0;
#endif

    // Generate CG to solve System
    sg::solver::ConjugateGradientsSP* myCG = new sg::solver::ConjugateGradientsSP(cg_max_learning, cg_eps_learning);

    sg::parallel::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::parallel::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, sg::parallel::X86SIMD);

    std::cout << "Starting Learning...." << std::endl;
    // execute adaptsteps
    sg::base::SGppStopwatch* myStopwatch = new sg::base::SGppStopwatch();
    for (size_t i = 0; i < refine_count+1; i++)
    {
    	std::cout << std::endl << "Doing refinement: " << i << std::endl;

    	myStopwatch->start();
    	// Do Refinements
    	if (i > 0)
    	{
    		sg::base::PrecisionConverter::convertDataVectorSPToDataVector(alphaSP, alpha);
    		sg::base::SurplusRefinementFunctor* myRefineFunc = new sg::base::SurplusRefinementFunctor(&alpha, refine_points, refine_thresh);
    		myGrid->createGridGenerator()->refine(myRefineFunc);
    		delete myRefineFunc;

    		mySystem->rebuildLevelAndIndex();

    		std::cout << "New Grid Size: " << myGrid->getStorage()->size() << std::endl;
    		alpha.resizeZero(myGrid->getStorage()->size());
    		alphaSP.resizeZero(myGrid->getStorage()->size());
    	}
    	else
    	{
    		std::cout << "Grid Size: " << myGrid->getStorage()->size() << std::endl;
    	}

    	sg::base::DataVectorSP bSP(alphaSP.getSize());
    	mySystem->generateb(classesSP, bSP);

    	if (i == refine_count)
    	{
    		myCG->setMaxIterations(cg_max);
    		myCG->setEpsilon(cg_eps);
    	}
    	myCG->solve(*mySystem, alphaSP, bSP, true, false, 0.0);

    	execTime += myStopwatch->stop();

    	std::cout << "Needed Iterations: " << myCG->getNumberIterations() << std::endl;
    	std::cout << "Final residuum: " << myCG->getResiduum() << std::endl;

    	// Calc flops and mem bandwidth
    	nGridsize = myGrid->getStorage()->size();

    	calcGFlopsAndGBytes(gridtype, myGrid, nInstancesNo, nGridsize, nDim, myCG->getNumberIterations(), sizeof(float), GFlops, GBytes);

    	std::cout << std::endl;
        std::cout << "Current GFlop/s: " << GFlops/execTime << std::endl;
        std::cout << "Current GByte/s: " << GBytes/execTime << std::endl;
        std::cout << std::endl;

    	// Do tests on test data
#ifndef TEST_LAST_ONLY
        sg::base::PrecisionConverter::convertDataVectorSPToDataVector(alphaSP, alpha);
    	if (isRegression)
    	{
    		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
			acc = myTest->testMSE(alpha, data, classes);
			std::cout << "MSE (train): " << acc << std::endl;
			accTest = myTest->testMSE(alpha, testData, testclasses);
			std::cout << "MSE (test): " << accTest << std::endl << std::endl;
			delete myTest;

			if (((i > 0) && (oldAcc <= accTest)) || accTest == 0.0)
			{
				std::cout << "The grid is becoming worse --> stop learning" << std::endl;
				break;
			}

			oldAcc = accTest;
    	}
    	else
    	{
    		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
    		sg::base::DataVector charNumbers(4);
    		acc = myTest->testWithCharacteristicNumber(alpha, data, classes, charNumbers);
    		acc /= static_cast<double>(classes.getSize());
    		std::cout << "train accuracy: " << acc << std::endl;
    		std::cout << "train sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
    		std::cout << "train specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
    		std::cout << "train precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
    		std::cout << "train true positives: " << charNumbers[0] << std::endl;
    		std::cout << "train true negatives: " << charNumbers[1] << std::endl;
    		std::cout << "train false positives: " << charNumbers[2] << std::endl;
    		std::cout << "train false negatives: " << charNumbers[3] << std::endl << std::endl;
    		accTest = myTest->testWithCharacteristicNumber(alpha, testData, testclasses, charNumbers);
    		accTest /= static_cast<double>(testclasses.getSize());
    		std::cout << "test accuracy: " << accTest << std::endl;
    		std::cout << "test sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
    		std::cout << "test specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
    		std::cout << "test precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
    		std::cout << "test true positives: " << charNumbers[0] << std::endl;
    		std::cout << "test true negatives: " << charNumbers[1] << std::endl;
    		std::cout << "test false positives: " << charNumbers[2] << std::endl;
    		std::cout << "test false negatives: " << charNumbers[3] << std::endl << std::endl;
#ifdef STORE_ROC_CURVE
    		std::cout << "calculating ROC curves ..." << std::endl;
    		sg::base::DataVector thresholds(ROC_POINTS+1);
    		double gap = 2.0/((double)ROC_POINTS);
    		for (size_t t = 0; t < ROC_POINTS+1; t++)
    		{
    			thresholds.set(t, 1.0-(t*gap));
    		}
    		sg::base::DataMatrix ROC_train(ROC_POINTS+1, 2);
    		sg::base::DataMatrix ROC_test(ROC_POINTS+1, 2);
    		myTest->calculateROCcurve(alpha, data, classes, thresholds, ROC_train);
    		myTest->calculateROCcurve(alpha, testData, testclasses, thresholds, ROC_test);
    		std::cout << "calculating ROC curves done!" << std::endl << std::endl;
    		std::stringstream filetrain;
    		filetrain << tfileTrain << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
    		std::stringstream filetest;
    		filetest << tfileTest << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
        	storeROCcurve(ROC_train, filetrain.str());
        	storeROCcurve(ROC_test, filetest.str());
#endif
    		delete myTest;

			if (((i > 0) && (oldAcc >= accTest)) || accTest == 1.0)
			{
				std::cout << "The grid is becoming worse --> stop learning" << std::endl;
				break;
			}

			oldAcc = accTest;
    	}
#endif
    }

    delete myStopwatch;

    std::cout << "Finished Learning!" << std::endl;

#ifdef TEST_LAST_ONLY
    sg::base::PrecisionConverter::convertDataVectorSPToDataVector(alphaSP, alpha);
    std::cout << std::endl << std::endl;
    if (isRegression)
	{
		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
		acc = myTest->testMSE(alpha, data, classes);
		std::cout << "MSE (train): " << acc << std::endl;
		accTest = myTest->testMSE(alpha, testData, testclasses);
		std::cout << "MSE (test): " << accTest << std::endl << std::endl;
		delete myTest;
	}
	else
	{
		sg::datadriven::OperationTest* myTest = sg::op_factory::createOperationTest(*myGrid);
		sg::base::DataVector charNumbers(4);
		acc = myTest->testWithCharacteristicNumber(alpha, data, classes, charNumbers);
		acc /= static_cast<double>(classes.getSize());
		std::cout << "train accuracy: " << acc << std::endl;
		std::cout << "train sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
		std::cout << "train specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
		std::cout << "train precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
		std::cout << "train true positives: " << charNumbers[0] << std::endl;
		std::cout << "train true negatives: " << charNumbers[1] << std::endl;
		std::cout << "train false positives: " << charNumbers[2] << std::endl;
		std::cout << "train false negatives: " << charNumbers[3] << std::endl << std::endl;
		accTest = myTest->testWithCharacteristicNumber(alpha, testData, testclasses, charNumbers);
		accTest /= static_cast<double>(testclasses.getSize());
		std::cout << "test accuracy: " << accTest << std::endl;
		std::cout << "test sensitivity: " << charNumbers[0]/(charNumbers[0] + charNumbers[3]) << std::endl;
		std::cout << "test specificity: " << charNumbers[1]/(charNumbers[1] + charNumbers[2]) << std::endl;
		std::cout << "test precision: " << charNumbers[0]/(charNumbers[0] + charNumbers[2]) << std::endl << std::endl;
		std::cout << "test true positives: " << charNumbers[0] << std::endl;
		std::cout << "test true negatives: " << charNumbers[1] << std::endl;
		std::cout << "test false positives: " << charNumbers[2] << std::endl;
		std::cout << "test false negatives: " << charNumbers[3] << std::endl << std::endl;
#ifdef STORE_ROC_CURVE
		std::cout << "calculating ROC curves ..." << std::endl;
		sg::base::DataVector thresholds(ROC_POINTS+1);
		double gap = 2.0/((double)ROC_POINTS);
		for (size_t t = 0; t < ROC_POINTS+1; t++)
		{
			thresholds.set(t, 1.0-(t*gap));
		}
		sg::base::DataMatrix ROC_train(ROC_POINTS+1, 2);
		sg::base::DataMatrix ROC_test(ROC_POINTS+1, 2);
		myTest->calculateROCcurve(alpha, data, classes, thresholds, ROC_train);
		myTest->calculateROCcurve(alpha, testData, testclasses, thresholds, ROC_test);
		std::cout << "calculating ROC curves done!" << std::endl << std::endl;
		std::stringstream filetrain;
		filetrain << tfileTrain << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
		std::stringstream filetest;
		filetest << tfileTest << "_SP" << "_level_" << start_level << "_refines_" << refine_count << "_lambda_" << lambda << ".roc";
    	storeROCcurve(ROC_train, filetrain.str());
    	storeROCcurve(ROC_test, filetest.str());
#endif
    	delete myTest;
	}
#endif

#ifdef GNUPLOT
	if (nDim <= 2)
	{
		sg::base::PrecisionConverter::convertDataVectorSPToDataVector(alphaSP, alpha);
		sg::base::GridPrinter* myPrinter = new sg::base::GridPrinter(*myGrid);
		myPrinter->printGrid(alpha, "ClassifyBenchmark.gnuplot", GRDIRESOLUTION);
		delete myPrinter;
	}
#endif

    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
    printSettings(dataFile, testFile, isRegression, start_level,
			(double)lambda, cg_max, (double)cg_eps, refine_count, refine_thresh, refine_points, gridtype, vectorization, myGrid->getSize(), acc, accTest, execTime);
    std::cout << "Needed time: " << execTime << " seconds (Single Precision)" << std::endl;
    std::cout << std::endl << "Timing Details:" << std::endl;
    double computeMult, completeMult, computeMultTrans, completeMultTrans;
    mySystem->getTimers(completeMult, computeMult, completeMultTrans, computeMultTrans);
    std::cout << "         mult (complete): " << completeMult << " seconds" << std::endl;
    std::cout << "         mult (compute) : " << computeMult << " seconds" << std::endl;
    std::cout << "  mult trans. (complete): " << completeMultTrans << " seconds" << std::endl;
    std::cout << "  mult trans. (compute) : " << computeMultTrans << " seconds" << std::endl;
    std::cout << std::endl << std::endl;
    std::cout << "GFlop/s: " << GFlops/execTime << std::endl;
    std::cout << "GByte/s: " << GBytes/execTime << std::endl;
    std::cout << "===============================================================" << std::endl;
    std::cout << std::endl;

    delete myCG;
    delete mySystem;
    delete myGrid;
}

/**
 * Testapplication for the Intel VTune Profiling Tool
 * and a measurement app for Sparse Grid Algorithms building blocks
 */
int main(int argc, char *argv[])
{
	std::string dataFile;
	std::string testFile;
	std::string gridtype;
	std::string precision;
	std::string vectorization;

	double lambda = 0.0;
	double cg_eps = 0.0;
	double refine_thresh = 0.0;
	double cg_eps_learning = 0.0;

	size_t cg_max = 0;
	size_t refine_count = 0;
	size_t refine_points = 0;
	size_t start_level = 0;
	size_t cg_max_learning = 0;

	bool regression;

	if (argc != 16)
	{
		std::cout << std::endl;
		std::cout << "Help for classification/regression benchmark" << std::endl << std::endl;
		std::cout << "Needed parameters:" << std::endl;
		std::cout << "	Traindata-file" << std::endl;
		std::cout << "	Testdata-file" << std::endl;
		std::cout << "	regression (0/1)" << std::endl;
		std::cout << "	precision (SP,DP)" << std::endl;
		std::cout << "	gridtype (linear,linearboundary,modlinear)" << std::endl;
		std::cout << "	Startlevel" << std::endl;
		std::cout << "	lambda" << std::endl;
		std::cout << "	CG max. iterations" << std::endl;
		std::cout << "	CG epsilon" << std::endl;
		std::cout << "	#refinements" << std::endl;
		std::cout << "	Refinement threshold" << std::endl;
		std::cout << "	#points refined" << std::endl;
		std::cout << "	CG max. iterations, first refinement steps" << std::endl;
		std::cout << "	CG epsilon, first refinement steps" << std::endl;
		std::cout << "	Vectorization: X86SIMD, OCL, HYBRID_X86SIMD_OCL, ArBB" << std::endl << std::endl << std::endl;
		std::cout << "Example call:" << std::endl;
		std::cout << "	app.exe     test.data train.data 0 SP linearboundary 3 0.000001 250 0.0001 6 0.0 100 20 0.1 X86SIMD" << std::endl << std::endl << std::endl;
	}
	else
	{
		dataFile.assign(argv[1]);
		testFile.assign(argv[2]);
		regression = false;
		if (atoi(argv[3]) == 1)
		{
			regression = true;
		}
		precision.assign(argv[4]);
		gridtype.assign(argv[5]);
		start_level = atoi(argv[6]);
		lambda = atof(argv[7]);
		cg_max = atoi(argv[8]);
		cg_eps = atof(argv[9]);
		refine_count = atoi(argv[10]);
		refine_thresh = atof(argv[11]);
		refine_points = atoi(argv[12]);
		cg_max_learning = atoi(argv[13]);
		cg_eps_learning = atof(argv[14]);
		vectorization.assign(argv[15]);

		// Fallback
		if (vectorization != "X86SIMD" && vectorization != "OCL" && vectorization != "HYBRID_X86SIMD_OCL" && vectorization != "ArBB")
		{
			vectorization = "X86SIMD";
		}

		if (precision == "SP")
		{
			bUseFloat = true;
			adaptClassificationTestSP(dataFile, testFile, regression, start_level,
				(float)lambda, cg_max, (float)cg_eps, refine_count, refine_thresh, refine_points, gridtype, cg_max_learning, (float)cg_eps_learning, vectorization);
		}
		else if (precision == "DP")
		{
			bUseFloat = false;
			adaptClassificationTest(dataFile, testFile, regression, start_level,
				lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points, gridtype, cg_max_learning, cg_eps_learning, vectorization);
		}
		else
		{
			std::cout << "Unsupported precision type has been chosen! Existing...." << std::endl << std::endl;
		}
	}

	return 0;
}