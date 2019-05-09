// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#pragma once

#include <sgpp/base/tools/Distribution.hpp>

#include <iostream>
#include <random>
#include <string>

namespace sgpp {
namespace base {

/**
 */
class DistributionLogNormal : public Distribution {
 public:
  /**
   * Constructor
   */
  DistributionLogNormal(double mean, double stddev)
      : Distribution(), mean(mean), stddev(stddev), dist(mean, stddev) {}

  /**
   * Destructor
   */
  virtual ~DistributionLogNormal() {}

  /**
   *
   */
  double sample() { return dist(gen); }

  /**
   *
   */
  double eval(double x) {
    if (x >= 0)
      // y =   1.0./(sigma2*x*sqrt(2*pi)) .* exp(-((log(x)-mu).^2)./(2*sigma2^2));
      return 1.0 / (stddev * x * sqrt(2 * M_PI)) *
             exp(-std::pow(log(x) - mean, 2) / (2 * stddev * stddev));
    else
      return 0.0;
  }

  /**
   * See DistributionNormal.cpp: 2*Phi(z)-1 of a normal distirbutions mass lie inside
   * the interval [mean-z*sigma,mean+z*sigma].
   * Therefore corresponding accuracies for logNormal distribution are achieved in an interval
   * [exp(mean-z*sigma),exp(mean+z*sigma)]
   * As for the normal distribution, we choose z=9 for precision of ~ 10^(-18)
   */
  sgpp::base::DataVector getBounds() {
    sgpp::base::DataVector bounds(2);
    bounds[0] = exp(mean - 9 * stddev);
    bounds[1] = exp(mean + 9 * stddev);
    return bounds;
  }

  std::string getType() {
    return std::string("LogNormal_") + std::to_string(mean) + std::string("_") +
           std::to_string(stddev);
  }

 private:
  double mean;
  double stddev;
  std::lognormal_distribution<double> dist;
};

}  // namespace base
}  // namespace sgpp
