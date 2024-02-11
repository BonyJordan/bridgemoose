#ifndef _ANSOLVER_H_
#define _ANSOLVER_H_

#include <map>
#include <string>
#include "cards.h"
#include "solutil.h"
#include "ddscache.h"
#include "jadeio.h"
#include "sthash.h"
#include "soltypes.h"

#define ANSOLVER_STATS(A) \
        A(cache_cutoffs) \
        A(cache_hits)    \
        A(cache_misses)  \
        A(cache_size)    \
        A(dds_calls)     \
        A(node_visits)

class ANSOLVER
{
  private:
    // problem definition
    PROBLEM       _p;

    // helper info
    STATE_HASHER _hasher;
    BDT_MANAGER  _b2;
    DDS_CACHE	 _dds_cache;
    INTSET	 _all_dids;
    bdt_t        _all_cube;
    TTMAP        _tt;

    // stats
#define A(x)	stat_t _ ## x;
ANSOLVER_STATS(A)
#undef A

    bool doit_ew(STATE& state, const INTSET& dids);
    bool doit_ns(STATE& state, const INTSET& dids);

    std::vector<CARD> find_usable_plays_ns(const STATE& state,
	const INTSET& dids);
    void fill_tt_inner(std::map<hand64_t, bdt_t>& visited, STATE& state,
	const INTSET& dids);

  public:
    ANSOLVER(const PROBLEM& p);
    ~ANSOLVER();

    bool eval(STATE& state, const INTSET& dids);
    bool eval(const std::vector<CARD>& plays_so_far);
    bool eval(const std::vector<CARD>& plays_so_far, const INTSET& dids);

    const PROBLEM& problem() const { return _p; }
    std::map<std::string, stat_t> get_stats() const;

    // both return "" in case of no error, otherwise a message
    std::string write_to_file(const char* filename);
    static RESULT<ANSOLVER> read_from_file(const char* filename);

    void fill_tt(const std::vector<CARD>& plays_so_far);

    void compare_tt(const ANSOLVER& b) const;
};

#endif // _ANSOLVER_H_
