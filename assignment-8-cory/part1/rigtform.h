#ifndef RIGTFORM_H
#define RIGTFORM_H

#include <cassert>
#include <iostream>

#include "bezier.h"
#include "matrix4.h"
#include "quat.h"

class RigTForm {
  Cvec3 t_; // translation component
  Quat r_;  // rotation component represented as a quaternion

public:
  RigTForm() : t_(0) { assert(norm2(Quat(1, 0, 0, 0) - r_) < CS175_EPS2); }

  RigTForm(const Cvec3 &t, const Quat &r) : t_(t), r_(r) {}
  explicit RigTForm(const Cvec3 &t)
      : t_(t), r_() {} // only set translation part (rotation is identity)
  explicit RigTForm(const Quat &r)
      : t_(0), r_(r) {} // only set rotation part (translation is 0)

  Cvec3 getTranslation() const { return t_; }

  Quat getRotation() const { return r_; }

  RigTForm &setTranslation(const Cvec3 &t) {
    t_ = t;
    return *this;
  }

  RigTForm &setRotation(const Quat &r) {
    r_ = r;
    return *this;
  }

  Cvec4 operator*(const Cvec4 &a) const {
    return Cvec4(t_, 1.0) * a[3] + Cvec4(r_ * Cvec3(a));
  }

  RigTForm operator*(const RigTForm &a) const {
    return RigTForm(t_ + r_ * a.t_, r_ * a.r_);
  }
};

inline RigTForm inv(const RigTForm &tform) {
  Quat invRot = inv(tform.getRotation());
  return RigTForm(invRot * (-tform.getTranslation()), invRot);
}

inline RigTForm transFact(const RigTForm &tform) {
  return RigTForm(tform.getTranslation());
}

inline RigTForm linFact(const RigTForm &tform) {
  return RigTForm(tform.getRotation());
}

inline Matrix4 rigTFormToMatrix(const RigTForm &tform) {
  Matrix4 m = quatToMatrix(tform.getRotation());
  const Cvec3 t = tform.getTranslation();
  for (int i = 0; i < 3; ++i) {
    m(i, 3) = t(i);
  }
  return m;
}

inline RigTForm linear_interp(const RigTForm &start, const RigTForm &end,
                              double alpha) {
  if (alpha < 0) {
    fprintf(stderr, "Exiting lerp early, alpha < 0: %f.\n", alpha);
    return start;
  }
  if (alpha > 1) {
    fprintf(stderr, "Exiting lerp early, alpha > 1: %f.\n", alpha);
    return end;
  }
  Cvec3 lerp =
      start.getTranslation() * (1 - alpha) + end.getTranslation() * alpha;
  Quat slerp = Quat::slerp(start.getRotation(), end.getRotation(), alpha);
  return RigTForm(lerp, slerp);
}

inline RigTForm cubic_interp(const RigTForm &before_left, const RigTForm &left,
                             const RigTForm &right, const RigTForm &after_right,
                             double alpha) {
  if (alpha < 0) {
    fprintf(stderr, "Exiting lerp early, alpha < 0: %f.\n", alpha);
    return left;
  }
  if (alpha > 1) {
    fprintf(stderr, "Exiting lerp early, alpha > 1: %f.\n", alpha);
    return right;
  }
  Cvec3 trans = Bezier::interp(before_left.getTranslation(),
                               left.getTranslation(), right.getTranslation(),
                               after_right.getTranslation(), alpha);
  Quat rot =
      Bezier::interp(before_left.getRotation(), left.getRotation(),
                     right.getRotation(), after_right.getRotation(), alpha);
  return RigTForm(trans, rot);
}

#endif
