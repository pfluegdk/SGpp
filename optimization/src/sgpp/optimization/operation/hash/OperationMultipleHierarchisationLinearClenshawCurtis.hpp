// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef SGPP_OPTIMIZATION_OPERATION_HASH_OPERATIONMULTIPLEHIERARCHISATIONLINEARCLENSHAWCURTIS_HPP
#define SGPP_OPTIMIZATION_OPERATION_HASH_OPERATIONMULTIPLEHIERARCHISATIONLINEARCLENSHAWCURTIS_HPP

#include <sgpp/globaldef.hpp>

#include <sgpp/optimization/operation/hash/OperationMultipleHierarchisation.hpp>
#include <sgpp/base/grid/type/LinearClenshawCurtisGrid.hpp>
#include <sgpp/base/datatypes/DataVector.hpp>

namespace SGPP {
  namespace optimization {

    /**
     * Hierarchisation operation for linear basis functions on
     * Clenshaw-Curtis grids.
     */
    class OperationMultipleHierarchisationLinearClenshawCurtis :
      public OperationMultipleHierarchisation {
      public:
        /**
         * Constructor.
         *
         * @param grid      grid
         */
        OperationMultipleHierarchisationLinearClenshawCurtis(
          base::LinearClenshawCurtisGrid& grid) :
          grid(grid) {
        }

        /**
         * Virtual destructor.
         */
        virtual ~OperationMultipleHierarchisationLinearClenshawCurtis() {
        }

        /**
         * @param[in,out] nodeValues before: vector of function values at
         *                           the grid points,
         *                           after: vector of hierarchical coefficients
         * @return                   whether hierarchisation was successful
         */
        virtual bool doHierarchisation(base::DataVector& nodeValues);

        /**
         * @param[in,out] alpha before: vector of hierarchical coefficients,
         *                      after: vector of function values at
         *                      the grid points
         */
        virtual void doDehierarchisation(base::DataVector& alpha);

        /**
         * @param[in,out] nodeValues before: vector of function values at
         *                           the grid points,
         *                           after: vector of hierarchical coefficients
         * @return                   whether hierarchisation was successful
         */
        virtual bool doHierarchisation(
          std::vector<base::DataVector>& nodeValues);

        /**
         * @param[in,out] alpha before: vector of hierarchical coefficients,
         *                      after: vector of function values at
         *                      the grid points
         */
        virtual void doDehierarchisation(
          std::vector<base::DataVector>& alpha);

      protected:
        /// storage of the sparse grid
        base::LinearClenshawCurtisGrid& grid;
    };

  }
}

#endif /* SGPP_OPTIMIZATION_OPERATION_HASH_OPERATIONMULTIPLEHIERARCHISATIONLINEARCLENSHAWCURTIS_HPP */
