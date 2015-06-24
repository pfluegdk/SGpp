// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <sgpp/globaldef.hpp>

#include <sgpp/optimization/sle/solver/Auto.hpp>
#include <sgpp/optimization/sle/solver/Armadillo.hpp>
#include <sgpp/optimization/sle/solver/BiCGStab.hpp>
#include <sgpp/optimization/sle/solver/Eigen.hpp>
#include <sgpp/optimization/sle/solver/GaussianElimination.hpp>
#include <sgpp/optimization/sle/solver/Gmmpp.hpp>
#include <sgpp/optimization/sle/solver/UMFPACK.hpp>
#include <sgpp/optimization/tools/Printer.hpp>

#include <cstddef>
#include <algorithm>
#include <map>

namespace SGPP {
  namespace optimization {
    namespace sle_solver {

      /**
       * Add a solver to the vector of solvers, if the solver is supported
       * (e.g. SG++ configured and compiled for use with the solve) and
       * it's not already in the vector.
       *
       * @param solver    linear solver
       * @param solvers   vector of solvers
       * @param supports  map indicating which solvers are supported
       */
      void addSLESolver(SLESolver* solver, std::vector<SLESolver*>& solvers,
                        const std::map<SLESolver*, bool>& supports) {
        // add solver if it's supported and not already in the vector
        if ((supports.at(solver)) &&
            (std::find(solvers.begin(), solvers.end(), solver) ==
             solvers.end())) {
          solvers.push_back(solver);
        }
      }

      bool Auto::solve(SLE& system, base::DataVector& b,
                       base::DataVector& x) const {
        std::vector<base::DataVector> B = {b};
        std::vector<base::DataVector> X = {x};

        if (solve(system, B, X)) {
          x.resize(X[0].getSize());
          x = X[0];
          return true;
        } else {
          return false;
        }
      }

      bool Auto::solve(SLE& system, std::vector<base::DataVector>& B,
                       std::vector<base::DataVector>& X) const {
        printer.printStatusBegin(
          "Solving linear system (automatic method)...");

        Armadillo solverArmadillo;
        Eigen solverEigen;
        UMFPACK solverUMFPACK;
        Gmmpp solverGmmpp;
        BiCGStab solverBiCGStab;
        GaussianElimination solverGaussianElimination;

        std::map<SLESolver*, bool> supports;

        // by default, only BiCGStab and GaussianElimination supported
        supports[&solverArmadillo] = false;
        supports[&solverEigen] = false;
        supports[&solverUMFPACK] = false;
        supports[&solverGmmpp] = false;
        supports[&solverBiCGStab] = true;
        supports[&solverGaussianElimination] = true;

#ifdef USE_ARMADILLO
        supports[&solverArmadillo] = true;
#endif /* USE_ARMADILLO */

#ifdef USE_EIGEN
        supports[&solverEigen] = true;
#endif /* USE_EIGEN */

#ifdef USE_UMFPACK
        supports[&solverUMFPACK] = true;
#endif /* USE_UMFPACK */

#ifdef USE_GMMPP
        supports[&solverGmmpp] = true;
#endif /* USE_GMMPP */

        // solvers to be used, the solver which should be tried first
        // should be the first element
        std::vector<SLESolver*> solvers;
        const size_t n = system.getDimension();

        if (supports[&solverUMFPACK] || supports[&solverGmmpp]) {
          // if at least one of the sparse solvers is supported
          // ==> estimate sparsity ratio of matrix by considering
          // every inc-th row
          size_t nrows = 0;
          size_t nnz = 0;
          size_t inc = static_cast<size_t>(
                         ESTIMATE_NNZ_ROWS_SAMPLE_SIZE *
                         static_cast<float_t>(n)) + 1;

          printer.printStatusUpdate("estimating sparsity pattern");

          for (size_t i = 0; i < n; i += inc) {
            nrows++;

            for (size_t j = 0; j < n; j++) {
              if (system.isMatrixEntryNonZero(i, j)) {
                nnz++;
              }
            }
          }

          // calculate estimate ratio nonzero entries
          float_t nnzRatio = static_cast<float_t>(nnz) /
                             (static_cast<float_t>(nrows) *
                              static_cast<float_t>(n));

          // print ratio
          {
            char str[10];
            snprintf(str, 10, "%.1f%%", nnzRatio * 100.0);
            printer.printStatusUpdate("estimated nnz ratio: " +
                                      std::string(str));
            printer.printStatusNewLine();
          }

          if (nnzRatio <= MAX_NNZ_RATIO_FOR_SPARSE) {
            // prefer UMFPACK over Gmm++
            addSLESolver(&solverUMFPACK, solvers, supports);
            addSLESolver(&solverGmmpp, solvers, supports);
          }
        }

        // add all remaining solvers (prefer Armadillo over Eigen)
        addSLESolver(&solverArmadillo, solvers, supports);
        addSLESolver(&solverEigen, solvers, supports);
        addSLESolver(&solverUMFPACK, solvers, supports);
        addSLESolver(&solverGmmpp, solvers, supports);
        addSLESolver(&solverBiCGStab, solvers, supports);
        addSLESolver(&solverGaussianElimination, solvers, supports);

        for (size_t i = 0; i < solvers.size(); i++) {
          // try solver
          bool result = solvers[i]->solve(system, B, X);

          if (result) {
            printer.printStatusEnd();
            return true;
          } else if ((solvers[i] == &solverGmmpp) &&
                     (n > MAX_DIM_FOR_FULL)) {
            // don't use full solvers and return approximative solution
            printer.printStatusEnd(
              "warning: using non-converged solution of iterative "
              "solver, residual can be large "
              "(matrix too large to try other solvers)");
            return true;
          }
        }

        printer.printStatusEnd("error: could not solve linear system!");
        return false;
      }

    }
  }
}
