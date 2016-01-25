// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#include "CombigridLevelVector.hpp"


namespace combigrid {

  const int CombigridLevelVector::LEVELMAX = 128;

  CombigridLevelVector::CombigridLevelVector(int dim) {
    levelVec_.push_back(std::vector<int>(dim, LEVELMAX));
    coef_.push_back(1);
  }

  CombigridLevelVector::CombigridLevelVector(std::vector<int> level) {
    levelVec_.push_back(level);
    coef_.push_back(1);
  }

  CombigridLevelVector::CombigridLevelVector(std::vector<int> level, double coef) {
    levelVec_.push_back(level);
    coef_.push_back(coef);
  }
  CombigridLevelVector::CombigridLevelVector(std::vector<std::vector<int> > input, std::vector<double> coef) {
    levelVec_ = input;
    coef_ = coef;
    doAddition();
  }

  CombigridLevelVector& CombigridLevelVector::operator =(const CombigridLevelVector& rhs) {
    if (this == &rhs) return (*this);

    levelVec_ = rhs.getLevelVec();
    coef_ = rhs.getCoef();
    return (*this);
  }

  const CombigridLevelVector CombigridLevelVector::operator *(const CombigridLevelVector& b) const {
    std::vector< std::vector<int> > result;
    std::vector<int> buffer(b.getDim());
    std::vector<double> c;

    for (int i = 0; i < getN(); ++i) {
      for (int j = 0; j < b.getN(); ++j) {
        for (int k = 0; k < b.getDim(); ++k) {
          buffer[k] = levelVec_[i][k] < b.levelVec_[j][k] ? levelVec_[i][k] : b.levelVec_[j][k];
        }

        result.push_back(buffer);
        c.push_back(coef_[i]*b.getCoef()[j]);
      }
    }

    return CombigridLevelVector(result, c);
  }
  const CombigridLevelVector CombigridLevelVector::operator -(const CombigridLevelVector& b) const {
    CombigridLevelVector result(*this), inVec(b);

    for (int i = 0; i < b.getN(); ++i) {
      inVec.coef_[i] *= -1.0;
    }

    return result + inVec;
  }

  const CombigridLevelVector CombigridLevelVector::operator +(const CombigridLevelVector& b) const {
    CombigridLevelVector result(*this);

    for (int i = 0; i < b.getN(); ++i) {
      result.levelVec_.insert(result.levelVec_.end(), b.getLevelVec()[i]);
      result.coef_.insert(result.coef_.end(), b.getCoef()[i]);
    }

    result.doAddition();
    return result;
  }

  void CombigridLevelVector::doAddition() {
    for (int i = 0; i < (int)levelVec_.size(); ++i) {
      for (int j = i + 1; j < (int)levelVec_.size(); ++j) {
        bool same = true;

        for (int k = 0; k < getDim(); ++k) {
          if (levelVec_[i][k] != levelVec_[j][k]) {
            same = false;
            break;
          }
        }

        if (same) {
          coef_[i] += coef_[j];
          coef_[j] = 0.0;
        }
      }
    }

    for (int i = getN() - 1; i > -1; --i) {
      if (coef_[i] == 0.0) {
        levelVec_.erase(levelVec_.begin() + i);
        coef_.erase(coef_.begin() + i);
      }
    }
  }

  void CombigridLevelVector::printLevelVec() {
    for (int i = 0; i < getN(); ++i) {
      for (int j = 0; j < getDim(); ++j) {
        std::cout << levelVec_[i][j] << "\t";
      }

      std::cout << " | " << coef_[i] << std::endl;
    }
  }

  CombigridLevelVector CombigridLevelVector::getCombiLevels(std::vector<CombigridLevelVector> input) {
    CombigridLevelVector unity(input[0].getDim());
    CombigridLevelVector erg = unity - input[0];

    for (int i = 1; i < (int)input.size(); ++i) {
      erg = erg * (unity - input[i]);
    }

    erg = unity - erg;
    return erg;
  }

  CombigridLevelVector CombigridLevelVector::getCombiLevels(std::vector<std::vector<int> > input) {
    std::vector<CombigridLevelVector> buffer;

    for (int i = 0; i < (int)input.size(); ++i) {
      buffer.push_back(CombigridLevelVector(input[i]));
    }

    return getCombiLevels(buffer);

  }

  std::vector<CombigridLevelVector> CombigridLevelVector::split() {
    std::vector<CombigridLevelVector> buffer(0);

    for (int i = 0; i < getN(); ++i) {
      buffer.push_back(CombigridLevelVector(levelVec_[i], coef_[i]));
    }

    return buffer;
  }

  CombigridLevelVector CombigridLevelVector::getCombiLevels(CombigridLevelVector input) {
    input.doAddition();
    //  for (int i = 0; i < input.getN(); ++i) {
    //    if(input.getCoef()[i]!=1) {
    //      std::cout<<'will return NULL!'<<std::endl;
    //      return NULL;
    //    }
    //  }
    return getCombiLevels(input.split());

  }

  CombigridLevelVector CombigridLevelVector::getChanges(std::vector<int> input) {
    CombigridLevelVector inVector(input);
    CombigridLevelVector unity( static_cast<int>( input.size() ) );
    //  CombigridLevelVector current(levelVec_,coef_);
    std::vector<CombigridLevelVector> currentVec = (*this).split();

    for (unsigned int i = 0; i < levelVec_.size(); i++) {
      inVector = inVector * (unity - currentVec[i]);
    }

    inVector.doAddition();
    return inVector;

  }

  void CombigridLevelVector::update(std::vector<int> input) {
    (*this) = (*this) + (*this).getChanges(input);
  }

}