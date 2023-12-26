#ifndef _JASSERT_H_
#define _JASSERT_H_

#include <stdio.h>
#include <stdlib.h>

#define jassert(e)  \
    ((void) ((e) ? ((void)0) : __jassert (#e, __FILE__, __LINE__)))
#define __jassert(e, file, line) \
    ((void)printf ("%s:%d: failed jassertion `%s'\n", file, line, e), abort())

#endif // _JASSERT_H_
