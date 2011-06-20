/******************************************************************************
* Copyright (C) 2011 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
******************************************************************************/
// @author Alexander Heinecke (Alexander.Heinecke@mytum.de)

#ifndef OPERATIONMULTIPLEEVALITERATIVESPSSEMODLINEAR_HPP
#define OPERATIONMULTIPLEEVALITERATIVESPSSEMODLINEAR_HPP

#include "operation/datadriven/OperationMultipleEvalVectorizedSP.hpp"
#include "grid/GridStorage.hpp"
#include "tools/common/SGppStopwatch.hpp"

namespace sg
{
namespace parallel
{

/**
 * This class implements sg::base::OperationMultipleEval for a grids with modified linear basis ansatzfunctions without boundaries
 *
 * However in this case high efficient vector code (SSE3 instructions) are generated
 * to implement a iterative OperationB version. In addition cache blocking is used
 * in order to assure a most efficient cache usage.
 *
 * IMPORTANT REMARK:
 * In order to use this routine you have to keep following points in mind (for multVectorized and multTransposeVectorized):
 * @li data MUST a have even number of points AND it must be transposed
 * @li result MUST have the same size as data points that should be evaluated
 */
class OperationMultipleEvalIterativeSPSSEModLinear : public sg::base::OperationMultipleEvalVectorizedSP
{
public:
	/**
	 * Within the constructor sg::base::DataMatrix Level and sg::base::DataMatrix Index are set up.
	 * If the grid changes during your calculations and you don't want to create
	 * a new instance of this class, you have to call rebuildLevelAndIndex before
	 * doing any further mult or multTranspose calls.
	 *
	 * @param storage Pointer to the grid's gridstorage obejct
	 * @param dataset dataset that should be evaluated
	 */
	OperationMultipleEvalIterativeSPSSEModLinear(sg::base::GridStorage* storage, sg::base::DataMatrixSP* dataset);

	/**
	 * Destructor
	 */
	virtual ~OperationMultipleEvalIterativeSPSSEModLinear();

	virtual double multVectorized(sg::base::DataVectorSP& alpha, sg::base::DataVectorSP& result);

	virtual double multTransposeVectorized(sg::base::DataVectorSP& source, sg::base::DataVectorSP& result);

	virtual void rebuildLevelAndIndex();

protected:
	/// Pointer to the grid's gridstorage object
	sg::base::GridStorage* storage;
	/// Timer object to handle time measurements
	sg::base::SGppStopwatch* myTimer;
};

}

}

#endif /* OPERATIONMULTIPLEEVALITERATIVESPSSEMODLINEAR_HPP */