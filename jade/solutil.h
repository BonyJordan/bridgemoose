#ifndef _SOLUTIL_H_
#define _SOLUTIL_H_

#include <utility>
#include <map>
#include "bdt2.h"
#include "intset.h"
#include "problem.h"
#include "state.h"

typedef std::map<CARD, INTSET> UPMAP;

std::pair<STATE, INTSET> load_from_history(const PROBLEM& problem,
    const std::vector<CARD>& plays_so_far);
std::pair<STATE, INTSET> load_from_history(const PROBLEM& problem,
    const std::vector<CARD>& plays_so_far, const INTSET& dids);
UPMAP find_usable_plays_ew(const PROBLEM& problem, const STATE& state,
    const INTSET& dids);
bool is_target_achievable(const PROBLEM& problem, const STATE& state);
bool all_can_win(const PROBLEM& problem, const STATE& state,
    const INTSET& dids);

///////////

bdt_t set_to_atoms(BDT_MANAGER& b2, const INTSET& is);
bdt_t set_to_cube(BDT_MANAGER& b2, const INTSET& is);
bdt_t bdt_anti_cube(BDT_MANAGER& b2, const INTSET& big, const INTSET& small);

///////////

class DDS_LOADER
{
    const PROBLEM& _problem;
    const STATE&   _state;

    INTSET_ITR	_itr;
    int		_did_map[MAXNOOFBOARDS];

    struct boards       _bo;
    struct solvedBoards _solved;

    int _mode;
    int _solutions;

  private:
    void solve_some();
    void load_some();
    
  public:
    DDS_LOADER(const PROBLEM& problem, const STATE& state, const INTSET& dids,
	int mode, int solutions);
    ~DDS_LOADER();

    int chunk_size() const { return _solved.noOfBoards; }
    const struct futureTricks& chunk_solution(int i) const
	{ return _solved.solvedBoard[i]; }
    int chunk_did(int i) const { return _did_map[i]; }

    bool more() const {
	return _bo.noOfBoards > 0;
    }
    void next() {
	load_some();
    }
};

/////////////////

#endif // _SOLUTIL_H_
