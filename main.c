#include "config.h"
#include "config.h"

#if 0
#error never seen
#endif

#if VERSION >= 2 && defined(ZUPAFAST)
int mode = ZUPAFAST;
#elif VERSION >= 2
int mode = 2;
#else
int mode = 1;
#endif

const char* where = __FILE__;
int line = __LINE__;

#ifdef DEBUG
#warning dbg
#endif

#if VERSION > 1 +
#endif
