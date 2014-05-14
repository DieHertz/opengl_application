#ifndef NOEXCEPT
#ifndef _MSC_VER
#define NOEXCEPT noexcept
#else
#define NOEXCEPT throw()
#endif
#endif
