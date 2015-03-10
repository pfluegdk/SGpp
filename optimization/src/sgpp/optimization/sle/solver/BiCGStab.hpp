// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef SGPP_OPTIMIZATION_SLE_SOLVER_BICGSTAB_HPP
#define SGPP_OPTIMIZATION_SLE_SOLVER_BICGSTAB_HPP

#include <sgpp/globaldef.hpp>
#include <sgpp/optimization/sle/solver/SLESolver.hpp>

#include <cstddef>
#include <vector>

namespace SGPP {
  namespace optimization {
    namespace sle_solver {

      /**
       * Linear system solver implementing the iterative BiCGStab method.
       */
      class BiCGStab : public SLESolver {
        public:
          /// default maximal number of iterations
          static const size_t DEFAULT_MAX_IT_COUNT = 1000;
          /// default tolerance
          static constexpr float_t DEFAULT_TOLERANCE = 1e-10;

          /**
           * Constructor.
           */
          BiCGStab();

          /**
           * @param maxItCount        maximal number of iterations
           * @param tolerance         tolerance
           * @param startingPoint     starting vector
           */
          BiCGStab(size_t maxItCount, float_t tolerance,
                   const std::vector<float_t>& startingPoint);

          /**
           * @param       system  system to be solved
           * @param       b       right-hand side
           * @param[out]  x       solution to the system
           * @return              whether all went well
           *                      (false if errors occurred)
           */
          bool solve(SLE& system, const std::vector<float_t>& b,
                     std::vector<float_t>& x) const;

          /**
           * @return              maximal number of iterations
           */
          size_t getMaxItCount() const;

          /**
           * @param maxItCount   maximal number of iterations
           */
          void setMaxItCount(size_t maxItCount);

          /**
           * @return              tolerance
           */
          float_t getTolerance() const;

          /**
           * @param tolerance     tolerance
           */
          void setTolerance(float_t tolerance);

          /**
           * @return                  starting vector
           */
          const std::vector<float_t>& getStartingPoint() const;

          /**
           * @param startingPoint     starting vector
           */
          void setStartingPoint(const std::vector<float_t>& startingPoint);

        protected:
          /// maximal number of iterations
          size_t N;
          /// tolerance
          float_t tol;
          /// starting vector
          std::vector<float_t> x0;
      };

    }
  }
}

#endif /* SGPP_OPTIMIZATION_SLE_SOLVER_BICGSTAB_HPP */
