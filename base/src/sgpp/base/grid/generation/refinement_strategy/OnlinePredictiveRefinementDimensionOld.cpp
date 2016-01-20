// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include <algorithm>
#include <limits>
#include <list>
#include <vector>
#include <utility>
#include <iterator>

#include <sgpp/base/grid/generation/refinement_strategy/OnlinePredictiveRefinementDimensionOld.hpp>
#include <sgpp/base/grid/generation/functors/PredictiveRefinementDimensionIndicator.hpp>



#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace base {

    bool float_tReverseCompare(const float_t firstEl, const float_t secondEl) {
      return firstEl > secondEl;
    }

    void OnlinePredictiveRefinementDimensionOld::collectRefinablePoints(
      GridStorage* storage, RefinementFunctor* functor,
      size_t refinements_num, size_t* max_indices,
      PredictiveRefinementDimensionIndicator::value_type* max_values) {

      //this refinement algorithm uses the predictive refinement indicator.
      //dynamic casting is used to maintain the signature of the algorithm, but still be able to use the
      //predictive refinement indicator with it.
      PredictiveRefinementDimensionIndicator* errorIndicator =
        dynamic_cast<PredictiveRefinementDimensionIndicator*>(functor);

      //size_t min_idx = 0;

      // max value equals min value
      //PredictiveRefinementDimensionIndicator::value_type* max_values =static_cast<PredictiveRefinementDimensionIndicator::value_type*>(mv);
      //PredictiveRefinementDimensionIndicator::value_type max_value = max_values[min_idx];

      index_type index;
      GridStorage::grid_map_iterator end_iter = storage->end();

      // start iterating over whole grid
      for (GridStorage::grid_map_iterator iter = storage->begin();
           iter != end_iter; iter++) {
        index = *(iter->first);

        GridStorage::grid_map_iterator child_iter;

        // check for each grid point whether it can be refined (i.e., whether not all kids exist yet)
        // if yes, check whether it belongs to the refinements_num largest ones
        for (size_t d = 0; d < storage->dim(); d++) {
          index_t source_index;
          level_t source_level;
          index.get(d, source_level, source_index);
          float_t error = functor->start();
          //errorIndicator->setActiveDim(d);

          // test existence of left child
          index.set(d, source_level + 1, 2 * source_index - 1);
          child_iter = storage->find(&index);

          if (child_iter == end_iter) {
            //use the predictive error indicator, which takes a pointer to the grid point object
            //instead of the storage index
            error += (*errorIndicator)(&index);
          }

          // test existance of right child
          index.set(d, source_level + 1, 2 * source_index + 1);
          child_iter = storage->find(&index);

          if (child_iter == end_iter) {//use predictive refinement indicator
            //use the predictive error indicator, which takes a pointer to the grid point object
            //instead of the storage index
            error += (*errorIndicator)(&index);
          }

          // reset current grid point in dimension d
          index.set(d, source_level, source_index);

          // Heuristic!
          error *= std::fabs(alpha_->get(storage->seq(iter->first)));



          //if (error > iThreshold_) {
          key_type key(storage->seq(iter->first), d);
          //        if (refinementCollection_.find(key)
          //            != refinementCollection_.end()) {
          //          refinementCollection_[key] = refinementCollection_[key] + error;
          //        } else {
          refinementCollection_[key] = error;
          //        }
          //}
        }
      }
    }

    bool refinementPairCompare(const std::pair<OnlinePredictiveRefinementDimensionOld::key_type, OnlinePredictiveRefinementDimensionOld::value_type>& firstEl,
                               const std::pair<OnlinePredictiveRefinementDimensionOld::key_type, OnlinePredictiveRefinementDimensionOld::value_type>& secondEl);
    /*{
      return firstEl.second > secondEl.second;
    }*/


    void OnlinePredictiveRefinementDimensionOld::refineGridpointsCollection(
      GridStorage* storage, RefinementFunctor* functor,
      size_t refinements_num, size_t* max_indices,
      PredictiveRefinementDimensionIndicator::value_type* max_values) {
      PredictiveRefinementDimensionIndicator::value_type max_value;
      //size_t max_index;
      //PredictiveRefinementDimensionIndicator::value_type* max_values =static_cast<PredictiveRefinementDimensionIndicator::value_type*>(mv);
      // now refine all grid points which satisfy the refinement criteria
      float_t threshold = functor->getRefinementThreshold();

      std::vector< std::pair<key_type, value_type> > errorsVector;
      //std::cout << "refinement collection size " << refinementCollection_.size() << std::endl;
      std::copy(refinementCollection_.begin(),
                refinementCollection_.end(),
                std::back_inserter<std::vector<std::pair<key_type, value_type> > >(errorsVector));

      std::nth_element(errorsVector.begin(), errorsVector.begin() + refinements_num,
                       errorsVector.end(), refinementPairCompare);

      PredictiveRefinementDimensionIndicator* predFunctor = dynamic_cast<PredictiveRefinementDimensionIndicator*>(functor);

      std::vector<std::pair<key_type, value_type> >::const_iterator iter;

      for (iter = errorsVector.begin(); iter < errorsVector.begin() + refinements_num && iter < errorsVector.end(); iter++) {
        if (iter->second > functor->start() && iter->second >= threshold) {
          index_pointer index(storage->get(iter->first.first));
          //Sets leaf property of index, which is refined to false

          //(storage->get((storage->find(index))->second))->setLeaf(false);
          //storage->get(iter->first.first)->setLeaf(false);
          std::cout << "Refining grid point " << iter->first.first << " dim " << iter->first.second << " value "
                    << iter->second << " ";

          // output counter sum
          index_type it = *index;
          size_t d = iter->first.second;
          auto lvl = it.getLevel(d);
          auto idx = it.getIndex(d);
          it.set(d, lvl + 1, idx * 2 - 1);
          long unsigned int counter = 0;

          if (predFunctor->countersMap.find(it) != predFunctor->countersMap.end()) {
            counter += predFunctor->countersMap[it];
          }

          it.set(d, lvl + 1, idx * 2 + 1);

          if (predFunctor->countersMap.find(it) != predFunctor->countersMap.end()) {
            counter += predFunctor->countersMap[it];
          }

          std::cout << " points on support: " << counter << " map size " << predFunctor->countersMap.size() << std::endl;

          index_type index_tmp = *index;
          this->refineGridpoint1D(storage, index_tmp, iter->first.second);
          index->setLeaf(false);
        }
      }

      predFunctor->countersMap.clear();

      /*for (size_t i = 0; i < refinements_num; i++) {
        max_value = max_values[i];
        max_index = max_indices[i];

        if (max_value.second > functor->start()
            && max_value.second >= threshold) {
          index_type index((*storage)[max_index]);
          //Sets leaf property of index, which is refined to false
          (*storage)[max_index]->setLeaf(false);
          std::cout << "Refining grid point " << max_index << " value "
              << max_value.second << std::endl;
          this->refineGridpoint1D(storage, index, max_value.first);
        }
      }*/

    }

    void OnlinePredictiveRefinementDimensionOld::free_refine(GridStorage* storage,
        PredictiveRefinementDimensionIndicator* functor) {
      if (storage->size() == 0) {
        throw generation_exception("storage empty");
      }

      // the functor->getRefinementsNum() largest grid points should be refined.
      // gather them in an array max_values
      size_t refinements_num = functor->getRefinementsNum();
      // values
      PredictiveRefinementDimensionIndicator::value_type* max_values =
        new PredictiveRefinementDimensionIndicator::value_type[refinements_num];
      // indices
      size_t* max_indices = new size_t[refinements_num];

      // initialization
      for (size_t i = 0; i < refinements_num; i++) {
        max_values[i].second = functor->start();
        max_indices[i] = 0;
      }

      collectRefinablePoints(storage, functor, refinements_num, max_indices,
                             max_values);
      // now refine all grid points which satisfy the refinement criteria
      refineGridpointsCollection(storage, functor, refinements_num, max_indices,
                                 max_values);
      refinementCollection_.clear();
      //storage->recalcLeafProperty();
      delete[] max_values;
      delete[] max_indices;

    }

    size_t OnlinePredictiveRefinementDimensionOld::getIndexOfMin(
      PredictiveRefinementDimensionIndicator::value_type* array,
      size_t length) {
      size_t min_idx = 0;

      for (size_t i = 1; i < length; i++) {
        if (array[i].second < array[min_idx].second)
          min_idx = i;
      }

      return min_idx;
    }

  } /* namespace base */
} /* namespace SGPP */
