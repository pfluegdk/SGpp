// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at 
// sgpp.sparsegrids.org

#include <sgpp/base/grid/Grid.hpp>
#include <sgpp/base/grid/type/ModPolyGrid.hpp>

#include <sgpp/base/grid/generation/StandardGridGenerator.hpp>

#include <sgpp/base/exception/factory_exception.hpp>

#include <sgpp/base/basis/modpoly/ModifiedPolyBasis.hpp>



#include <iostream>

#include <sgpp/globaldef.hpp>


namespace SGPP {
  namespace base {

    ModPolyGrid::ModPolyGrid(std::istream& istr) : Grid(istr), degree(1 << 16), basis_(NULL) {
      istr >> degree;
    }


    ModPolyGrid::ModPolyGrid(size_t dim, size_t degree) : degree(degree), basis_(NULL) {
      this->storage = new GridStorage(dim);
    }

    ModPolyGrid::~ModPolyGrid() {
    	if (basis_ != NULL){
    		delete basis_;
    	}
    }

    const char* ModPolyGrid::getType() {
      return "modpoly";
    }

    const SBasis& ModPolyGrid::getBasis(){
    	if (basis_ == NULL){
    		basis_ = new SModPolyBase(degree);
    	}
		return *basis_;
	}

    size_t ModPolyGrid::getDegree() const {
      return this->degree;
    }

    Grid* ModPolyGrid::unserialize(std::istream& istr) {
      return new ModPolyGrid(istr);
    }

    void ModPolyGrid::serialize(std::ostream& ostr) {
      this->Grid::serialize(ostr);
      ostr << degree << std::endl;
    }


    /**
     * Creates new GridGenerator
     * This must be changed if we add other storage types
     */
    GridGenerator* ModPolyGrid::createGridGenerator() {
      return new StandardGridGenerator(this->storage);
    }


  }
}