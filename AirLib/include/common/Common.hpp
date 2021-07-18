// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_Common_hpp
#define msr_airlib_Common_hpp

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cstdint>
#include "common/common_utils/Utils.hpp"
#include "common_utils/RandomGenerator.hpp"
#include "VectorMath.hpp"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#define RpcLibPort 41451

namespace msr
{
namespace airlib
{

    //numericals
    typedef float real_T;
    //this is not required for most compilers
    typedef unsigned int uint;

    //well known types
    typedef msr::airlib::VectorMathf VectorMath;
    typedef VectorMath::Vector3f Vector3r;
    typedef VectorMath::Vector2f Vector2r;
    typedef VectorMath::Vector1f Vector1r;
    typedef VectorMath::Array3f Array3r;
    typedef VectorMath::Pose Pose;
    typedef VectorMath::Quaternionf Quaternionr;
    typedef VectorMath::Matrix3x3f Matrix3x3r;
    typedef VectorMath::AngleAxisf AngleAxisr;
    typedef common_utils::RandomGeneratorF RandomGeneratorR;
    typedef common_utils::RandomGeneratorGaussianF RandomGeneratorGausianR;
    typedef std::string string;
    typedef common_utils::Utils Utils;
    typedef VectorMath::RandomVectorGaussianT RandomVectorGaussianR;
    typedef VectorMath::RandomVectorT RandomVectorR;
    typedef uint64_t TTimePoint;
    typedef double TTimeDelta;

    template <typename T>
    using vector = std::vector<T>;
    template <typename TKey, typename TValue>
    using unordered_map = std::unordered_map<TKey, TValue>;
    template <typename TKey>
    using unordered_set = std::unordered_set<TKey>;
    template <typename T>
    using unique_ptr = std::unique_ptr<T>;
    template <typename T>
    using shared_ptr = std::shared_ptr<T>;
    template <typename T>
    using vector_size_type = typename std::vector<T>::size_type;

    inline std::ostream& operator<<(std::ostream& os, Quaternionr const& q)
    {
        float p, r, y;
        VectorMath::toEulerianAngle(q, p, r, y);
        return os << "(" << r << "\t" << p << "\t" << y << ")" << q.w() << q.x() << "\t" << q.y() << "\t" << q.z() << "\t";
    }

    inline std::ostream& operator<<(std::ostream& os, Vector3r const& vec)
    {
        return os << vec.x() << "\t" << vec.y() << "\t" << vec.z() << "\t";
    }
}
} //namespace
#endif
