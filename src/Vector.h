//=======================================================================
// Copyright 2014-2015 David Simmons-Duffin.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================


#ifndef SDPB_VECTOR_H_
#define SDPB_VECTOR_H_

#include <assert.h>
#include <algorithm>
#include <iostream>
#include <ostream>
#include <vector>
#include "types.h"

using std::ostream;
using std::vector;

typedef vector<Real> Vector;

// print any vector<T>, including Vector
template <class T>
ostream& operator<<(ostream& os, const vector<T>& v) {
  os << "{";
  int last = v.size() - 1;
  for (int i = 0; i < last; i++)
    os << v[i] << ", ";
  if (last >= 0)
    os << v[last];
  os << "}";
  return os;
}

inline bool compareAbs(const Real &a, const Real &b) {
  return abs(a) < abs(b);
}

// The maximal absolute value of the components of v
inline Real maxAbsVector(const Vector &v) {
  return abs(*std::max_element(v.begin(), v.end(), compareAbs));
}

// v := (a, a, ..., a)
inline void fillVector(Vector &v, const Real &a) {
  std::fill(v.begin(), v.end(), a);
}

// v := a*v, where a is a constant
inline void scaleVector(Vector &v, const Real &a) {
  for (unsigned int i = 0; i < v.size(); i++)
    v[i] *= a;
}

// v := v + u
inline void addVector(Vector &v, const Vector &u) {
  assert(v.size() == u.size());

  for (unsigned int i = 0; i < v.size(); i++)
    v[i] += u[i];
}

// The smash product... just kidding. The dot product u.v.
inline Real dotProduct(const Vector &u, const Vector v) {
  Real result = 0;
  for (unsigned int i = 0; i < u.size(); i++)
    result += u[i]*v[i];
  return result;
}

// Return the component-wise product w = (v[0] u[0], ..., v[n] u[n])
// This routine is used only once, so we needn't worry about memory
// efficiency.
inline Vector multiplyVectors(const Vector &u, const Vector &v) {
  Vector w(u.size());
  for (unsigned int i = 0; i < w.size(); i++)
    w[i] = u[i]*v[i];
  return w;
}

#endif  // SDPB_VECTOR_H_
