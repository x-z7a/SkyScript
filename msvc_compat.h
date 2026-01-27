// MSVC compatibility header
// Force-included before all other headers to handle GCC-specific syntax

#ifndef MSVC_COMPAT_H
#define MSVC_COMPAT_H

#ifdef _MSC_VER
// MSVC doesn't support GCC's __attribute__ syntax
#define __attribute__(x)
#endif

#endif // MSVC_COMPAT_H
