#ifndef _STHASH_H_
#define _STHASH_H_

#include "problem.h"
#include "state.h"

class STATE_HASHER
{
  private:
    enum { TBL_SIZE = 1<<13 };
    const PROBLEM&	_problem;
    uint16_t	_tbl[4][TBL_SIZE];

    void precompute_table();
    uint16_t compute_one(int suit, uint16_t played) const;

  public:
    STATE_HASHER(const PROBLEM& problem);
    ~STATE_HASHER();

    uint64_t hash(const STATE& state) const;
};

#endif // _STHASH_H_
