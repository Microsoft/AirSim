// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef commn_utils_type_utils_hpp
#define commn_utils_type_utils_hpp

#include <type_traits>
#include <valarray>

namespace common_utils
{
namespace type_utils
{
    //from: https://raw.githubusercontent.com/louisdx/cxx-prettyprint/master/prettyprint.hpp
    //also see https://gist.github.com/louisdx/1076849
    namespace detail
    {
        // SFINAE type trait to detect whether T::const_iterator exists.

        struct sfinae_base
        {
            using yes = char;
            using no = yes[2];
        };

        template <typename T>
        struct has_const_iterator : private sfinae_base
        {
        private:
            template <typename C>
            static yes& test(typename C::const_iterator*);
            template <typename C>
            static no& test(...);

        public:
            static const bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
            using type = T;

            void dummy(); //for GCC to suppress -Wctor-dtor-privacy
        };

        template <typename T>
        struct has_begin_end : private sfinae_base
        {
        private:
            template <typename C>
            static yes& f(typename std::enable_if<
                          std::is_same<decltype(static_cast<typename C::const_iterator (C::*)() const>(&C::begin)),
                                       typename C::const_iterator (C::*)() const>::value>::type*);

            template <typename C>
            static no& f(...);

            // using template parameter symbol 'D' instead of 'C' as a workaround for
            // VS2017 compiler issue (internal compiler error) starting 15.9.X releases.
            template <typename D>
            static yes& g(typename std::enable_if<
                          std::is_same<decltype(static_cast<typename D::const_iterator (D::*)() const>(&D::end)),
                                       typename D::const_iterator (D::*)() const>::value,
                          void>::type*);

            template <typename D>
            static no& g(...);

        public:
            static bool const beg_value = sizeof(f<T>(nullptr)) == sizeof(yes);
            static bool const end_value = sizeof(g<T>(nullptr)) == sizeof(yes);

            void dummy(); //for GCC to suppress -Wctor-dtor-privacy
        };

    } // namespace detail

    // Basic is_container template; specialize to derive from std::true_type for all desired container types

    template <typename T>
    struct is_container : public std::integral_constant<bool, detail::has_const_iterator<T>::value && detail::has_begin_end<T>::beg_value && detail::has_begin_end<T>::end_value>
    {
    };

    template <typename T, std::size_t N>
    struct is_container<T[N]> : std::true_type
    {
    };

    template <std::size_t N>
    struct is_container<char[N]> : std::false_type
    {
    };

    template <typename T>
    struct is_container<std::valarray<T>> : std::true_type
    {
    };

    template <typename T1, typename T2>
    struct is_container<std::pair<T1, T2>> : std::true_type
    {
    };

    template <typename... Args>
    struct is_container<std::tuple<Args...>> : std::true_type
    {
    };
}
} //namespace
#endif
