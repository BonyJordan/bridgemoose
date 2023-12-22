#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <map>
#include <vector>
#include "cards.h"
#include "lubdt.h"
#include "state.h"

typedef std::map<hand64_t, LUBDT> TTMAP;

class SOLVER
{
  private:
    // problem definition
    hand64_t	_north;
    hand64_t	_south;
    int         _trump;
    int         _target;
    std::vector<hand64_t> _wests;
    std::vector<hand64_t> _easts;

    // helper info
    INTSET	_all_dids;
    TTMAP       _tt;
  
    // Internal Functions
    LUBDT doit(STATE& state, const INTSET& dids, LUBDT search_bounds);
    void eval_1(const std::vector<CARD>& plays_so_far, STATE& state,
	INTSET& dids);
    void eval_2(STATE& state, INTSET& dids);

  public:
    // public interface
    SOLVER(hand64_t north, hand64_t south, int trump, int target,
	const std::vector<hand64_t>& wests, const std::vector<hand64_t>& easts);
    ~SOLVER();

    BDT eval(STATE& state, const INTSET& dids);
    BDT eval(const std::vector<CARD> plays_so_far);

    size_t count_ew() const { return _wests.size(); }
    hand64_t west(size_t i) const { return _wests[i]; }
    hand64_t east(size_t i) const { return _easts[i]; }
    hand64_t north() const { return _north; }
    hand64_t south() const { return _south; }
    int trump() const { return _trump; }
    int target() const { return _target; }
};

#endif // _SOLVER_H_
