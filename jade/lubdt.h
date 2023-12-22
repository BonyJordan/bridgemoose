#ifndef _LUBDT_H_
#define _LUBDT_H_

#include "bdt.h"

struct LUBDT {
    LUBDT() {}
    LUBDT(BDT l, BDT u) : lower(l), upper(u) {}
    LUBDT(const LUBDT& u) : lower(u.lower),upper(u.upper) {}
    ~LUBDT() {}

    LUBDT& operator=(const LUBDT& o) {
        lower = o.lower; upper = o.upper; return *this;
    }

    BDT lower;
    BDT upper;
};

#endif // _LUBDT_H_
