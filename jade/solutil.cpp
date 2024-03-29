#include "solutil.h"
#include "jassert.h"

DDS_C_API* dds_api = NULL;


bool won_already(const PROBLEM& problem, const STATE& state)
{
    return (state.ns_tricks() >= problem.target);
}

bool lost_already(const PROBLEM& problem, const STATE& state)
{
    return (state.ew_tricks() >= 14 - problem.target);
}

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
	bool check_suit = false;
	if (!state.new_trick() && state.suit_led() != card.suit)
	    check_suit = true;

	if (state.to_play_ew()) {
	    // filter out illegal dids
	    INTSET good_dids;
	    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next())
	    {
		hand64_t hand =
		    state.to_play() == J_EAST ?
		    problem.easts[itr.current()] :
		    problem.wests[itr.current()];

		bool found = true;
		if ((hand & bit) == 0)
		    found = false;
		if (found && check_suit) {
		    hand64_t in_suit =
			hand &
			suit_bits(state.suit_led()) &
			~state.played();
		    if (in_suit != 0)
			found = false;
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

bool is_target_achievable(const PROBLEM& problem, const STATE& state)
{
    return handbits_count(problem.north) - state.ew_tricks() >= problem.target;
}

bool all_can_win(const PROBLEM& problem, const STATE& state,
    const INTSET& dids)
{
    const bool debug = false;
    if (won_already(problem, state))
	return true;
    if (lost_already(problem, state))
	return false;

    DDS_LOADER loader(problem, state, dids, 1, 1);

    for ( ; loader.more() ; loader.next())
    {
        for (int i=0 ; i<loader.chunk_size() ; i++) {
            int score = loader.chunk_solution(i).score[0];
            if (state.to_play_ns() && score <= 0) {
		if (debug) {
		    fprintf(stderr, "all_can_win: ns play chunk=%d did=%d score=%d returning false\n", i, loader.chunk_did(i), score);
		}
                return false;
	    }
            if (state.to_play_ew() && score > 0) {
		if (debug) {
		    fprintf(stderr, "all_can_win: ew play chunk=%d did=%d score=%d returning false\n", i, loader.chunk_did(i), score);
		}
                return false;
	    }
        }
    }
    return true;
}
 
 
///////////////////////////////////////

bdt_t set_to_atoms(BDT_MANAGER& b2, const INTSET& is)
{
    bdt_t out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
        out = b2.unionize(out, b2.atom(itr.current()));
    return out;
}


bdt_t set_to_cube(BDT_MANAGER& b2, const INTSET& is)
{
    bdt_t out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next()) {
        out = b2.extrude(out, itr.current());
    }
    return out;
}

bdt_t bdt_anti_cube(BDT_MANAGER& b2, const INTSET& big, const INTSET& small)
{
    bdt_t perfect, flawed;
    bool any_flaws = false;
    for (INTSET_PAIR_ITR itr(big,small) ; itr.more() ; itr.next())
    {
	if (itr.a_only()) {
	    perfect = b2.extrude(perfect, itr.current());
	    flawed = b2.extrude(flawed, itr.current());
	} else if (itr.both()) {
	    if (any_flaws) {
		flawed = b2.unionize(
		    perfect,
		    b2.extrude(flawed, itr.current()));
	    } else {
		any_flaws = true;
		flawed = perfect;
	    }
	    perfect = b2.extrude(perfect, itr.current());
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

    int r = (*dds_api->pSolveAllBoardsBin)(&_bo, &_solved);

    if (r < 0) {
	char line[80];
	(*dds_api->pErrorMessage)(r, line);

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

/////////////////

std::string bdt_to_string(BDT_MANAGER& b2, bdt_t bdt)
{
    bool first = true;
    std::vector<INTSET> cubes = b2.get_cubes(bdt);
    std::vector<INTSET>::const_iterator itr;
    std::string out = "{";

    for (itr=cubes.begin() ; itr != cubes.end() ; itr++) {
        if (first)
            first = false;
        else
            out += '/';
        out += intset_to_string(*itr);
    }
    return out + '}';
}
