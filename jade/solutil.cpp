#include "solutil.h"
#include "jassert.h"

std::pair<STATE, INTSET> load_from_history(const PROBLEM& problem,
    const std::vector<CARD>& plays_so_far)
{
    INTSET dids = INTSET::full_set((int)problem.wests.size());
    return load_from_history(problem, plays_so_far, dids);
}


std::pair<STATE, INTSET> load_from_history(const PROBLEM& problem,
    const std::vector<CARD>& plays_so_far, const INTSET& dids_in)
{
    STATE state(problem.trump);
    INTSET dids(dids_in);

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
		    if ((problem.easts[itr.current()] & bit) != 0)
			found = true;
		} else {
		    if ((problem.wests[itr.current()] & bit) != 0)
			found = true;
		}
		if (found)
		    good_dids.insert(itr.current());
		else {
		    /*printf("WARN: dropping (%d) %s::%s as inconsistent\n",
			itr.current(),
			hand_to_string(problem.wests[itr.current()]).c_str(),
			hand_to_string(problem.easts[itr.current()]).c_str());
		    */
		}
	    }
	    dids = good_dids;
	} else if (state.to_play() == J_NORTH) {
	    if ((problem.north & bit) == 0)
		dids.remove_all();
	} else if (state.to_play() == J_SOUTH) {
	    if ((problem.south & bit) == 0)
		dids.remove_all();
	} else {
	    // Impossible!
	    jassert(false);
	}

	state.play(card);
    }

    return std::pair<STATE,INTSET>(state, dids);
}


UPMAP find_usable_plays_ew(const PROBLEM& problem, const STATE& state,
    const INTSET& dids)
{
    UPMAP plays;
    hand64_t follow_bits = 0;
    if (!state.new_trick()) {
        follow_bits = suit_bits(state.trick_card(0).suit);
    }
    bool is_east = (state.to_play() == J_EAST);

    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next())
    {
        hand64_t hand = is_east ?
	    problem.easts[itr.current()] :
	    problem.wests[itr.current()];
        hand &= ~state.played();

        if ((hand & follow_bits) != 0)
            hand &= follow_bits;

        for (HAND_ITR hitr(hand) ; hitr.more() ; hitr.next()) {
            plays[hitr.current()].insert(itr.current());
        }
    }
    return plays;
}


///////////////////////////////////////

BDT set_to_atoms(const INTSET& is)
{
    BDT out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
        out |= BDT::atom(itr.current());
    return out;
}


BDT set_to_cube(const INTSET& is)
{
    BDT out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
        out = out.extrude(itr.current());
    return out;
}

BDT bdt_anti_cube(const INTSET& big, const INTSET& small)
{
    BDT perfect, flawed;
    bool any_flaws = false;
    for (INTSET_PAIR_ITR itr(big,small) ; itr.more() ; itr.next())
    {
	if (itr.a_only()) {
	    perfect = perfect.extrude(itr.current());
	    flawed = flawed.extrude(itr.current());
	} else if (itr.both()) {
	    if (any_flaws) {
		flawed = perfect | flawed.extrude(itr.current());
	    } else {
		any_flaws = true;
		flawed = perfect;
	    }
	    perfect = perfect.extrude(itr.current());
	}
    }
    return flawed;
}

///////////////////////////////////////

DDS_LOADER::DDS_LOADER(const PROBLEM& problem, const STATE &state,
    const INTSET& dids, int mode, int solutions)
:
    _problem(problem),_state(state),_itr(dids),
    _mode(mode),_solutions(solutions)
{
    load_some();
}

DDS_LOADER::~DDS_LOADER()
{
}


void DDS_LOADER::solve_some()
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
    jassert(_solved.noOfBoards == _bo.noOfBoards);
}


void DDS_LOADER::load_some()
{
    int k = 0;
    int target;

    if (_state.to_play_ns()) {
	target = _problem.target - _state.ns_tricks();
    } else {
	target = handbits_count(_problem.north) - _problem.target +
	    1 - _state.ew_tricks();
    }

    while (true)
    {
	if (!_itr.more() || k == MAXNOOFBOARDS) {
	    _bo.noOfBoards = k;
	    if (k > 0)
		jassert(_bo.deals[k-1].first >= 0 &&
		    _bo.deals[k-1].first <= 3);
	    solve_some();
	    return;
	}            

	_bo.deals[k].trump = _problem.trump;
	for (int j=0 ; j<3 ; j++) {
	    CARD tc = _state.trick_card(j);
	    _bo.deals[k].currentTrickSuit[j] = tc.suit;
	    _bo.deals[k].currentTrickRank[j] = tc.rank;
	}
	int did = _itr.current();
	_did_map[k] = did;
	set_deal_cards(_problem.north & ~_state.played(), J_NORTH,
	    _bo.deals[k]);
	set_deal_cards(_problem.south & ~_state.played(), J_SOUTH,
	    _bo.deals[k]);
	set_deal_cards(_problem.wests[did] & ~_state.played(), J_WEST,
	    _bo.deals[k]);
	set_deal_cards(_problem.easts[did] & ~_state.played(), J_EAST,
	    _bo.deals[k]);

	_bo.deals[k].first = _state.trick_leader();
	jassert(_bo.deals[k].first >= 0 && _bo.deals[k].first <= 3);
	_bo.mode[k] = _mode;
	_bo.solutions[k] = _solutions;
	_bo.target[k] = target;

	k++;
	_itr.next();
    }
}
