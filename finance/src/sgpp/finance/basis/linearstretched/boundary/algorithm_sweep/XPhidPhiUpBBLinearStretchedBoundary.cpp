// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <sgpp/finance/basis/linearstretched/boundary/algorithm_sweep/XPhidPhiUpBBLinearStretchedBoundary.hpp>

#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace finance {



    XPhidPhiUpBBLinearStretchedBoundary::XPhidPhiUpBBLinearStretchedBoundary(SGPP::base::GridStorage* storage) : XPhidPhiUpBBLinearStretched(storage) {
    }

    XPhidPhiUpBBLinearStretchedBoundary::~XPhidPhiUpBBLinearStretchedBoundary() {
    }

    /*void XPhidPhiUpBBLinearStretchedBoundary::operator()(SGPP::base::DataVector& source, SGPP::base::DataVector& result, grid_iterator& index, size_t dim)
    {
      float_t q = this->stretching->getIntervalWidth(dim);
      float_t t = this->stretching->getIntervalOffset(dim);



      // get boundary values
      float_t fl = 0.0;
      float_t fr = 0.0;


        if(!index.hint())
        {
          index.top(dim);

          if(!this->storage->end(index.seq()))
          {
            rec(source, result, index, dim, fl, fr);
    //        recBB(source, result, index, dim, fl, fr, q, t);
          }

          index.left_levelzero(dim);
        }

        size_t seq_left;
        size_t seq_right;

        // left boundary
        seq_left = index.seq();

        // right boundary
        index.right_levelzero(dim);
        seq_right = index.seq();

        // check boundary conditions
        if (this->stretching->hasDirichletBoundaryLeft(dim))
        {
          result[seq_left] = 0.0; // source[seq_left];
        }
        else
        {
          // up
          //////////////////////////////////////
          result[seq_left] = fl;

          result[seq_left] += (source[seq_right] * (((-1.0/3.0)*q) - (0.5*t)));
        }

        if (this->stretching->hasDirichletBoundaryRight(dim))
        {
          result[seq_right] = 0.0; //source[seq_right];
        }
        else
        {
          result[seq_right] = fr;
        }

        index.left_levelzero(dim);


    }*/


    void XPhidPhiUpBBLinearStretchedBoundary::operator()(SGPP::base::DataVector& source, SGPP::base::DataVector& result, grid_iterator& index, size_t dim) {
      float_t q = this->stretching->getIntervalWidth(dim);
      float_t t = this->stretching->getIntervalOffset(dim);



      // get boundary values
      float_t fl = 0.0;
      float_t fr = 0.0;


      if (!index.hint()) {
        index.resetToLevelOne(dim);

        if (!this->storage->end(index.seq())) {
          rec(source, result, index, dim, fl, fr);
        }

        index.resetToLeftLevelZero(dim);
      }

      size_t seq_left;
      size_t seq_right;

      // left boundary
      seq_left = index.seq();

      // right boundary
      index.resetToRightLevelZero(dim);
      seq_right = index.seq();

      // check boundary conditions
      if (this->stretching->hasDirichletBoundaryLeft(dim)) {
        result[seq_left] = 0.0; // source[seq_left];
      } else {
        // up
        //////////////////////////////////////
        result[seq_left] = fl;

        result[seq_left] += (source[seq_right] * (((-1.0 / 3.0) * q) - (0.5 * t)));
      }

      if (this->stretching->hasDirichletBoundaryRight(dim)) {
        result[seq_right] = 0.0; //source[seq_right];
      } else {
        result[seq_right] = fr;
      }

      index.resetToLeftLevelZero(dim);


    }
    // namespace detail

  } // namespace SGPP
}