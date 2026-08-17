#ifndef SLIC3R_BOOST_GCC_CONFIG_HPP
#define SLIC3R_BOOST_GCC_CONFIG_HPP

#include <boost/config/compiler/gcc.hpp>

#if defined(BOOST_HAS_FLOAT128) && defined(__has_include)
#if !__has_include(<quadmath.h>)
#undef BOOST_HAS_FLOAT128
#undef _GLIBCXX_USE_FLOAT128
#endif
#endif

#endif
