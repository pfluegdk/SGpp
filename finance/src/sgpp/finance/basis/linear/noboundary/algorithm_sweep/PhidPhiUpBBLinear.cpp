// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at 
// sgpp.sparsegrids.org

#include <sgpp/finance/basis/linear/noboundary/algorithm_sweep/PhidPhiUpBBLinear.hpp>

#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace finance {



    PhidPhiUpBBLinear::PhidPhiUpBBLinear(SGPP::base::GridStorage* storage) : storage(storage), boundingBox(storage->getBoundingBox()) {
    }

    PhidPhiUpBBLinear::~PhidPhiUpBBLinear() {
    }

    void PhidPhiUpBBLinear::operator()(SGPP::base::DataVector& source, SGPP::base::DataVector& result, grid_iterator& index, size_t dim) {
      // get boundary values
      double fl = 0.0;
      double fr = 0.0;

      rec(source, result, index, dim, fl, fr);
    }

    void PhidPhiUpBBLinear::rec(SGPP::base::DataVector& source, SGPP::base::DataVector& result, grid_iterator& index, size_t dim, double& fl, double& fr) {
      size_t seq = index.seq();

      fl = fr = 0.0;
      double fml = 0.0;
      double fmr = 0.0;

      SGPP::base::GridStorage::index_type::level_type current_level;
      SGPP::base::GridStorage::index_type::index_type current_index;

      if (!index.hint()) {
        index.left_child(dim);

        if (!storage->end(index.seq())) {
          rec(source, result, index, dim, fl, fml);
        }

        index.step_right(dim);

        if (!storage->end(index.seq())) {
          rec(source, result, index, dim, fmr, fr);
        }

        index.up(dim);
      }

      index.get(dim, current_level, current_index);

      double fm = fml + fmr;

      double alpha_value = source[seq];

      double helper = 0.5;

      // transposed operations:
      result[seq] = fm;

      fl = (fm / 2.0) + ((alpha_value * ((-1.0) * helper)) + fl);
      fr = (fm / 2.0) + ((alpha_value * helper) + fr);
    }

    // namespace detail

  } // namespace SGPP
}