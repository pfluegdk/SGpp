// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef SGPP_OPTIMIZATION_OPTIMIZER_CONSTRAINED_LOGBARRIER_HPP
#define SGPP_OPTIMIZATION_OPTIMIZER_CONSTRAINED_LOGBARRIER_HPP

#include <sgpp/globaldef.hpp>

#include <sgpp/optimization/function/ObjectiveGradient.hpp>
#include <sgpp/optimization/function/ConstraintGradient.hpp>
#include <sgpp/optimization/optimizer/constrained/ConstrainedOptimizer.hpp>

namespace SGPP {
  namespace optimization {
    namespace optimizer {

      /**
       * Log Barrier method for constrained optimization.
       */
      class LogBarrier : public ConstrainedOptimizer {
        public:
          /// default tolerance
          static constexpr float_t DEFAULT_TOLERANCE = 1e-6;
          /// default barrier start value
          static constexpr float_t DEFAULT_BARRIER_START_VALUE = 1.0;
          /// default barrier decrease factor
          static constexpr float_t DEFAULT_BARRIER_DECREASE_FACTOR = 0.5;

          /**
           * Constructor.
           *
           * @param f                     objective function
           * @param fGradient             objective function gradient
           * @param g                     inequality constraint
           * @param gGradient             inequality constraint gradient
           * @param maxItCount            maximal number of
           *                              function evaluations
           * @param tolerance             tolerance
           * @param barrierStartValue     barrier start value
           * @param barrierDecreaseFactor barrier decrease factor
           */
          LogBarrier(ObjectiveFunction& f,
                     ObjectiveGradient& fGradient,
                     ConstraintFunction& g,
                     ConstraintGradient& gGradient,
                     size_t maxItCount = DEFAULT_N,
                     float_t tolerance = DEFAULT_TOLERANCE,
                     float_t barrierStartValue = DEFAULT_BARRIER_START_VALUE,
                     float_t barrierDecreaseFactor =
                       DEFAULT_BARRIER_DECREASE_FACTOR);

          /**
           * @param[out] xOpt optimal point
           * @return          optimal objective function value
           */
          float_t optimize(base::DataVector& xOpt);

          /**
           * @return objective function gradient
           */
          ObjectiveGradient& getObjectiveGradient() const;

          /**
           * @return inequality constraint function gradient
           */
          ConstraintGradient& getInequalityConstraintGradient() const;

          /**
           * @return tolerance
           */
          float_t getTolerance() const;

          /**
           * @param tolerance tolerance
           */
          void setTolerance(float_t tolerance);

          /**
           * @return barrier start value
           */
          float_t getBarrierStartValue() const;

          /**
           * @param barrierStartValue barrier start value
           */
          void setBarrierStartValue(float_t barrierStartValue);

          /**
           * @return barrier decrease factor
           */
          float_t getBarrierDecreaseFactor() const;

          /**
           * @param barrierDecreaseFactor barrier decrease factor
           */
          void setBarrierDecreaseFactor(float_t barrierDecreaseFactor);

        protected:
          /// objective function gradient
          ObjectiveGradient& fGradient;
          /// inequality constraint function gradient
          ConstraintGradient& gGradient;
          /// tolerance
          float_t theta;
          /// barrier start value
          float_t mu0;
          /// barrier decrease factor
          float_t rhoMuMinus;
      };

    }
  }
}

#endif /* SGPP_OPTIMIZATION_OPTIMIZER_CONSTRAINED_LOGBARRIER_HPP */