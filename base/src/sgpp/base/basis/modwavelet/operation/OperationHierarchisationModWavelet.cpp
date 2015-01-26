// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at 
// sgpp.sparsegrids.org

#include <sgpp/base/basis/modpoly/ModifiedPolyBasis.hpp>
#include <sgpp/base/basis/modwavelet/operation/OperationHierarchisationModWavelet.hpp>



#include <sgpp/base/exception/operation_exception.hpp>

#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace base {

    void OperationHierarchisationModWavelet::doHierarchisation(DataVector& node_values) {
      throw new operation_exception("This operation is not implemented, yet! Sorry ;-)");
    }

    void OperationHierarchisationModWavelet::doDehierarchisation(DataVector& alpha) {
      throw new operation_exception("This operation is not implemented, yet! Sorry ;-)");
    }

  }
}