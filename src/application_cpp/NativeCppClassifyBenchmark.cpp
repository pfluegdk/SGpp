/******************************************************************************
* Copyright (C) 2010 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
******************************************************************************/
// @author Alexander Heinecke (Alexander.Heinecke@mytum.de)

#include "sgpp.hpp"
#include "data/DataVectorSP.hpp"
#include "data/DataMatrixSP.hpp"
#include "algorithm/datadriven/DMSystemMatrixSPVectorizedIdentity.hpp"
#include "solver/sle/ConjugateGradientsSP.hpp"
#include "tools/datadriven/ARFFTools.hpp"
#include "basis/operations_factory.hpp"

#include <string>
#include <iostream>
#include <cstdlib>

// print grid in gnuplot readable format (1D and 2D only)
#define GNUPLOT
#define GRDIRESOLUTION 100

// at least one has to be defined, otherwise scalar&recursive version is used for DP, SSE for SP
#define USE_SSE
//#define USE_AVX
//#define USE_OCL
//#define USE_ARBB
//#define USE_HYBRID_SSE_OCL

// define if you want to use single precision floats (may deliver speed-up of 2 or greater),
// BUT: CG method may not converge because of bad system matrix condition.
#define USEFLOAT

// define this if you want to use grids with Neumann boundaries.
#define USE_BOUNDARIES

// do Test only after last refinement
#define TEST_LAST_ONLY

void convertDataVectorToDataVectorSP(DataVector& src, DataVectorSP& dest)
{
	if (src.getSize() != dest.getSize())
	{
		return;
	}
	else
	{
		for (size_t i = 0; i < src.getSize(); i++)
		{
			dest.set(i, static_cast<float>(src.get(i)));
		}
	}
}

void convertDataMatrixToDataMatrixSP(DataMatrix& src, DataMatrixSP& dest)
{
	if (src.getNcols() != dest.getNcols() || src.getNrows() != dest.getNrows())
	{
		return;
	}
	else
	{
		for (size_t i = 0; i < src.getNrows(); i++)
		{
			for (size_t j = 0; j < src.getNcols(); j++)
			{
				dest.set(i, j, static_cast<float>(src.get(i, j)));
			}
		}
	}
}

void convertDataVectorSPToDataVector(DataVectorSP& src, DataVector& dest)
{
	if (src.getSize() != dest.getSize())
	{
		return;
	}
	else
	{
		for (size_t i = 0; i < src.getSize(); i++)
		{
			dest.set(i, static_cast<double>(src.get(i)));
		}
	}
}

void printSettings(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		double lambda, size_t cg_max, double cg_eps, size_t refine_count, double refine_thresh, size_t refine_points,
		size_t gridsize = 0, double finaltr = 0.0, double finalte = 0.0, double time = 0.0)
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

	std::cout << "Lambda: " << lambda << std::endl;

#ifdef USE_SSE
	std::cout << "Vectorized: SSE" << std::endl << std::endl;
#endif
#ifdef USE_AVX
	std::cout << "Vectorized: AVX" << std::endl << std::endl;
#endif
#ifdef USE_OCL
	std::cout << "Vectorized: OpenCL (NVIDIA Fermi optimized)" << std::endl << std::endl;
#endif
#ifdef USE_HYBRID_SSE_OCL
	std::cout << "Vectorized: Hybrid, SSE and OpenCL (NVIDIA Fermi optimized)" << std::endl << std::endl;
#endif
#ifdef USE_ARBB
	std::cout << "Vectorized: Intel Array Building Blocks" << std::endl << std::endl;
#endif

	if (isRegression)
	{
		std::cout << "Mode: Regression" << std::endl << std::endl;
	}
	else
	{
		std::cout << "Mode: Classification" << std::endl << std::endl;
	}

#ifdef USE_BOUNDARIES
	std::cout << "Boundary-Mode: Neumann" << std::endl << std::endl;
	if (gridsize > 0)
	{
		std::cout << "$" << dataFile << ";" << testFile << ";Neumann;" << isRegression << ";" << start_level
		<< ";" << lambda << ";" << cg_max << ";" << cg_eps << ";" << refine_count << ";"  << refine_thresh
		<< ";" << refine_points << ";" << gridsize << ";" << finaltr << ";" << finalte << ";" << time << std::endl << std::endl;
	}
#else
	std::cout << "Boundary-Mode: Dirichlet-0" << std::endl << std::endl;
	if (gridsize > 0)
	{
		std::cout << "$" << dataFile << ";" << testFile << ";Dirichlet0;" << isRegression << ";" << start_level
		<< ";" << lambda << ";" << cg_max << ";" << cg_eps << ";" << refine_count << ";"  << refine_thresh
		<< ";" << refine_points << ";" << gridsize << ";" << finaltr << ";" << finalte << ";" << time << std::endl << std::endl;
	}
#endif
}

void adaptClassificationTest(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		double lambda, size_t cg_max, double cg_eps, size_t refine_count, double refine_thresh, size_t refine_points)
{
    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
    std::cout << "Classification Test App (Double Precision)" << std::endl;
#else
    std::cout << "Classification Test App (Double Precision, recursive)" << std::endl;
#endif
    std::cout << "===============================================================" << std::endl << std::endl;

    printSettings(dataFile, testFile, isRegression, start_level,
			lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points);

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
#ifdef USE_BOUNDARIES
	myGrid = new sg::base::LinearTrapezoidBoundaryGrid(nDim);
#else
	myGrid = new sg::base::LinearGrid(nDim);
#endif

	// Generate regular Grid with LEVELS Levels
	sg::base::GridGenerator* myGenerator = myGrid->createGridGenerator();
	myGenerator->regular(start_level);
	delete myGenerator;

	// Define DP data
	DataMatrix data(nInstancesNo, nDim);
    DataVector classes(nInstancesNo);
    DataMatrix testData(nInstancesTestNo, nDim);
    DataVector testclasses(nInstancesTestNo);
    DataVector result(nInstancesNo);
    DataVector alpha(myGrid->getSize());

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
    sg::solver::ConjugateGradients* myCG = new sg::solver::ConjugateGradients(cg_max, cg_eps);
#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
#ifdef USE_SSE
    sg::datadriven::DMSystemMatrixVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, "SSE");
#endif
#ifdef USE_AVX
    sg::datadriven::DMSystemMatrixVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, "AVX");
#endif
#ifdef USE_OCL
    sg::datadriven::DMSystemMatrixVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, "OCL");
#endif
#ifdef USE_HYBRID_SSE_OCL
    sg::datadriven::DMSystemMatrixVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, "HYBRID_SSE_OCL");
#endif
#ifdef USE_ARBB
    sg::datadriven::DMSystemMatrixVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixVectorizedIdentity(*myGrid, data, lambda, "ArBB");
#endif
#else
    sg::base::OperationMatrix* myC = sg::GridOperationFactory::createOperationIdentity(*myGrid);
    //sg::OperationMatrix* myC = myGrid->createOperationLaplace();
    sg::datadriven::DMSystemMatrix* mySystem = new sg::datadriven::DMSystemMatrix(*myGrid, data, *myC, lambda);
#endif

    std::cout << "Starting Learning...." << std::endl;

    // execute adaptsteps
    sg::base::SGppStopwatch* myStopwatch = new sg::base::SGppStopwatch();
    myStopwatch->start();
    for (size_t i = 0; i < refine_count+1; i++)
    {
    	std::cout << std::endl << "Doing refinement :" << i << std::endl;

    	// Do Refinements
    	if (i > 0)
    	{
    		sg::base::SurplusRefinementFunctor* myRefineFunc = new sg::base::SurplusRefinementFunctor(&alpha, refine_points, refine_thresh);
    		myGrid->createGridGenerator()->refine(myRefineFunc);
    		delete myRefineFunc;

#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
    		mySystem->rebuildLevelAndIndex();
#endif

    		std::cout << "New Grid Size: " << myGrid->getStorage()->size() << std::endl;
    		alpha.resizeZero(myGrid->getStorage()->size());
    	}
    	else
    	{
    		std::cout << "Grid Size: " << myGrid->getStorage()->size() << std::endl;
    	}

    	DataVector b(alpha.getSize());
    	mySystem->generateb(classes, b);

    	myCG->solve(*mySystem, alpha, b, true, false, 0.0);

    	std::cout << "Needed Iterations: " << myCG->getNumberIterations() << std::endl;
    	std::cout << "Final residuum: " << myCG->getResiduum() << std::endl;

    	// Calc flops and mem bandwidth
    	nGridsize = myGrid->getStorage()->size();
    	GFlops += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*(2.0
    			*((static_cast<double>(nDim)
    			*(static_cast<double>(nGridsize)
    			*static_cast<double>(nInstancesNo)))*6.0));

    	// GBytes for Eval
    	GBytes += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*((static_cast<double>(nInstancesNo)*2.0*static_cast<double>(nDim)*static_cast<double>(nGridsize)*static_cast<double>(sizeof(double)))
    				+(static_cast<double>(nInstancesNo)*static_cast<double>(nGridsize)*static_cast<double>(sizeof(double)))
    				+(static_cast<double>(nDim+1)*static_cast<double>(nInstancesNo)*static_cast<double>(sizeof(double))));

    	// GBytes for EvalTrans
    	GBytes += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*((static_cast<double>(nGridsize)*static_cast<double>(nDim+1)*static_cast<double>(nInstancesNo)*static_cast<double>(sizeof(double)))
    			+(static_cast<double>(nGridsize)*2.0*static_cast<double>(nDim)*static_cast<double>(sizeof(double))));


#ifndef TEST_LAST_ONLY
		// Do tests on test data
    	if (isRegression)
    	{
    		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
			acc = myTest->testMSE(alpha, data, classes);
			std::cout << "MSE (train): " << acc << std::endl;
			accTest = myTest->testMSE(alpha, testData, testclasses);
			std::cout << "MSE (test): " << accTest << std::endl;
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
    		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
			acc = myTest->test(alpha, data, classes);
			acc /= static_cast<double>(classes.getSize());
			std::cout << "train acc.: " << acc << std::endl;
			accTest = myTest->test(alpha, testData, testclasses);
			accTest /= static_cast<double>(testclasses.getSize());
			std::cout << "test acc.: " << accTest << std::endl;
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

    execTime = myStopwatch->stop();
    delete myStopwatch;

#ifdef TEST_LAST_ONLY
    std::cout << std::endl << std::endl;
    if (isRegression)
	{
		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
		acc = myTest->testMSE(alpha, data, classes);
		std::cout << "MSE (train): " << acc << std::endl;
		accTest = myTest->testMSE(alpha, testData, testclasses);
		std::cout << "MSE (test): " << accTest << std::endl;
		delete myTest;
	}
	else
	{
		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
		acc = myTest->test(alpha, data, classes);
		acc /= static_cast<double>(classes.getSize());
		std::cout << "train acc.: " << acc << std::endl;
		accTest = myTest->test(alpha, testData, testclasses);
		accTest /= static_cast<double>(testclasses.getSize());
		std::cout << "test acc.: " << accTest << std::endl;
		delete myTest;
	}

#endif

    std::cout << "Finished Learning!" << std::endl;
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
			lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points, myGrid->getSize(), acc, accTest, execTime);
#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
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

#ifndef USE_SSE
#ifndef USE_AVX
#ifndef USE_OCL
#ifndef USE_HYBRID_SSE_OCL
#ifndef USE_ARBB
    delete myC;
#endif
#endif
#endif
#endif
#endif
    delete myCG;
    delete mySystem;
    delete myGrid;
}

void adaptClassificationTestSP(std::string dataFile, std::string testFile, bool isRegression, size_t start_level,
		float lambda, size_t cg_max, float cg_eps, size_t refine_count, double refine_thresh, size_t refine_points)
{
    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
    std::cout << "Classification Test App (Single Precision)" << std::endl;
    std::cout << "===============================================================" << std::endl << std::endl;

    printSettings(dataFile, testFile, isRegression, start_level,
			(double)lambda, cg_max, (double)cg_eps, refine_count, refine_thresh, refine_points);

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
#ifdef USE_BOUNDARIES
	myGrid = new sg::base::LinearTrapezoidBoundaryGrid(nDim);
#else
	myGrid = new sg::base::LinearGrid(nDim);
#endif

	// Generate regular Grid with LEVELS Levels
	sg::base::GridGenerator* myGenerator = myGrid->createGridGenerator();
	myGenerator->regular(start_level);
	delete myGenerator;

	// Define DP data
	DataMatrix data(nInstancesNo, nDim);
    DataVector classes(nInstancesNo);
    DataMatrix testData(nInstancesTestNo, nDim);
    DataVector testclasses(nInstancesTestNo);
    DataVector result(nInstancesNo);
    DataVector alpha(myGrid->getSize());

    // Define SP data
	DataMatrixSP dataSP(nInstancesNo, nDim);
    DataVectorSP classesSP(nInstancesNo);
    DataVectorSP resultSP(nInstancesNo);
    DataVectorSP alphaSP(myGrid->getSize());

	// Read data from file
    ARFFTool.readTrainingData(tfileTrain, data);
    ARFFTool.readTrainingData(tfileTest, testData);
    ARFFTool.readClasses(tfileTrain, classes);
    ARFFTool.readClasses(tfileTest, testclasses);

    result.setAll(0.0);
    alpha.setAll(0.0);

    convertDataMatrixToDataMatrixSP(data, dataSP);
    convertDataVectorToDataVectorSP(alpha, alphaSP);
    convertDataVectorToDataVectorSP(classes, classesSP);
    convertDataVectorToDataVectorSP(result, resultSP);
    convertDataVectorToDataVectorSP(alpha, alphaSP);

    // Variable to save MSE/Acc from former iteration
#ifndef TEST_LAST_ONLY
    double oldAcc = 0.0;
#endif

    // Generate CG to solve System
    sg::solver::ConjugateGradientsSP* myCG = new sg::solver::ConjugateGradientsSP(cg_max, cg_eps);

#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
#ifdef USE_SSE
    sg::datadriven::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "SSE");
#endif
#ifdef USE_AVX
    sg::datadriven::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "AVX");
#endif
#ifdef USE_OCL
    sg::datadriven::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "OCL");
#endif
#ifdef USE_HYBRID_SSE_OCL
    sg::datadriven::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "HYBRID_SSE_OCL");
#endif
#ifdef USE_ARBB
    sg::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "ArBB");
#endif
#else
    sg::datadriven::DMSystemMatrixSPVectorizedIdentity* mySystem = new sg::datadriven::DMSystemMatrixSPVectorizedIdentity(*myGrid, dataSP, lambda, "SSE");
#endif

    std::cout << "Starting Learning...." << std::endl;
    // execute adaptsteps
    sg::base::SGppStopwatch* myStopwatch = new sg::base::SGppStopwatch();
    myStopwatch->start();
    for (size_t i = 0; i < refine_count+1; i++)
    {
    	std::cout << std::endl << "Doing refinement :" << i << std::endl;

    	// Do Refinements
    	if (i > 0)
    	{
    		convertDataVectorSPToDataVector(alphaSP, alpha);
    		sg::base::SurplusRefinementFunctor* myRefineFunc = new sg::base::SurplusRefinementFunctor(&alpha, refine_points, refine_thresh);
    		myGrid->createGridGenerator()->refine(myRefineFunc);
    		delete myRefineFunc;

#if defined(USE_SSE) || defined(USE_AVX) || defined(USE_OCL) || defined(USE_HYBRID_SSE_OCL) || defined(USE_ARBB)
    		mySystem->rebuildLevelAndIndex();
#endif

    		std::cout << "New Grid Size: " << myGrid->getStorage()->size() << std::endl;
    		alpha.resizeZero(myGrid->getStorage()->size());
    		alphaSP.resizeZero(myGrid->getStorage()->size());
    	}
    	else
    	{
    		std::cout << "Grid Size: " << myGrid->getStorage()->size() << std::endl;
    	}

    	DataVectorSP bSP(alphaSP.getSize());
    	mySystem->generateb(classesSP, bSP);

    	myCG->solve(*mySystem, alphaSP, bSP, true, false, 0.0);

    	std::cout << "Needed Iterations: " << myCG->getNumberIterations() << std::endl;
    	std::cout << "Final residuum: " << myCG->getResiduum() << std::endl;

    	// Calc flops and mem bandwidth
    	nGridsize = myGrid->getStorage()->size();
    	GFlops += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*(2.0
    			*((static_cast<double>(nDim)
    			*(static_cast<double>(nGridsize)
    			*static_cast<double>(nInstancesNo)))*6.0));

    	// GBytes for Eval
    	GBytes += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*((static_cast<double>(nInstancesNo)*2.0*static_cast<double>(nDim)*static_cast<double>(nGridsize)*static_cast<double>(sizeof(float)))
    				+(static_cast<double>(nInstancesNo)*static_cast<double>(nGridsize)*static_cast<double>(sizeof(float)))
    				+(static_cast<double>(nDim+1)*static_cast<double>(nInstancesNo)*static_cast<double>(sizeof(float))));

    	// GBytes for EvalTrans
    	GBytes += 1e-9*static_cast<double>(myCG->getNumberIterations())
    			*((static_cast<double>(nGridsize)*static_cast<double>(nDim+1)*static_cast<double>(nInstancesNo)*static_cast<double>(sizeof(float)))
    			+(static_cast<double>(nGridsize)*2.0*static_cast<double>(nDim)*static_cast<double>(sizeof(float))));

    	// Do tests on test data
#ifndef TEST_LAST_ONLY
    	convertDataVectorSPToDataVector(alphaSP, alpha);
    	if (isRegression)
    	{
    		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
			acc = myTest->testMSE(alpha, data, classes);
			std::cout << "MSE (train): " << acc << std::endl;
			accTest = myTest->testMSE(alpha, testData, testclasses);
			std::cout << "MSE (test): " << accTest << std::endl;
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
    		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
			acc = myTest->test(alpha, data, classes);
			acc /= static_cast<double>(classes.getSize());
			std::cout << "train acc.: " << acc << std::endl;
			accTest = myTest->test(alpha, testData, testclasses);
			accTest /= static_cast<double>(testclasses.getSize());
			std::cout << "test acc.: " << accTest << std::endl;
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

    execTime = myStopwatch->stop();
    delete myStopwatch;

#ifdef TEST_LAST_ONLY
	convertDataVectorSPToDataVector(alphaSP, alpha);
	std::cout << std::endl << std::endl;
	if (isRegression)
	{
		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
		acc = myTest->testMSE(alpha, data, classes);
		std::cout << "MSE (train): " << acc << std::endl;
		accTest = myTest->testMSE(alpha, testData, testclasses);
		std::cout << "MSE (test): " << accTest << std::endl;
		delete myTest;
	}
	else
	{
		sg::datadriven::OperationTest* myTest = sg::GridOperationFactory::createOperationTest(*myGrid);
		acc = myTest->test(alpha, data, classes);
		acc /= static_cast<double>(classes.getSize());
		std::cout << "train acc.: " << acc << std::endl;
		accTest = myTest->test(alpha, testData, testclasses);
		accTest /= static_cast<double>(testclasses.getSize());
		std::cout << "test acc.: " << accTest << std::endl;
		delete myTest;
	}
#endif

    std::cout << "Finished Learning!" << std::endl;

#ifdef GNUPLOT
	if (nDim <= 2)
	{
		convertDataVectorSPToDataVector(alphaSP, alpha);
		sg::base::GridPrinter* myPrinter = new sg::base::GridPrinter(*myGrid);
		myPrinter->printGrid(alpha, "ClassifyBenchmark.gnuplot", GRDIRESOLUTION);
//		myPrinter->printSparseGrid(alpha, "ClassifyBenchmark.gnuplot", false);
		delete myPrinter;
	}
#endif

    std::cout << std::endl;
    std::cout << "===============================================================" << std::endl;
    printSettings(dataFile, testFile, isRegression, start_level,
			(double)lambda, cg_max, (double)cg_eps, refine_count, refine_thresh, refine_points, myGrid->getSize(), acc, accTest, execTime);
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

	double lambda = 0.0;
	double cg_eps = 0.0;
	double refine_thresh = 0.0;

	size_t cg_max = 0.0;
	size_t refine_count = 0;
	size_t refine_points = 0;
	size_t start_level = 0;

	bool regression;

	if (argc != 11)
	{
		std::cout << std::endl;
		std::cout << "Help for classification/regression benchmark" << std::endl << std::endl;
		std::cout << "Needed parameters:" << std::endl;
		std::cout << "	Traindata-file" << std::endl;
		std::cout << "	Testdata-file" << std::endl;
		std::cout << "	regression (0/1)" << std::endl;
		std::cout << "	Startlevel" << std::endl;
		std::cout << "	lambda" << std::endl;
		std::cout << "	CG max. iterations" << std::endl;
		std::cout << "	CG threshold" << std::endl;
		std::cout << "	#refinements" << std::endl;
		std::cout << "	Refinement threshold" << std::endl;
		std::cout << "	#points refined" << std::endl << std::endl << std::endl;
		std::cout << "Example call:" << std::endl;
		std::cout << "	app.exe     test.data train.data 1 3 0.000001 250 0.0001 6 0.0 100" << std::endl << std::endl << std::endl;
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
		start_level = atoi(argv[4]);
		lambda = atof(argv[5]);
		cg_max = atoi(argv[6]);
		cg_eps = atof(argv[7]);
		refine_count = atoi(argv[8]);
		refine_thresh = atof(argv[9]);
		refine_points = atoi(argv[10]);

#ifdef USEFLOAT
		adaptClassificationTestSP(dataFile, testFile, regression, start_level,
				(float)lambda, cg_max, (float)cg_eps, refine_count, refine_thresh, refine_points);

#else
		adaptClassificationTest(dataFile, testFile, regression, start_level,
				lambda, cg_max, cg_eps, refine_count, refine_thresh, refine_points);
#endif
	}

	return 0;
}
