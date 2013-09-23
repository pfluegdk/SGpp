/* ****************************************************************************
* Copyright (C) 2013 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
**************************************************************************** */
// @author Alexander Heinecke (Alexander.Heinecke@mytum.de)
// @author Jacob Jepsen (jepsen@diku.dk)

#include "parallel/finance/algorithm/BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI.hpp"
#include "base/exception/algorithm_exception.hpp"
//#include "base/grid/generation/functors/SurplusCoarseningFunctor.hpp"
//#include "base/grid/generation/functors/SurplusRefinementFunctor.hpp"
#include "base/operation/BaseOpFactory.hpp"
#include "pde/operation/PdeOpFactory.hpp"
#include "misc/operation/MiscOpFactory.hpp"
#include "parallel/operation/ParallelOpFactory.hpp"
#include <cmath>
#include <cstring>

namespace sg {
  namespace parallel {

    BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI(sg::base::Grid& SparseGrid, sg::base::DataVector& alpha, sg::base::DataVector& lambda,
												     sg::base::DataMatrix& eigenvecs, sg::base::DataVector& mu_hat, double TimestepSize, std::string OperationMode,
												     double dStrike, std::string option_type, double r,
												     bool useCoarsen, double coarsenThreshold, std::string adaptSolveMode,
												     int numCoarsenPoints, double refineThreshold, std::string refineMode, sg::base::GridIndex::level_type refineMaxLevel) {


      this->BoundGrid = &SparseGrid;
      this->alpha_complete = &alpha;

      this->alpha_complete_old = new sg::base::DataVector(*this->alpha_complete);
      this->alpha_complete_tmp = new sg::base::DataVector(*this->alpha_complete);
      this->oldGridStorage = new sg::base::GridStorage(*(this->BoundGrid)->getStorage());
      this->secondGridStorage = new sg::base::GridStorage(*(this->BoundGrid)->getStorage());

      this->InnerGrid = NULL;
      this->alpha_inner = NULL;
      this->tOperationMode = OperationMode;
      this->TimestepSize = TimestepSize;
      this->TimestepSize_old = TimestepSize;
      this->BoundaryUpdate = new sg::base::DirichletUpdateVector(SparseGrid.getStorage());
      this->GridConverter = new sg::base::DirichletGridConverter();

      // set Eigenvalues, Eigenvector of covariance matrix and mu_hat
      this->lambda = new sg::base::DataVector(lambda);
      this->eigenvecs = new sg::base::DataMatrix(eigenvecs);
      this->mu_hat = new sg::base::DataVector(mu_hat);

      this->BSalgoDims = this->BoundGrid->getAlgorithmicDimensions();
      this->nExecTimesteps = 0;

      // throw exception if grid dimensions not equal algorithmic dimensions
      if (this->BSalgoDims.size() > this->BoundGrid->getStorage()->dim()) {
        throw sg::base::algorithm_exception("BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI : Number of algorithmic dimensions higher than the number of grid's dimensions.");
      }

      // test if number of dimensions in the coefficients match the numbers of grid dimensions (mu and sigma)
      if (this->BoundGrid->getStorage()->dim() != this->lambda->getSize()) {
        throw sg::base::algorithm_exception("BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI : Dimension of mu and sigma parameters don't match the grid's dimensions!");
      }

      // test if all algorithmic dimensions are inside the grid's dimensions
      for (size_t i = 0; i < this->BSalgoDims.size(); i++) {
        if (this->BSalgoDims[i] >= this->BoundGrid->getStorage()->dim()) {
          throw sg::base::algorithm_exception("BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI : Minimum one algorithmic dimension is not inside the grid's dimensions!");
        }
      }

      // test if there are double algorithmic dimensions
      std::vector<size_t> tempAlgoDims(this->BSalgoDims);

      for (size_t i = 0; i < this->BSalgoDims.size(); i++) {
        size_t dimCount = 0;

        for (size_t j = 0; j < tempAlgoDims.size(); j++) {
          if (this->BSalgoDims[i] == tempAlgoDims[j]) {
            dimCount++;
          }
        }

        if (dimCount > 1) {
          throw sg::base::algorithm_exception("BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI : There is minimum one doubled algorithmic dimension!");
        }
      }

      // create the inner grid
      this->GridConverter->buildInnerGridWithCoefs(*this->BoundGrid, *this->alpha_complete, &this->InnerGrid, &this->alpha_inner);

      // Create operations
      char* alg_selector = getenv("SGPP_PDE_SOLVER_ALG");
      if(! strcmp(alg_selector, "X86SIMD")) {
        this->OpLaplaceInner = sg::op_factory::createOperationLaplaceVectorized(*this->InnerGrid, *this->lambda, sg::parallel::X86SIMD);
        this->OpLaplaceBound = sg::op_factory::createOperationLaplaceVectorized(*this->BoundGrid, *this->lambda, sg::parallel::X86SIMD);
        this->OpLTwoInner = sg::op_factory::createOperationLTwoDotProductVectorized(*this->InnerGrid, sg::parallel::X86SIMD);
        this->OpLTwoBound = sg::op_factory::createOperationLTwoDotProductVectorized(*this->BoundGrid, sg::parallel::X86SIMD);
        this->OpLTwoDotLaplaceInner = sg::op_factory::createOperationLTwoDotLaplaceVectorized(*this->InnerGrid, *this->lambda, sg::parallel::X86SIMD);
        this->OpLTwoDotLaplaceBound = sg::op_factory::createOperationLTwoDotLaplaceVectorized(*this->BoundGrid, *this->lambda, sg::parallel::X86SIMD);
#ifdef USEOCL
      } else if (! strcmp(alg_selector, "OCL")) {
        this->OpLaplaceInner = sg::op_factory::createOperationLaplaceVectorized(*this->InnerGrid, *this->lambda, sg::parallel::OpenCL);
        this->OpLaplaceBound = sg::op_factory::createOperationLaplaceVectorized(*this->BoundGrid, *this->lambda, sg::parallel::OpenCL);
        this->OpLTwoInner = sg::op_factory::createOperationLTwoDotProductVectorized(*this->InnerGrid, sg::parallel::OpenCL);
        this->OpLTwoBound = sg::op_factory::createOperationLTwoDotProductVectorized(*this->BoundGrid, sg::parallel::OpenCL);
        this->OpLTwoDotLaplaceInner = sg::op_factory::createOperationLTwoDotLaplaceVectorized(*this->InnerGrid, *this->lambda, sg::parallel::OpenCL);
        this->OpLTwoDotLaplaceBound = sg::op_factory::createOperationLTwoDotLaplaceVectorized(*this->BoundGrid, *this->lambda, sg::parallel::OpenCL);
#endif
      } else {
        throw sg::base::algorithm_exception("BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI : no supported vectorization was selected!");        
      }

      // right hand side if System
      this->rhs = NULL;

      // set coarsen settings
      this->useCoarsen = useCoarsen;
      this->coarsenThreshold = coarsenThreshold;
      this->refineThreshold = refineThreshold;
      this->adaptSolveMode = adaptSolveMode;
      this->numCoarsenPoints = numCoarsenPoints;
      this->refineMode = refineMode;
      this->refineMaxLevel = refineMaxLevel;

      // init Number of AverageGridPoins
      this->numSumGridpointsInner = 0;
      this->numSumGridpointsComplete = 0;

      // init option type and strike
      this->dStrike = dStrike;
      this->option_type = option_type;
      this->r = r;
    }

    BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::~BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI() {
      delete this->OpLaplaceBound;
      delete this->OpLTwoBound;
      delete this->OpLaplaceInner;
      delete this->OpLTwoInner;
      delete this->OpLTwoDotLaplaceInner;
      delete this->OpLTwoDotLaplaceBound;

      delete this->BoundaryUpdate;
      delete this->GridConverter;

      if (this->InnerGrid != NULL) {
        delete this->InnerGrid;
      }

      if (this->alpha_inner != NULL) {
        delete this->alpha_inner;
      }

      if (this->rhs != NULL) {
        delete this->rhs;
      }

      delete this->alpha_complete_old;
      delete this->alpha_complete_tmp;
      delete this->lambda;
      delete this->eigenvecs;
      delete this->mu_hat;
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyLOperatorComplete(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());

      result.setAll(0.0);
      // Apply the Laplace operator
      this->OpLaplaceBound->mult(alpha, temp);
      result.axpy(-0.5, temp);
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyLOperatorInner(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());
      result.setAll(0.0);

      // Apply the Laplace operator
      this->OpLaplaceInner->mult(alpha, temp);
      result.axpy(-0.5, temp);
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyMassMatrixComplete(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());

      result.setAll(0.0);

      // Apply the mass matrix
      this->OpLTwoBound->mult(alpha, temp);

      result.add(temp);
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyMassMatrixInner(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());

      result.setAll(0.0);

      // Apply the mass matrix
      this->OpLTwoInner->mult(alpha, temp);

      result.add(temp);
    }
    
    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyMassMatrixLOperatorInner(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());

      result.setAll(0.0);

      this->OpLTwoDotLaplaceInner->mult(alpha, temp);

      result.add(temp);
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::applyMassMatrixLOperatorBound(sg::base::DataVector& alpha, sg::base::DataVector& result) {
      sg::base::DataVector temp(alpha.getSize());

      result.setAll(0.0);

      this->OpLTwoDotLaplaceBound->mult(alpha, temp);

      result.add(temp);
    }
    
    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::setTimestepCoefficientInner(double timestep_coefficient) {
      this->OpLTwoDotLaplaceInner->setTimestepCoeff(timestep_coefficient);
    }
    
    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::setTimestepCoefficientBound(double timestep_coefficient) {
      this->OpLTwoDotLaplaceBound->setTimestepCoeff(timestep_coefficient);
    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::finishTimestep() {
      // Replace the inner coefficients on the boundary grid
      this->GridConverter->updateBoundaryCoefs(*this->alpha_complete, *this->alpha_inner);

      // check if we are doing an American put -> handle early exercise
      if (this->option_type == "std_amer_put") {
        double current_time = static_cast<double>(this->nExecTimesteps) * this->TimestepSize;

        sg::base::OperationHierarchisation* myHierarchisation = sg::op_factory::createOperationHierarchisation(*this->BoundGrid);
        myHierarchisation->doDehierarchisation(*this->alpha_complete);
        size_t dim = this->BoundGrid->getStorage()->dim();
        sg::base::BoundingBox* myBB = new sg::base::BoundingBox(*(this->BoundGrid->getBoundingBox()));

        double* coords_val = new double[dim];

        for (size_t i = 0; i < this->BoundGrid->getStorage()->size(); i++) {
          std::vector<double> eval_point_coord;
          std::string coords = this->BoundGrid->getStorage()->get(i)->getCoordsStringBB(*myBB);
          std::stringstream coordsStream(coords);

          double tmp;

          // read coordinates
          for (size_t j = 0; j < dim; j++) {
            coordsStream >> tmp;

            coords_val[j] = tmp;
          }

          tmp = 0.0;

          for (size_t j = 0; j < dim; j++) {
            double inner_tmp = 0.0;

            for (size_t l = 0; l < dim; l++) {
              inner_tmp += this->eigenvecs->get(j, l) * (coords_val[l] - (current_time * this->mu_hat->get(l)));
            }

            tmp += exp(inner_tmp);
          }

          double payoff = std::max<double>(this->dStrike - (tmp / static_cast<double>(dim)), 0.0);
          double discounted_value = ((*this->alpha_complete)[i]) * exp(((-1.0) * (this->r * this->TimestepSize)));

          (*this->alpha_complete)[i] = std::max<double>(payoff, discounted_value);
        }

        delete[] coords_val;

        myHierarchisation->doHierarchisation(*this->alpha_complete);
        delete myHierarchisation;
        delete myBB;
      }

    }
    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::coarsenAndRefine(bool isLastTimestep) {
      // add number of Gridpoints
      this->numSumGridpointsInner += this->InnerGrid->getSize();
      this->numSumGridpointsComplete += this->BoundGrid->getSize();

      if (this->useCoarsen == true && isLastTimestep == false) {
         std::cout << std::endl << "WARNING!: Adaptivity during solution was requested but is not implemented ATM and is therefore skipped!!" << std::endl << std::endl;
#if 0
        ///////////////////////////////////////////////////
        // Start integrated refinement & coarsening
        ///////////////////////////////////////////////////

        size_t originalGridSize = this->BoundGrid->getStorage()->size();

        // Coarsen the grid
        sg::base::GridGenerator* myGenerator = this->BoundGrid->createGridGenerator();

        //std::cout << "Coarsen Threshold: " << this->coarsenThreshold << std::endl;
        //std::cout << "Grid Size: " << originalGridSize << std::endl;

        if (this->adaptSolveMode == "refine" || this->adaptSolveMode == "coarsenNrefine") {
          size_t numRefines = myGenerator->getNumberOfRefinablePoints();
          sg::base::SurplusRefinementFunctor* myRefineFunc = new sg::base::SurplusRefinementFunctor(this->alpha_complete, numRefines, this->refineThreshold);

          if (this->refineMode == "maxLevel") {
            myGenerator->refineMaxLevel(myRefineFunc, this->refineMaxLevel);
            this->alpha_complete->resizeZero(this->BoundGrid->getStorage()->size());
          }

          if (this->refineMode == "classic") {
            myGenerator->refine(myRefineFunc);
            this->alpha_complete->resizeZero(this->BoundGrid->getStorage()->size());
          }

          delete myRefineFunc;
        }

        if (this->adaptSolveMode == "coarsen" || this->adaptSolveMode == "coarsenNrefine") {
          size_t numCoarsen = myGenerator->getNumberOfRemovablePoints();
          sg::base::SurplusCoarseningFunctor* myCoarsenFunctor = new sg::base::SurplusCoarseningFunctor(this->alpha_complete, numCoarsen, this->coarsenThreshold);
          myGenerator->coarsenNFirstOnly(myCoarsenFunctor, this->alpha_complete, originalGridSize);
          delete myCoarsenFunctor;
        }

        delete myGenerator;

        ///////////////////////////////////////////////////
        // End integrated refinement & coarsening
        ///////////////////////////////////////////////////

        // rebuild the inner grid + coefficients
        this->GridConverter->rebuildInnerGridWithCoefs(*this->BoundGrid, *this->alpha_complete, &this->InnerGrid, &this->alpha_inner);
#endif
      }

    }

    void BlackScholesPATParabolicPDESolverSystemEuroAmerVectorizedMPI::startTimestep() {
    }

  }

}
