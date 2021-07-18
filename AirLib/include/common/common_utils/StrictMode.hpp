#ifndef common_StrictMode_hpp
#define common_StrictMode_hpp

#if defined(_MSC_VER)
//'=': conversion from 'double' to 'float', possible loss of data
#define STRICT_MODE_OFF     \
    __pragma(warning(push)) \
        __pragma(warning(disable : 4100 4189 4244 4245 4239 4267 4365 4464 4456 4505 4514 4571 4624 4625 4626 4668 4701 4710 4820 4917 5026 5027 5031))
#define STRICT_MODE_ON \
    __pragma(warning(pop))

//TODO: limit scope of below statements required to suppress VC++ warnings
#define _CRT_SECURE_NO_WARNINGS 1
#pragma warning(disable : 4996)
#endif

// special way to quiet the warning:  warning: format string is not a string literal
#ifdef __CLANG__
#define IGNORE_FORMAT_STRING_ON      \
    _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wformat-nonliteral\"")

#define IGNORE_FORMAT_STRING_OFF \
    _Pragma("clang diagnostic pop")
#else
#define IGNORE_FORMAT_STRING_ON
#define IGNORE_FORMAT_STRING_OFF
#endif

// Please keep this list sorted so it is easier to find stuff, also make sure there
// is no whitespace after the traling \, GCC doesn't like that.
#ifdef __CLANG__
#define STRICT_MODE_OFF                                                                                   \
    _Pragma("clang diagnostic push")                                                                      \
        _Pragma("clang diagnostic ignored \"-Wctor-dtor-privacy\"")                                       \
            _Pragma("clang diagnostic ignored \"-Wdelete-non-virtual-dtor\"")                             \
                _Pragma("clang diagnostic ignored \"-Wmissing-field-initializers\"")                      \
                    _Pragma("clang diagnostic ignored \"-Wold-style-cast\"")                              \
                        _Pragma("clang diagnostic ignored \"-Wredundant-decls\"")                         \
                            _Pragma("clang diagnostic ignored \"-Wreturn-type\"")                         \
                                _Pragma("clang diagnostic ignored \"-Wshadow\"")                          \
                                    _Pragma("clang diagnostic ignored \"-Wstrict-overflow\"")             \
                                        _Pragma("clang diagnostic ignored \"-Wswitch-default\"")          \
                                            _Pragma("clang diagnostic ignored \"-Wundef\"")               \
                                                _Pragma("clang diagnostic ignored \"-Wunused-variable\"") \
                                                    _Pragma("clang diagnostic ignored \"-Wcast-qual\"")   \
                                                        _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")

/* Addition options that can be enabled
_Pragma("clang diagnostic ignored \"-Wpedantic\"")                \
_Pragma("clang diagnostic ignored \"-Wformat=\"")                 \
_Pragma("clang diagnostic ignored \"-Werror\"")                   \
_Pragma("clang diagnostic ignored \"-Werror=\"")                  \
_Pragma("clang diagnostic ignored \"-Wunused-variable\"")         \
*/

#define STRICT_MODE_ON \
    _Pragma("clang diagnostic pop")

#else
#ifdef __GNUC__
#define STRICT_MODE_OFF                                                                                 \
    _Pragma("GCC diagnostic push")                                                                      \
        _Pragma("GCC diagnostic ignored \"-Wctor-dtor-privacy\"")                                       \
            _Pragma("GCC diagnostic ignored \"-Wdelete-non-virtual-dtor\"")                             \
                _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"")                      \
                    _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")                              \
                        _Pragma("GCC diagnostic ignored \"-Wredundant-decls\"")                         \
                            _Pragma("GCC diagnostic ignored \"-Wreturn-type\"")                         \
                                _Pragma("GCC diagnostic ignored \"-Wshadow\"")                          \
                                    _Pragma("GCC diagnostic ignored \"-Wstrict-overflow\"")             \
                                        _Pragma("GCC diagnostic ignored \"-Wswitch-default\"")          \
                                            _Pragma("GCC diagnostic ignored \"-Wundef\"")               \
                                                _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") \
                                                    _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")   \
                                                        _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")

/* Addition options that can be enabled
_Pragma("GCC diagnostic ignored \"-Wpedantic\"")                \
_Pragma("GCC diagnostic ignored \"-Wformat=\"")                 \
_Pragma("GCC diagnostic ignored \"-Werror\"")                   \
_Pragma("GCC diagnostic ignored \"-Werror=\"")                  \
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"")         \
_Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")		\

*/

#define STRICT_MODE_ON \
    _Pragma("GCC diagnostic pop")
#endif

#endif

#endif