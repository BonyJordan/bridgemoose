#ifndef _ANSOLVER_H_
#define _ANSOLVER_H_

#include <map>
#include "cards.h"
#include "lubdt.h"
#include "solutil.h"
#include "ddscache.h"

typedef std::map<hand64_t, LUBDT> TTMAP;

class ANSOLVER
{
  private:
    // problem definition
    const PROBLEM& _p;

    // helper info
    DDS_CACHE	_dds_cache;
    INTSET	_all_dids;
    BDT         _all_cube;
    TTMAP       _tt;

    bool doit_ew(STATE& state, const INTSET& dids);
    bool doit_ns(STATE& state, const INTSET& dids);

    std::vector<CARD> find_usable_plays_ns(const STATE& state,
	const INTSET& dids);

    bool all_can_win(const STATE& state, const INTSET& dids);

  public:
    ANSOLVER(const PROBLEM& p);
    ~ANSOLVER();

    bool eval(STATE& state, const INTSET& dids);
    bool eval(const std::vector<CARD>& plays_so_far);
    bool eval(const std::vector<CARD>& plays_so_far, const INTSET& dids);

    const PROBLEM& problem() const { return _p; }
};

#endif // _ANSOLVER_H_
