// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_MULTIDIM_FULLGRIDTENSOREVALUATOR_HPP_
#define COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_MULTIDIM_FULLGRIDTENSOREVALUATOR_HPP_

#include <sgpp/combigrid/common/MultiIndexIterator.hpp>
#include <sgpp/combigrid/definitions.hpp>
#include <sgpp/combigrid/grid/hierarchy/AbstractPointHierarchy.hpp>
#include <sgpp/combigrid/operation/multidim/AbstractFullGridEvaluator.hpp>
#include <sgpp/combigrid/operation/onedim/AbstractLinearEvaluator.hpp>
#include <sgpp/combigrid/storage/AbstractCombigridStorage.hpp>
#include <sgpp/combigrid/threading/PtrGuard.hpp>
#include <sgpp/combigrid/threading/ThreadPool.hpp>

#include <vector>

namespace sgpp {
namespace combigrid {

/**
 * FullGridTensorEvaluator can be used to combine one-dimensional evaluation methods to a
 * multi-dimensional evaluation method on a full grid.
 * For every grid point, the evaluators in every dimension are consulted to get basis coefficients
 * for this dimension.
 * These coefficients are multiplied together and multiplied with the function value at that grid
 * point.
 * The values generated for each grid point in this way are then summed up to compute the resulting
 * value.
 * The concrete grid is given by PointHierarchy-Objects for each dimension and a level-multi-index
 * encoding a level for each dimension.
 * The template parameter V controls whether single-evaluation or multi-evaluation is done. Confer
 * also FloatArrayVector.
 */
template <typename V>
class FullGridTensorEvaluator : public AbstractFullGridEvaluator<V> {
  // partialProducts[i] stores the product of the first i basis values (corresponding to the current
  // multiindex)
  // , i. e. partialProducts[0] = 1
  // partialProducts has Size numDimensions, since the product partialProducts[numDimensions] is
  // only used once and does not have to be stored
  std::vector<V> partialProducts;

  /**
   * For each dimension, this contains a vector of weights which are used as coefficients for
   * linearly combining the function values at different grid points.
   */
  std::vector<std::vector<V>> basisValues;

  /**
   * Provides access to the function values (stored or computed on demand)
   */
  std::shared_ptr<AbstractCombigridStorage> storage;
  // one per dimension
  std::vector<std::shared_ptr<AbstractLinearEvaluator<V>>> evaluatorPrototypes;
  // one per dimension and level
  std::vector<std::vector<std::shared_ptr<AbstractLinearEvaluator<V>>>> evaluators;
  // one per dimension
  std::vector<std::shared_ptr<AbstractPointHierarchy>> pointHierarchies;
  // parameters (empty when doing quadrature)
  std::vector<V> parameters;

  /**
   * Pointer to a mutex that is locked when doing critical operations on data.
   * This is set to nullptr if the action is done in a single thread.
   */
  std::shared_ptr<std::mutex> mutexPtr;

 public:
  /**
   * Constructor.
   *
   * @param storage Storage that stores and provides the function values for each grid point.
   * @param evaluatorPrototypes prototype objects for the evaluators that are cloned to get an
   * evaluator for each dimension and each level.
   * @param pointHierarchies PointHierarchy objects for each dimension providing the points for each
   * level and information about their ordering.
   */
  FullGridTensorEvaluator(
      std::shared_ptr<AbstractCombigridStorage> storage,
      std::vector<std::shared_ptr<AbstractLinearEvaluator<V>>> evaluatorPrototypes,
      std::vector<std::shared_ptr<AbstractPointHierarchy>> pointHierarchies)
      : partialProducts(evaluatorPrototypes.size()),
        basisValues(evaluatorPrototypes.size()),
        storage(storage),
        evaluatorPrototypes(evaluatorPrototypes),
        evaluators(evaluatorPrototypes.size()),
        pointHierarchies(pointHierarchies),
        parameters(evaluatorPrototypes.size()) {
    // TODO(holzmudd): check for dimension equality
  }

  virtual ~FullGridTensorEvaluator() {}

  /**
   * Updates the current mutex. If the mutex is set to nullptr, no mutex locking is done. Otherwise,
   * the mutex is locked at critical actions.
   */
  virtual void setMutex(std::shared_ptr<std::mutex> mutexPtr) {
    this->mutexPtr = mutexPtr;
    storage->setMutex(mutexPtr);
  }

  /**
   * @return a vector of tasks which can be precomputed in parallel to make the (serialized)
   * execution of eval() faster
   * @param level the level which one wants to compute
   * @param callback This callback is called (with already locked mutex) from inside one of the
   * returned tasks when all tasks for the given level are completed and the level can be added.
   */
  std::vector<ThreadPool::Task> getLevelTasks(MultiIndex const &level, ThreadPool::Task callback) {
    size_t numDimensions = evaluators.size();
    MultiIndex multiBounds(numDimensions);

    for (size_t d = 0; d < numDimensions; ++d) {
      multiBounds[d] = pointHierarchies[d]->getNumPoints(level[d]);
    }

    MultiIndexIterator it(multiBounds);
    std::vector<bool> orderingConfiguration(numDimensions,
                                            false);  // The request does not need ordered points
    auto funcIter = storage->getGuidedIterator(level, it, orderingConfiguration);

    std::vector<std::function<double()>> computationTasks;
    std::vector<MultiIndex> multiIndices;

    while (funcIter->isValid()) {
      if (!funcIter->computationRequested()) {
        computationTasks.push_back(funcIter->requestComputationTask());
        multiIndices.push_back(funcIter->getMultiIndex());
      }
      funcIter->moveToNext();
    }

    std::vector<ThreadPool::Task> tasks;

    if (computationTasks.empty()) {
      callback();
    } else {
      // make it a pointer so that it does not get deleted before all tasks are completed
      auto counter = std::make_shared<size_t>(computationTasks.size());

      for (size_t i = 0; i < computationTasks.size(); ++i) {
        auto compTask = computationTasks[i];
        auto index = multiIndices[i];

        tasks.push_back([compTask, index, counter, callback, this, level]() {
          auto result = compTask();

          CGLOG_SURROUND(PtrGuard guard(this->mutexPtr));
          this->storage->set(level, index, result);
          --(*counter);
          if (*counter == 0) {
            callback();
          }
          CGLOG("leave guard(this->mutexPtr) in FGEval");
        });
      }
    }

    return tasks;
  }

  /**
   * Evaluates the function given through the storage for a certain level-multi-index (see class
   * description).
   */
  virtual V eval(MultiIndex const &level) {
    CGLOG("FullGridTensorEvaluator::eval(): start");
    size_t numDimensions = evaluators.size();
    size_t lastDim = numDimensions - 1;
    MultiIndex multiBounds(numDimensions);
    std::vector<bool> orderingConfiguration(numDimensions);

    size_t paramIndex = 0;

    // the basis coefficients for this level are stored
    // the bounds for traversal are initialized given the number of points in each direction
    // if not already stored, the evaluators for the given level are cloned and their parameter, if
    // needed, is set
    // the ordering configuration is created - it stores a boolean for each dimension expressing
    // whether the corresponding evaluator needs sorted (ascending) points

    // init evaluators and basis values, init multiBounds and orderingConfiguration
    for (size_t d = 0; d < numDimensions; ++d) {
      size_t currentLevel = level[d];
      auto &currentEvaluators = evaluators[d];

      bool needsParam = evaluatorPrototypes[d]->needsParameter();
      bool needsOrdered = evaluatorPrototypes[d]->needsOrderedPoints();

      for (size_t i = currentEvaluators.size(); i <= currentLevel; ++i) {
        auto eval = evaluatorPrototypes[d]->cloneLinear();
        eval->setGridPoints(pointHierarchies[d]->getPoints(i, needsOrdered));
        if (needsParam) {
          eval->setParameter(parameters[paramIndex]);
        }
        currentEvaluators.push_back(eval);
      }
      basisValues[d] = currentEvaluators[currentLevel]->getBasisCoefficients();
      multiBounds[d] = pointHierarchies[d]->getNumPoints(currentLevel);
      orderingConfiguration[d] = needsOrdered;

      if (needsParam) {
        ++paramIndex;
      }
    }

    // for efficient computation, the products over the first i evaluator coefficients are stored
    // for all i up to n-1.
    // This way, we only have to multiply them with the values for the changing indices at each
    // iteration step.
    // init partial products
    partialProducts[0] = V::one();
    for (size_t d = 1; d < numDimensions; ++d) {
      V value = partialProducts[d - 1];
      value.componentwiseMult(basisValues[d - 1][0]);
      partialProducts[d] = value;
    }

    CGLOG("FullGridTensorEvaluator::eval(): create storage iterator");

    // start iteration
    MultiIndexIterator it(multiBounds);
    auto funcIter = storage->getGuidedIterator(level, it, orderingConfiguration);
    V sum = V::zero();

    CGLOG("FullGridTensorEvaluator::eval(): start loop");

    if (!funcIter->isValid()) {  // should not happen
      return sum;
    }

    while (true) {
      CGLOG("FullGridTensorEvaluator::eval(): in loop");
      // get function value and partial product and multiply them together with the last basis
      // coefficient, then add the resulting value to the total sum
      double value = funcIter->value();
      V vec = partialProducts[lastDim];
      vec.componentwiseMult(basisValues[lastDim][it.indexAt(lastDim)]);
      vec.scalarMult(value);
      sum.add(vec);

      // increment iterator
      int h = funcIter->moveToNext();

      CGLOG("FullGridTensorEvaluator::eval(): moveToNext() == " << h);

      // check if not only the last dimension index changed
      if (h != 0) {
        if (h < 0) {
          break;  // all indices have been traversed, stop iteration and return sum
        } else {
          // more than the last index have changed, thus update partialProducts
          for (size_t d = lastDim - h; d < lastDim; ++d) {
            auto pp = partialProducts[d];  // TODO(holzmudd): could probably be optimized...
            pp.componentwiseMult(basisValues[d][it.indexAt(d)]);
            partialProducts[d + 1] = pp;
          }
        }
      }
    }

    return sum;
  }

  /**
   * @return Returns the function value storage.
   */
  std::shared_ptr<AbstractCombigridStorage> getStorage() { return storage; }

  /**
   * Sets the parameters for the evaluators. Each dimension in which the evaluator does not need a
   * parameter is skipped.
   * So if only the evaluators at dimensions 1 and 3 need a parameter, params.size() should be 2 (or
   * at least 2)
   */
  void setParameters(std::vector<V> const &params) {
    size_t numDimensions = evaluatorPrototypes.size();

    parameters = params;

    size_t paramIndex = 0;

    // we can't just set the parameters to the prototypes because the prototypes might be identical
    // (the pointer to one prototype might be duplicated)
    for (size_t d = 0; d < numDimensions; ++d) {
      auto &prototype = evaluatorPrototypes[d];

      if (prototype->needsParameter()) {
        // prototype->setParameter(params[paramIndex]); <- this is useless, see above
        for (auto &eval : evaluators[d]) {
          eval->setParameter(params[paramIndex]);
        }

        ++paramIndex;  // TODO(holzmudd): check bounds
      }
    }
  }

  /**
   * @return an estimate (upper bound, in the case of nested points normally exact) of the number of
   * new function evaluations (grid points) that have to be performed when evaluating this level.
   */
  virtual size_t maxNewPoints(MultiIndex const &level) {
    size_t result = 1;

    for (size_t d = 0; d < pointHierarchies.size(); ++d) {
      size_t currentLevel = level[d];
      size_t levelPoints = pointHierarchies[d]->getNumPoints(currentLevel);

      if (!pointHierarchies[d]->isNested() || currentLevel == 0) {
        result *= levelPoints;
      } else {
        result *= (levelPoints - pointHierarchies[d]->getNumPoints(currentLevel - 1));
      }
    }

    return result;
  }

  /**
   * @return the total number of grid points in a given level.
   */
  virtual size_t numPoints(MultiIndex const &level) {
    size_t result = 1;

    for (size_t d = 0; d < pointHierarchies.size(); ++d) {
      size_t currentLevel = level[d];
      size_t levelPoints = pointHierarchies[d]->getNumPoints(currentLevel);
      result *= levelPoints;
    }

    return result;
  }
};
}  // namespace combigrid
} /* namespace sgpp*/

#endif /* COMBIGRID_SRC_SGPP_COMBIGRID_OPERATION_MULTIDIM_FULLGRIDTENSOREVALUATOR_HPP_ */