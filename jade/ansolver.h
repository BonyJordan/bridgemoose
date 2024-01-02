#ifndef _ANSOLVER_H_
#define _ANSOLVER_H_

#include <map>
#include "cards.h"
#include "lubdt.h"
#include "solutil.h"

typedef std::map<hand64_t, LUBDT> TTMAP;

class ANSOLVER
{
  private:
    // problem definition
    const PROBLEM& _p;

    // helper info
    INTSET	_all_dids;
    BDT         _all_cube;
    TTMAP       _tt;

    bool doit_ew(STATE& state, const INTSET& dids);
    bool doit_ns(STATE& state, const INTSET& dids);

    std::vector<CARD> find_usable_plays_ns(const STATE& state,
	const INTSET& dids);

    void eval_1(const std::vector<CARD>& plays_so_far, STATE& state,
	INTSET& dids);
    bool eval_2(const STATE& state, const INTSET& dids);

  public:
    ANSOLVER(const PROBLEM& p);
    ~ANSOLVER();

    bool eval(STATE& state, const INTSET& dids);
    bool eval(const std::vector<CARD>& plays_so_far);

    const PROBLEM& problem() const { return _p; }
};

#endif // _ANSOLVER_H_
