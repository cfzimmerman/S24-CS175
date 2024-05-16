#ifndef RIGTFORM_H
#define RIGTFORM_H

#include <cassert>
#include <iostream>

#include "cvec.h"
#include "matrix4.h"
#include "quat.h"

class RigTForm {
  Cvec3 t_; // translation component
  Quat r_;  // rotation component represented as a quaternion
            //
  const double FLOAT_E = 0.000001;

public:
  RigTForm() : t_(0) { assert(norm2(Quat(1, 0, 0, 0) - r_) < CS175_EPS2); }

  RigTForm(const Cvec3 &t, const Quat &r) {
    t_ = t;
    r_ = r;
  }

  // I got a compiler error that RigTForms cannot be copied. This prevented
  // using the assignment operator. I'm using this instead to overwrite one
  // RigTForm into another.
  RigTForm &consume(RigTForm other) {
    t_ = std::move(other.t_);
    r_ = std::move(other.r_);
    return *this;
  }

  explicit RigTForm(const Cvec3 &t) {
    t_ = t;
    r_ = Quat();
  }

  explicit RigTForm(const Quat &r) {
    t_ = Cvec3();
    r_ = r;
  }

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
    // book page 70
    Cvec3 rotated = r_ * Cvec3(a);
    if (a(3) < FLOAT_E) {
      // Assume it's a vector
      return Cvec4(rotated, 0.);
    }
    // Assume it's a point
    rotated += t_;
    return Cvec4(rotated, a(3));
  }

  RigTForm operator*(const RigTForm &a) const {
    // book page 70
    return RigTForm(t_ + r_ * a.t_, r_ * a.r_);
  }
};

inline RigTForm inv(const RigTForm &tform) {
  // book page 71
  Quat invRot = inv(tform.getRotation());
  return RigTForm(invRot * tform.getTranslation() * -1, invRot);
}

inline RigTForm transFact(const RigTForm &tform) {
  return RigTForm(tform.getTranslation());
}

inline RigTForm linFact(const RigTForm &tform) {
  return RigTForm(tform.getRotation());
}

inline Matrix4 rigTFormToMatrix(const RigTForm &tform) {
  // Lecture 8, page 5
  return Matrix4::makeTranslation(tform.getTranslation()) *
         quatToMatrix(tform.getRotation());
}

#endif
