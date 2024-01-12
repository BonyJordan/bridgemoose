#ifndef _LUBDT_H_
#define _LUBDT_H_

#include "bdt.h"
#include "bdt2.h"

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

struct LUBDT2 {
    LUBDT2() {}
    LUBDT2(bdt2_t l, bdt2_t u) : lower(l), upper(u) {}
    LUBDT2(const LUBDT2& u) : lower(u.lower),upper(u.upper) {}
    ~LUBDT2() {}

    LUBDT2& operator=(const LUBDT2& o) {
        lower = o.lower; upper = o.upper; return *this;
    }

    bdt2_t lower;
    bdt2_t upper;
};

#endif // _LUBDT_H_
