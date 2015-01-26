// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at 
// sgpp.sparsegrids.org

#include <sgpp/base/basis/modlinear/operation/OperationStencilHierarchisationModLinear.hpp>
#include <sgpp/base/basis/modlinear/algorithm_sweep/StencilHierarchisationModLinear.hpp>
#include <sgpp/base/basis/modlinear/algorithm_sweep/StencilDehierarchisationModLinear.hpp>

#include <sgpp/base/algorithm/sweep.hpp>


#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace base {

    void OperationStencilHierarchisationModLinear::doHierarchisation(DataVector& node_values) {
      surplusStencil.clear();
      neighborStencil.clear();
      weightStencil.clear();
      StencilHierarchisationModLinear func(this->storage, surplusStencil, neighborStencil, weightStencil);
      sweep<StencilHierarchisationModLinear> s(func, this->storage);

      // Execute hierarchisation in every dimension of the grid
      for (size_t i = 0; i < this->storage->dim(); i++) {
        s.sweep1D(node_values, node_values, i);
      }
    }

    void OperationStencilHierarchisationModLinear::doDehierarchisation(DataVector& alpha) {
      surplusStencil.clear();
      neighborStencil.clear();
      weightStencil.clear();
      StencilDehierarchisationModLinear func(this->storage, surplusStencil, neighborStencil, weightStencil);
      sweep<StencilDehierarchisationModLinear> s(func, this->storage);

      // Execute hierarchisation in every dimension of the grid
      for (size_t i = 0; i < this->storage->dim(); i++) {
        s.sweep1D(alpha, alpha, i);
      }
    }

  }
}