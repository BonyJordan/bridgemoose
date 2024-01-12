#ifndef _LUBDT_H_
#define _LUBDT_H_

#include "bdt.h"

struct LUBDT {
    LUBDT() {}
    LUBDT(bdt_t l, bdt_t u) : lower(l), upper(u) {}
    LUBDT(const LUBDT& u) : lower(u.lower),upper(u.upper) {}
    ~LUBDT() {}

    LUBDT& operator=(const LUBDT& o) {
        lower = o.lower; upper = o.upper; return *this;
    }

    bdt_t lower;
    bdt_t upper;
};

#endif // _LUBDT_H_
