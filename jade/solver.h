#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <map>
#include <vector>
#include <string>
#include "cards.h"
#include "lubdt.h"
#include "state.h"

typedef std::map<hand64_t, LUBDT> TTMAP;
typedef std::map<CARD, INTSET>    UPMAP;


struct PROBLEM
{
    hand64_t    north;
    hand64_t    south;
    int         trump;
    int         target;
    std::vector<hand64_t> wests;
    std::vector<hand64_t> easts;
};


class SOLVER
{
  private:
    // problem definition
    const PROBLEM& _p;

    // helper info
    INTSET	_all_dids;
    BDT         _all_cube;
    TTMAP       _tt;
  
    // Internal Functions
    LUBDT doit(STATE& state, const INTSET& dids, LUBDT search_bounds);
    LUBDT doit_ew(STATE& state, const INTSET& dids, LUBDT search_bounds, LUBDT node_bounds);
    LUBDT doit_ns(STATE& state, const INTSET& dids, LUBDT search_bounds, LUBDT node_bounds);
    void eval_1(const std::vector<CARD>& plays_so_far, STATE& state,
	INTSET& dids);
    void eval_2(STATE& state, INTSET& dids);

    UPMAP find_usable_plays_ew(const STATE& state, const INTSET& dids) const;
    UPMAP find_usable_plays_ns(const STATE& state, const INTSET& dids) const;
    CARD recommend_usable_play(const UPMAP& upmap) const;

  public:
    // public interface
    SOLVER(const PROBLEM& p);
    ~SOLVER();

    BDT eval(STATE& state, const INTSET& dids);
    BDT eval(const std::vector<CARD> plays_so_far);

    const PROBLEM& problem() const { return _p; }
    size_t count_ew() const { return _p.wests.size(); }
    hand64_t west(size_t i) const { return _p.wests[i]; }
    hand64_t east(size_t i) const { return _p.easts[i]; }
    hand64_t north() const { return _p.north; }
    hand64_t south() const { return _p.south; }
    int trump() const { return _p.trump; }
    int target() const { return _p.target; }
};

std::string bdt_to_string(BDT bdt);

#endif // _SOLVER_H_
