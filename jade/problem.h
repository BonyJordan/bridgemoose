#ifndef _PROBLEM_H_
#define _PROBLEM_H_

#include <stdio.h>
#include <vector>
#include <string>
#include "cards.h"


struct PROBLEM
{
    hand64_t    north;
    hand64_t    south;
    int         trump;
    int         target;
    std::vector<hand64_t> wests;
    std::vector<hand64_t> easts;
    
    std::string read_from_filestream(FILE* fp);
    std::string write_to_filestream(FILE* fp);
};


#endif // _PROBLEM_H_
