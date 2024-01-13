#ifndef _ANSOLVER_H_
#define _ANSOLVER_H_

#include <map>
#include "cards.h"
#include "lubdt.h"
#include "solutil.h"
#include "ddscache.h"

typedef std::map<hand64_t, LUBDT> TTMAP;
typedef unsigned long stat_t;

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
    const PROBLEM& _p;

    // helper info
    BDT_MANAGER _b2;
    DDS_CACHE	_dds_cache;
    INTSET	_all_dids;
    bdt_t       _all_cube;
    TTMAP       _tt;

    // stats
#define A(x)	stat_t _ ## x;
ANSOLVER_STATS(A)
#undef A

    bool doit_ew(STATE& state, const INTSET& dids);
    bool doit_ns(STATE& state, const INTSET& dids);

    std::vector<CARD> find_usable_plays_ns(const STATE& state,
	const INTSET& dids);

  public:
    ANSOLVER(const PROBLEM& p);
    ~ANSOLVER();

    bool eval(STATE& state, const INTSET& dids);
    bool eval(const std::vector<CARD>& plays_so_far);
    bool eval(const std::vector<CARD>& plays_so_far, const INTSET& dids);

    const PROBLEM& problem() const { return _p; }
    std::map<std::string, stat_t> get_stats() const;

    bool write_to_files(const char* bdt_file, const char* tt_file);
    bool read_from_files(const char* bdt_file, const char* tt_file);
};

#endif // _ANSOLVER_H_
