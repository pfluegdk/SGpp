// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org
#ifndef MPIOPERATIONFACTORY_H
#define MPIOPERATIONFACTORY_H
#include <sgpp/datadriven/operation/hash/OperationMPI/OperationCreateGraphMPI.hpp>
#include <sgpp/datadriven/operation/hash/OperationMPI/OperationRhsMPI.hpp>

namespace sgpp {
namespace datadriven {
namespace clusteringmpi {

MPISlaveOperation* create_mpi_operation(char *classname) {
  std::cout << classname << std::endl;
  if (std::strcmp(classname, "N4sgpp10datadriven13clusteringmpi25OperationCreateGraphSlave")
      == 0)  {
    std::cout << "blub1" << std::endl;
    return new OperationCreateGraphSlave();
  }
  if (std::strcmp(classname, "N4sgpp10datadriven13clusteringmpi17OperationRhsSlave") == 0)  {
    std::cout << "blub2" << std::endl;
    return new OperationRhsSlave();
  }
  return NULL;
}

}  // namespace clusteringmpi
}  // namespace datadriven
}  // namespace sgpp
#endif /* MPIOPERATIONFACTORY_H */
