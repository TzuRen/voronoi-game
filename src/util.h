#pragma once

#include <cmath>
#include <set>
#include <unordered_set>
#include <vector>
#include <list>
#include <iterator>

#ifdef MSVC
#define __attribute__()
#endif

template<typename T>
inline T deg2rad(T deg) {
  return (deg * static_cast<T>(M_PI)) / static_cast<T>(180.0);
}

template<typename T>
inline T rad2deg(T rad) {
  return (rad * static_cast<T>(180.0)) / static_cast<T>(M_PI);
}

// Pre-computed 45-deg Euler rotation matrices for Z-rotation
const float ANGLE_DEG = 45.0f;
const float pangle = deg2rad(ANGLE_DEG);
const float nangle = -pangle;

#if 0
#include <opencv2/core/core.hpp>

inline cv::Point2f rotateZ2f_neg(cv::Point2f pt) {
  static const cv::Mat rotZn = cv::Mat(cv::Matx22f({
    cosf(nangle), -sinf(nangle), /* 0, */
    sinf(nangle),  cosf(nangle), /* 0, */
    /*         0,             0,    1, */
  }));
  cv::Mat ans = cv::Mat(cv::Matx12f({pt.x, pt.y})) * rotZn;
  return cv::Point2f(ans.at<float>(0u, 0u), ans.at<float>(0u, 1u));
}

inline cv::Point2f rotateZ2f_pos(cv::Point2f pt) {
  static const cv::Mat rotZp = cv::Mat(cv::Matx22f({
    cosf(pangle), -sinf(pangle), /* 0, */
    sinf(pangle),  cosf(pangle), /* 0, */
    /*         0,             0,    1, */
  }));
  cv::Mat ans = cv::Mat(cv::Matx12f({pt.x, pt.y})) * rotZp;
  return cv::Point2f(ans.at<float>(0u, 0u), ans.at<float>(0u, 1u));
}
#endif

#include <random>
#include <ctime>
#include <type_traits>
#ifdef DEBUG
#include <iostream>
#endif

typedef std::default_random_engine rng_type;
extern rng_type* rng;

template<class Type>
inline Type randrange(Type min, Type max)
{
  if (rng == nullptr)
  {
    auto seed = time(NULL);
    rng = new rng_type(seed);
  }

  typedef typename std::conditional<std::is_integral<Type>::value,
      std::uniform_int_distribution<Type>,
      std::uniform_real_distribution<Type>
    >::type distribution_type;

  distribution_type distribution(min, max);
  return distribution(*rng);
}

#ifdef DEBUG
namespace std
{

template<typename T, typename C, typename A>
ostream& operator<<(ostream& os, set<T, C, A> const& s) {
  os << "set<" << endl;
  for (auto it = s.begin(); it != s.end(); ++it)
    os << "  " << *it << endl;
  os << ">";
  return os;
}

template<typename T>
ostream& operator<<(ostream& os, unordered_set<T> const& s) {
  os << "set{" << endl;
  for (auto it = s.begin(); it != s.end(); ++it)
    os << *it << endl;
  os << "}";
  return os;
}

template<typename T>
ostream& operator<<(ostream& os, vector<T> const& v) {
  os << "[ ";
  for (auto it = v.begin(); it != v.end(); ++it)
    os << *it << ", ";
  os << " ]";
  return os;
}

template<typename T>
ostream& operator<<(ostream& os, list<T> const& l) {
  os << "[ ";
  for (auto it = l.begin(); it != l.end(); ++it)
    os << *it << ", ";
  os << " ]";
  return os;
}

} // end namespace std


template <class Container>
  class push_insert_iterator:
    public std::iterator<std::output_iterator_tag,void,void,void,void>
{
protected:
  Container* container;

public:
  typedef Container container_type;
  explicit push_insert_iterator(Container& x) : container(&x) {}
  push_insert_iterator<Container>& operator=(
      typename Container::const_reference value)
    { container->push(value); return *this; }
  push_insert_iterator<Container>& operator* (){ return *this; }
  push_insert_iterator<Container>& operator++ (){ return *this; }
  push_insert_iterator<Container> operator++ (int){ return *this; }
};

template<typename Container>
push_insert_iterator<Container> push_inserter(Container& container){
  return push_insert_iterator<Container>(container);
}

template <class Container>
  class emplace_insert_iterator:
    public std::iterator<std::output_iterator_tag,void,void,void,void>
{
protected:
  Container* container;

public:
  typedef Container container_type;
  explicit emplace_insert_iterator(Container& x) : container(&x) {}
  template<typename Tp_>
  emplace_insert_iterator<Container>& operator=(Tp_ &&value)
    { container->emplace(value); return *this; }
  emplace_insert_iterator<Container>& operator* (){ return *this; }
  emplace_insert_iterator<Container>& operator++ (){ return *this; }
  emplace_insert_iterator<Container> operator++ (int){ return *this; }
};

template<typename Container>
emplace_insert_iterator<Container> emplace_inserter(Container& container){
  return emplace_insert_iterator<Container>(container);
}

#endif // DEBUG
