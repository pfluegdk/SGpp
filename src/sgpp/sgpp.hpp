/******************************************************************************
* Copyright (C) 2009 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
******************************************************************************/
// @author Dirk Pflueger (pflueged@in.tum.de), Jörg Blank (blankj@in.tum.de), Alexander Heinecke (Alexander.Heinecke@mytum.de), Sarpkan Selcuk (Sarpkan.Selcuk@mytum.de)

#ifndef SGPP_HPP_
#define SGPP_HPP_

#include "basis/linear/noboundary/LinearBasis.hpp"
#include "basis/linear/boundary/LinearBoundaryBasis.hpp"
#include "basis/linearstretched/noboundary/LinearStretchedBasis.hpp"
#include "basis/linearstretched/boundary/LinearStretchedBoundaryBasis.hpp"
#include "basis/modlinear/ModifiedLinearBasis.hpp"
#include "basis/poly/PolyBasis.hpp"
#include "basis/modpoly/ModifiedPolyBasis.hpp"
#include "basis/modwavelet/ModifiedWaveletBasis.hpp"
#include "basis/modbspline/ModifiedBsplineBasis.hpp"
#include "basis/prewavelet/PrewaveletBasis.hpp"

#include "grid/GridStorage.hpp"
#include "grid/GridDataBase.hpp"

#include "tools/common/OperationQuadratureMC.hpp"

#include "application/common/ScreenOutput.hpp"

#include "algorithm/datadriven/AlgorithmDGEMV.hpp"
#include "algorithm/datadriven/AlgorithmMultipleEvaluation.hpp"
#include "algorithm/common/GetAffectedBasisFunctions.hpp"
#include "algorithm/common/AlgorithmEvaluation.hpp"
#include "algorithm/common/AlgorithmEvaluationTransposed.hpp"

#include "algorithm/datadriven/test_dataset.hpp"
#include "algorithm/datadriven/DMSystemMatrix.hpp"
#include "algorithm/datadriven/DMSystemMatrixVectorizedIdentity.hpp"
#include "algorithm/datadriven/DMWeightMatrix.hpp"
#include "algorithm/datadriven/AlgorithmAdaBoost.hpp"

#include "algorithm/datadriven/DensitySystemMatrix.hpp"

#include "algorithm/pde/BlackScholesParabolicPDESolverSystem.hpp"
#include "algorithm/pde/ModifiedBlackScholesParabolicPDESolverSystem.hpp"
#include "algorithm/pde/HullWhiteParabolicPDESolverSystem.hpp"
#include "algorithm/pde/BlackScholesParabolicPDESolverSystemEuroAmer.hpp"
#include "algorithm/pde/BlackScholesParabolicPDESolverSystemEuroAmerParallelOMP.hpp"
#include "algorithm/pde/HeatEquationParabolicPDESolverSystem.hpp"
#include "algorithm/pde/PoissonEquationEllipticPDESolverSystemDirichlet.hpp"

#include "application/pde/BlackScholesSolver.hpp"
#include "application/pde/BlackScholesSolverWithStretching.hpp"
#include "application/pde/HullWhiteSolver.hpp"
#include "application/pde/BlackScholesHullWhiteSolver.hpp"
#include "application/pde/HeatEquationSolver.hpp"
#include "application/pde/HeatEquationSolverWithStretching.hpp"
#include "application/pde/LaserHeatEquationSolver2D.hpp"
#include "application/pde/PoissonEquationSolver.hpp"




// @todo (heinecke) check if this can be removed
#include "basis/linear/noboundary/operation/pde/OperationLaplaceLinear.hpp"
#include "basis/linear/boundary/operation/pde/OperationLaplaceLinearBoundary.hpp"
#include "basis/linearstretched/noboundary/operation/pde/OperationLaplaceLinearStretched.hpp"
#include "basis/linearstretched/boundary/operation/pde/OperationLaplaceLinearStretchedBoundary.hpp"
#include "basis/modlinear/operation/pde/OperationLaplaceModLinear.hpp"

#include "data/DataVector.hpp"
#include "data/DataMatrix.hpp"

#include "grid/Grid.hpp"
#include "grid/common/BoundingBox.hpp"
#include "grid/common/Stretching.hpp"
#include "grid/common/DirichletUpdateVector.hpp"
#include "grid/generation/RefinementFunctor.hpp"
#include "grid/generation/CoarseningFunctor.hpp"
#include "grid/generation/StandardGridGenerator.hpp"
#include "grid/generation/BoundaryGridGenerator.hpp"
#include "grid/generation/TrapezoidBoundaryGridGenerator.hpp"
#include "grid/generation/StretchedTrapezoidBoundaryGridGenerator.hpp"
#include "grid/generation/SquareRootGridGenerator.hpp"
#include "grid/generation/TruncatedTrapezoidGridGenerator.hpp"
#include "grid/generation/GridGenerator.hpp"
#include "grid/generation/PrewaveletGridGenerator.hpp"
#include "grid/generation/hashmap/HashGenerator.hpp"
#include "grid/generation/hashmap/HashRefinement.hpp"
#include "grid/generation/hashmap/HashCoarsening.hpp"
#include "grid/generation/hashmap/HashRefinementBoundaries.hpp"
#include "grid/generation/hashmap/HashRefinementBoundariesMaxLevel.hpp"
#include "grid/generation/SurplusRefinementFunctor.hpp"
#include "grid/generation/SurplusVolumeRefinementFunctor.hpp"
#include "grid/generation/SurplusCoarseningFunctor.hpp"

#include "solver/sle/ConjugateGradients.hpp"
#include "solver/sle/BiCGStab.hpp"
#include "solver/ode/Euler.hpp"
#include "solver/ode/CrankNicolson.hpp"
#include "solver/ode/AdamsBashforth.hpp"
#include "solver/ode/VarTimestep.hpp"

#include "tools/finance/IOToolBonnSG.hpp"
#include "tools/common/GridPrinter.hpp"
#include "tools/common/GridPrinterForStretching.hpp"
#include "tools/common/SGppStopwatch.hpp"
#include "tools/common/EvalCuboidGenerator.hpp"
#include "tools/common/EvalCuboidGeneratorForStretching.hpp"

// note pflueged: anyone using this?
//namespace sg
//{
  //typedef AlgorithmDGEMV<base::SLinearBase> SGridOperationB;
  //typedef AlgorithmMultipleEvaluation<base::SLinearBase> SGridOperationNewB;
  //typedef AlgorithmDGEMV<base::SLinearBoundaryBase> SGridBoundaryOperationB;
  //typedef AlgorithmDGEMV<base::SModLinearBase> SGridModOperationB;
//}

#include "basis/operations_factory.hpp"
//#include "datadriven/operation/DatadrivenOpFactory.hpp"
//#include "parallel/operation/ParallelOpFactory.hpp"
//#include "finance/operation/FinanceOpFactory.hpp"
//#include "pde/operation/PdeOpFactory.hpp"
//#include "base/operation/BaseOpFactory.hpp"

#endif /*SGPP_HPP_*/
