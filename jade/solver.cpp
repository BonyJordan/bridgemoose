#include "solver.h"

//////////////

class DDS_LOADER
{
    const SOLVER& _solver;
    const STATE&  _state;
    const INTSET& _dids;

    INTSET_ITR    _itr;
    int           _did_map[MAXNOOFBOARDS];

    struct boards       _bo;
    struct solvedBoards _solved;

    int _mode;
    int _solutions;

  private:
void solve_some()
{
    if (_bo.noOfBoards == 0) {
	_solved.noOfBoards = 0;
	return;
    }

    int r = SolveAllBoardsBin(&_bo, &_solved);
    if (r < 0) {
	char line[80];
	ErrorMessage(r, line);
	fprintf(stderr, "DDS_LOADER::solve_some(): DDS(%d): %s\n", r, line);
	exit(-1);
    }
    assert(_solved.noOfBoards == _bo.noOfBoards);
}


void load_some()
{
    int k = 0;
    int target;

    if (_state.to_play_ns()) {
	target = _solver.target() - _state.ns_tricks();
    } else {
	target = handbits_count(_solver.north()) - _solver.target() + 1 - _state.ew_tricks();
    }

    while (true)
    {
	if (!_itr.more() || k == MAXNOOFBOARDS) {
	    _bo.noOfBoards = k;
	    solve_some();
	    return;
	}

	_bo.deals[k].trump = _solver.trump();
	for (int j=0 ; j<3 ; j++) {
	    CARD tc = _state.trick_card(j);
	    _bo.deals[k].currentTrickSuit[j] = tc.suit;
	    _bo.deals[k].currentTrickRank[j] = tc.rank;
	}
	int did = _itr.current();
	_did_map[k] = did;
	set_deal_cards(_solver.north() & ~_state.played(), J_NORTH,
	    _bo.deals[k]);
	set_deal_cards(_solver.south() & ~_state.played(), J_SOUTH,
	    _bo.deals[k]);
	set_deal_cards(_solver.west(did) & ~_state.played(), J_WEST,
	    _bo.deals[k]);
	set_deal_cards(_solver.east(did) & ~_state.played(), J_EAST,
	    _bo.deals[k]);

	_bo.deals[k].first = _state.trick_leader();
	_bo.mode[k] = _mode;
	_bo.solutions[k] = _solutions;
	_bo.target[k] = target;

	k++;
	_itr.next();
    }
}

  public:
    DDS_LOADER(const SOLVER& solver, const STATE& state, const INTSET& dids,
	int mode, int solutions) :
    _solver(solver),_state(state),_dids(dids),_itr(dids),_mode(mode),
    _solutions(solutions)
    {
	load_some();
    }
    ~DDS_LOADER()
    {
    }

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

//////////////////////

static BDT set_to_atoms(const INTSET& is)
{
    BDT out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
        out |= BDT::atom(itr.current());
    return out;
}


static BDT set_to_cube(const INTSET& is)
{
    BDT out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
        out = out.extrude(itr.current());
    return out;
}


SOLVER::SOLVER(hand64_t north, hand64_t south, int trump, int target,
    const std::vector<hand64_t>& wests, const std::vector<hand64_t>& easts)
    :
_north(north),
_south(south),
_trump(trump),
_target(target),
_wests(wests),
_easts(easts)
{
    assert(_wests.size() == _easts.size());
    _all_dids = INTSET::full_set((int)_wests.size());
}


SOLVER::~SOLVER()
{
}


BDT SOLVER::eval(const std::vector<CARD> plays_so_far)
{
    INTSET dids = _all_dids;
    STATE state(_trump);

    eval_1(plays_so_far, state, dids);
    eval_2(state, dids);
    return eval(state, dids);
}

void SOLVER::eval_1(const std::vector<CARD>& plays_so_far, STATE& state,
    INTSET& dids)
{
    // Step 1:
    //   update the STATE to match the cards played so far
    //   filter out deal-ids (dids) which are not consistent with the plays
    //   made.

    for (unsigned i=0 ; i<plays_so_far.size() ; i++)
    {
	CARD card = plays_so_far[i];
	hand64_t bit = card_to_handbit(card);

	if (state.to_play_ew()) {
	    // filter out illegal dids
	    INTSET good_dids;
	    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next())
	    {
		bool found = false;
		if (state.to_play() == J_EAST) {
		    if ((_easts[itr.current()] & bit) != 0)
			found = true;
		} else {
		    if ((_wests[itr.current()] & bit) != 0)
			found = true;
		}
		if (found)
		    good_dids.insert(itr.current());
	    }
	    dids = good_dids;
	} else if (state.to_play() == J_NORTH) {
	    if ((_north & bit) == 0)
		dids.remove_all();
	} else if (state.to_play() == J_SOUTH) {
	    if ((_south & bit) == 0)
		dids.remove_all();
	} else {
	    // Impossible!
	    assert(false);
	}

	state.play(card);
    }
}


void SOLVER::eval_2(STATE& state, INTSET& dids)
{
    // The inner workings of SOLVER have the invariant that every
    // deal in <dids> has at least one double-dummy solution for the
    // target number of tricks... so we need to filter out those which
    // don't.

    // mode 1: find the actual score
    // solutions 1: find any card which succeeds

    DDS_LOADER loader(*this, state, dids, 1, 1);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
	    int score = loader.chunk_solution(i).score[0];
	    if (score <= 0) {
		dids.remove(loader.chunk_did(i));
	    }
	}
    }
}


BDT SOLVER::eval(STATE& state, const INTSET& dids)
{
    LUBDT search_bounds(set_to_atoms(dids), set_to_cube(dids));
    LUBDT result = doit(state, dids, search_bounds);
    return (result.lower | search_bounds.lower) & search_bounds.upper;
}


LUBDT SOLVER::doit(STATE& state, const INTSET& dids, LUBDT search_bounds)
{
    if (state.ns_tricks() >= _target) {
	BDT cube = set_to_cube(dids);
	return LUBDT(cube, cube);
    } else if (handbits_count(_north) - state.ew_tricks() < _target) {
	// I think this never happens
	assert(false);
    }

    return search_bounds;
}
