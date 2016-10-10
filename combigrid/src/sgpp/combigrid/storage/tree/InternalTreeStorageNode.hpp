// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef COMBIGRID_SRC_SGPP_COMBIGRID_STORAGE_TREE_INTERNALTREESTORAGENODE_HPP_
#define COMBIGRID_SRC_SGPP_COMBIGRID_STORAGE_TREE_INTERNALTREESTORAGENODE_HPP_

#include <sgpp/combigrid/storage/tree/AbstractTreeStorageNode.hpp>
#include <sgpp/combigrid/storage/tree/LowestTreeStorageNode.hpp>

#include <vector>

namespace sgpp {
namespace combigrid {

/**
 * Each inner node of the storage contains a vector of other nodes.
 * For a d-dimensional multi-index, to find a storage entry, the components of the multi-index are
 * iterated over.
 * Each component defines the index of the next child to be traversed.
 * Because traversal would be slower if each leaf only contained one number,
 * the nodes for the last dimension contain a vector of numbers (or Ts) instead of having children
 * which themselves have one number.
 * These nodes are represented by LowestTreeStorageNode objects, all other nodes are
 * InternalTreeStorageNode objects.
 */
template <typename T>
class InternalTreeStorageNode : public AbstractTreeStorageNode<T> {
  /**
   * Returns a Multi-index containing only the first depth+1 entries of index.
   */
  MultiIndex restrictIndex(MultiIndex const &index, size_t depth) {
    MultiIndex result(depth + 1);
    for (size_t i = 0; i <= depth; ++i) {
      result[i] = index[i];
    }
    return result;
  }

 public:
  std::vector<std::unique_ptr<AbstractTreeStorageNode<T>>> children;
  TreeStorageContext<T> &context;

  /**
   * @param context TreeStorageContext<T>-object containing information about the storage.
   * @param remainingDimensions Number of dimensions that come after this node (at least 1, since
   * this is an internal node).
   * @param index Multi-index at which the
   */
  InternalTreeStorageNode(TreeStorageContext<T> &context, size_t remainingDimensions,
                          MultiIndex const &index, size_t depth, size_t zeroDepth,
                          bool createFirstChild = true)
      : children(), context(context) {
    if (!createFirstChild) {
      return;
    }

    if (remainingDimensions > 1) {
      children.emplace_back(new InternalTreeStorageNode<T>(context, remainingDimensions - 1, index,
                                                           depth + 1, zeroDepth, createFirstChild));
    } else {
      children.emplace_back(new LowestTreeStorageNode<T>(context, index, depth + 1, zeroDepth));
    }
  }

  virtual ~InternalTreeStorageNode() {}

  virtual size_t numChildren() const { return children.size(); }

  virtual bool isLeaf() const { return false; }

  virtual T &get(MultiIndex const &index, size_t depth = 0) {
    size_t currentIndex = index[depth];
    size_t remainingDimensions = context.numDimensions - depth - 1;

    while (currentIndex >= children.size()) {
      MultiIndex restrictedIndex = restrictIndex(index, depth);
      restrictedIndex[depth] = children.size();
      bool createFirstChild = true;
      if (remainingDimensions >= 2) {
        children.emplace_back(new InternalTreeStorageNode<T>(
            context, remainingDimensions - 1, index, depth + 1, depth, createFirstChild));
      } else {
        children.emplace_back(new LowestTreeStorageNode<T>(context, index, depth + 1, depth));
      }
    }
    return children[currentIndex]->get(index, depth + 1);
  }

  virtual void set(MultiIndex const &index, T const &value, size_t depth = 0) {
    size_t currentIndex = index[depth];
    size_t remainingDimensions = context.numDimensions - depth - 1;

    while (currentIndex >= children.size()) {
      MultiIndex restrictedIndex = restrictIndex(index, depth);
      restrictedIndex[depth] = children.size();
      bool createFirstChild =
          (currentIndex != children.size());  // if this is the child to set, then it will be
                                              // created in the recursion or by setting the value
      if (remainingDimensions >= 2) {
        children.emplace_back(new InternalTreeStorageNode<T>(
            context, remainingDimensions - 1, index, depth + 1, depth, createFirstChild));
      } else {
        // TODO(holzmudd) check depth, depth+1 and not depth+1, depth
        children.emplace_back(new LowestTreeStorageNode<T>(context, index, depth + 1, depth));
      }
    }

    children[currentIndex]->set(index, value, depth + 1);
  }

  virtual bool containsIndex(MultiIndex const &index, size_t depth = 0) const {
    return index[depth] < children.size() &&
           children[index[depth]]->containsIndex(index, depth + 1);
  }
};
}  // namespace combigrid
} /* namespace sgpp*/

#endif /* COMBIGRID_SRC_SGPP_COMBIGRID_STORAGE_TREE_INTERNALTREESTORAGENODE_HPP_ */
