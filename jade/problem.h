#ifndef _PROBLEM_H_
#define _PROBLEM_H_

#include <vector>
#include "cards.h"


struct PROBLEM
{
    hand64_t    north;
    hand64_t    south;
    int         trump;
    int         target;
    std::vector<hand64_t> wests;
    std::vector<hand64_t> easts;
};


#endif // _PROBLEM_H_
