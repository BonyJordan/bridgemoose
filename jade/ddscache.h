#ifndef _DDSCACHE_H_
#define _DDSCACHE_H_

#include "problem.h"

struct DDS_KEY {
    int	     did;
    int      trick_card_bits;
    hand64_t key;

    bool operator==(const DDS_KEY& other) const {
	return compare(*this, other) == 0;
    }
    bool operator<(const DDS_KEY& other) const {
	return compare(*this, other) < 0;
    }
    static int compare(const DDS_KEY& a, const DDS_KEY& b) {
#define CMP(x)   if (a.x<b.x) return -1; else if (a.x>b.x) return 1;
	CMP(did);
	CMP(trick_card_bits);
	CMP(key);
	return 0;
#undef CMP
    }
    static DDS_KEY from_state(const STATE& state);
};


class DDS_CACHE {
    const PROBLEM& _problem;
    std::map<DDS_KEY, hand64_t>  _data;

  public:
    DDS_CACHE(const PROBLEM& problem);
    ~DDS_CACHE();

    std::map<int, hand64_t> solve_many(const STATE& state, const INTSET& dids);
};

#endif // _DDSCACHE_H_
