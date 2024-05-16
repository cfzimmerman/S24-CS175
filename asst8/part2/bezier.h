#include "cvec.h"
#include "quat.h"
#include <iostream>

struct BezierControls {
  double c0;
  double d0;
  double e0;
  double c1;

  BezierControls(double c_0, double d_0, double e_0, double c_1)
      : c0(c_0), d0(d_0), e0(e_0), c1(c_1) {}

  double apply(double alpha) {
    double pow3 = this->c0 * pow(1. - alpha, 3.);
    double pow2 = 3. * this->d0 * alpha * pow(1. - alpha, 2);
    double pow1 = 3. * this->e0 * pow(alpha, 2.) * (1. - alpha);
    double pow0 = this->c1 * pow(alpha, 3.);
    return pow3 + pow2 + pow1 + pow0;
  }
};

class Bezier {
public:
  static Cvec3 interp(const Cvec3 &before_left, const Cvec3 &left,
                      const Cvec3 &right, const Cvec3 &after_right,
                      double alpha) {
    Cvec3 res;
    for (size_t pos = 0; pos < 3; pos++) {
      double d = (1. / 6.) * (right(pos) - before_left(pos)) + left(pos);
      double e = (-1. / 6.) * (after_right(pos) - left(pos)) + right(pos);
      BezierControls ctrl(left(pos), d, e, right(pos));
      res(pos) = ctrl.apply(alpha);
    }
    return res;
  }

  static Quat interp(const Quat &before_left, const Quat &left,
                     const Quat &right, const Quat &after_right, double alpha) {
    Quat d = (right * (before_left.pow(-1.))).cn()->pow(1. / 6.) * left;
    Quat e = (after_right * (left.pow(-1.))).cn()->pow(-1. / 6.) * right;

    Quat res;
    for (size_t pos = 0; pos < 3; pos++) {
      BezierControls ctrl(left(pos), d(pos), e(pos), right(pos));
      res(pos) = ctrl.apply(alpha);
    }
    return res;
  }
};
