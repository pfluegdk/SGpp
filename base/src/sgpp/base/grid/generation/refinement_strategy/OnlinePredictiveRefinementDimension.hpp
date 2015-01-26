// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at 
// sgpp.sparsegrids.org

#ifndef ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_
#define ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_

#include "RefinementDecorator.hpp"
#include <sgpp/base/grid/generation/hashmap/AbstractRefinement.hpp>
#include <sgpp/base/grid/generation/functors/PredictiveRefinementDimensionIndicator.hpp>
#include <vector>
#include <utility>

//#include "sgpp_datadriven.hpp"
#include <sgpp/base/operation/BaseOpFactory.hpp>
#include <sgpp/base/operation/OperationMultipleEval.hpp>
#include <sgpp/base/exception/application_exception.hpp>
#include <sgpp/parallel/tools/TypesParallel.hpp>


#include <sgpp/globaldef.hpp>


namespace SGPP {
namespace base {

class OnlinePredictiveRefinementDimension: public virtual RefinementDecorator {
	friend class LearnerOnlineSGD;
public:

  typedef std::pair<size_t, size_t> key_type; // gred point seq number and dimension
  typedef double value_type; // refinement functor value

  typedef std::pair<size_t, unsigned int> refinement_key;
  typedef std::map<refinement_key, double> refinement_map;


  OnlinePredictiveRefinementDimension(AbstractRefinement* refinement, size_t dim): RefinementDecorator(refinement), iThreshold_(0.0),
		  trainDataset(NULL), classes(NULL), errors(NULL){
	  predictiveGrid_ = Grid::createLinearGrid(dim);
	  GridGenerator* predictiveGridGenerator = predictiveGrid_->createGridGenerator();
	  predictiveGridGenerator->regular(2);
	  delete predictiveGridGenerator;
  };


  virtual ~OnlinePredictiveRefinementDimension(){
	  delete predictiveGrid_;
  }

  void free_refine(GridStorage* storage, RefinementFunctor* functor);

	virtual void collectRefinablePoints(GridStorage* storage,
				size_t refinements_num, refinement_map* result);

	bool hasLeftChild(GridStorage* storage, GridIndex* gridIndex, size_t dim);
	bool hasRightChild(GridStorage* storage, GridIndex* gridIndex, size_t dim);

	void setTrainDataset(DataMatrix* trainDataset_);
	void setErrors(DataVector* errors_);

	// For the Python test case
	double basisFunctionEvalHelper(unsigned int level, unsigned int index, double value);

protected:
	virtual void refineGridpointsCollection(GridStorage* storage,
	    RefinementFunctor* functor, size_t refinements_num, size_t* max_indices,
	    PredictiveRefinementDimensionIndicator::value_type* max_values);

	virtual size_t getIndexOfMin(PredictiveRefinementDimensionIndicator::value_type* array,
	    size_t length);

private:
	double iThreshold_;
	std::map<key_type, value_type> refinementCollection_;
	//virtual static bool refinementPairCompare(std::pair<key_type, value_type>& firstEl, std::pair<key_type, value_type>& secondEl);

	DataMatrix* trainDataset;
	DataVector* classes;
	DataVector* errors;
	Grid* predictiveGrid_;

	static const SGPP::parallel::VectorizationType vecType_;

};

} /* namespace base */
} /* namespace SGPP */
#endif /* ONLINEPREDICTIVEREFINEMENTDIMENSION_HPP_ */