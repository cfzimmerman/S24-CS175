#include <algorithm>
#include <cassert>
#include <memory>

#include "scenegraph.h"

using namespace std;

bool SgTransformNode::accept(SgNodeVisitor &visitor) {
  if (!visitor.visit(*this))
    return false;
  for (int i = 0, n = children_.size(); i < n; ++i) {
    if (!children_[i]->accept(visitor))
      return false;
  }
  return visitor.postVisit(*this);
}

void SgTransformNode::addChild(shared_ptr<SgNode> child) {
  children_.push_back(child);
}

void SgTransformNode::removeChild(shared_ptr<SgNode> child) {
  children_.erase(find(children_.begin(), children_.end(), child));
}

bool SgShapeNode::accept(SgNodeVisitor &visitor) {
  if (!visitor.visit(*this))
    return false;
  return visitor.postVisit(*this);
}

class RbtAccumVisitor : public SgNodeVisitor {
protected:
  vector<RigTForm> rbtStack_;
  SgTransformNode &target_;
  bool found_;

public:
  RbtAccumVisitor(SgTransformNode &target)
      : rbtStack_(1, RigTForm()), target_(target), found_(false) {}

  const RigTForm getAccumulatedRbt(int offsetFromStackTop = 0) {
    int ind = rbtStack_.size() - 1 - offsetFromStackTop;
    assert(0 <= ind);
    return rbtStack_[ind];
  }

  virtual bool visit(SgTransformNode &node) {
    rbtStack_.push_back(rbtStack_.back() * node.getRbt());
    found_ = node == target_;
    return !found_;
  }

  virtual bool postVisit(SgTransformNode &node) {
    if (!found_) {
      rbtStack_.pop_back();
    }
    return !found_;
  }
};

RigTForm getPathAccumRbt(shared_ptr<SgTransformNode> source,
                         shared_ptr<SgTransformNode> destination,
                         int offsetFromDestination) {

  // Ensure source and destination aren't null ptrs
  assert(source);
  assert(destination);

  RbtAccumVisitor accum(*destination);
  source->accept(accum);
  return accum.getAccumulatedRbt(offsetFromDestination);
}
