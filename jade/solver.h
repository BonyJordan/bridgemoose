#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <map>
#include <vector>
#include <string>
#include "cards.h"
#include "lubdt.h"
#include "state.h"
#include "problem.h"
#include "solutil.h"
#include "ddscache.h"

#include <set>

typedef std::map<hand64_t, LUBDT> TTMAP;


typedef unsigned long stat_t;

#define SOLVER_STATS(A) \
	A(cache_hits)	\
	A(cache_misses)	\
	A(cache_size)	\
	A(dds_calls)	\
	A(dds_boards)	\
	A(dds_repeats)	\
	A(node_visits)


class SOLVER
{
  private:
    // problem definition
    const PROBLEM& _p;

    // helper info
    INTSET	_all_dids;
    BDT         _all_cube;
    TTMAP       _tt;

    // stats
#define A(x)	stat_t _ ## x;
SOLVER_STATS(A)
#undef A
    std::set<DDS_KEY>	_dds_tracker;
  
    // Internal Functions
    LUBDT doit(STATE& state, const INTSET& dids, LUBDT search_bounds);
    LUBDT doit_ew(STATE& state, const INTSET& dids, LUBDT search_bounds, LUBDT node_bounds);
    LUBDT doit_ns(STATE& state, const INTSET& dids, LUBDT search_bounds, LUBDT node_bounds);
    void eval_2(STATE& state, INTSET& dids);

    UPMAP find_usable_plays_ns(const STATE& state, const INTSET& dids);
    CARD recommend_usable_play(const UPMAP& upmap) const;

    void track_dds(const STATE& state, const INTSET& dids);

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

    std::map<std::string, stat_t> get_stats() const;
};

std::string bdt_to_string(BDT bdt);

#endif // _SOLVER_H_
