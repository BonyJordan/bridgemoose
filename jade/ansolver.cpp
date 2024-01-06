#include "ansolver.h"
#include "jassert.h"


ANSOLVER::ANSOLVER(const PROBLEM& problem) :
    _p(problem),_dds_cache(problem)
{
    jassert(_p.wests.size() == _p.easts.size());
    _all_dids = INTSET::full_set((int)_p.wests.size());
    _all_cube = set_to_cube(_all_dids);
}


ANSOLVER::~ANSOLVER()
{
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far, const INTSET& dids)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far, dids);
    if (!all_can_win(sd.first, sd.second))
	return false;
    return eval(sd.first, sd.second);
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far);
    if (!all_can_win(sd.first, sd.second))
	return false;
    return eval(sd.first, sd.second);
}


bool ANSOLVER::all_can_win(const STATE& state, const INTSET& dids)
{
    DDS_LOADER loader(_p, state, dids, 1, 1);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
	    int score = loader.chunk_solution(i).score[0];
	    if (state.to_play_ns() && score <= 0)
		return false;
	    if (state.to_play_ew() && score > 0)
		return false;
	}
    }
    return true;
}


bool ANSOLVER::eval(STATE& state, const INTSET& dids)
{
    const bool debug = false;
    if (debug)
	printf("ANSOLVER::eval with [%s]\n", state.to_string().c_str());

    if (state.ns_tricks() >= _p.target)
	return true;
    else if (handbits_count(_p.north) - state.ew_tricks() < _p.target) {
	// this never happens.  we think.
	jassert(false);
    }
    bool new_trick = state.new_trick();
    hand64_t state_key = state.to_key();

    if (new_trick) {
	TTMAP::iterator f = _tt.find(state_key);
	if (f != _tt.end()) {
	    if (f->second.lower.contains(dids)) {
		if (debug)
		    printf("ANSOLVER::eval cache hit True\n"); 
		return true;
	    }
	    if (!f->second.upper.contains(dids)) {
		if (debug)
		    printf("ANSOLVER::eval cache hit False\n"); 
		return false;
	    }
	}
    }

    bool result;
    if (state.to_play_ew())
	result = doit_ew(state, dids);
    else
	result = doit_ns(state, dids);

    if (debug)
	printf("ANSOLVER::eval [%s] => %d\n", state.to_string().c_str(),
	    result);

    if (state.new_trick()) {
	TTMAP::iterator f = _tt.find(state_key);
	if (f == _tt.end()) {
	    _tt[state_key] = LUBDT(set_to_atoms(dids), _all_cube);
	}

	if (result)
	    _tt[state_key].lower |= set_to_cube(dids);
	else
	    _tt[state_key].upper &= bdt_anti_cube(_all_dids, dids);
    }
    return result;
}


bool ANSOLVER::doit_ew(STATE& state, const INTSET& dids)
{
    UPMAP plays = find_usable_plays_ew(_p, state, dids);
    UPMAP::const_iterator itr;

    for (itr = plays.begin() ; itr != plays.end() ; itr++)
    {
	CARD card = itr->first;
	const INTSET& sub_dids = itr->second;
	if (sub_dids.size() == 1)
	    continue;

	state.play(card);
	bool result = eval(state, sub_dids);
	state.undo();

	if (!result)
	    return false;
    }
    return true;
}


bool ANSOLVER::doit_ns(STATE& state, const INTSET& dids)
{
    std::vector<CARD> usable_plays = find_usable_plays_ns(state, dids);
    std::vector<CARD>::const_iterator itr;

    for (itr = usable_plays.begin() ; itr != usable_plays.end() ; itr++)
    {
	state.play(*itr);
	bool result = eval(state, dids);
	state.undo();

	if (result)
	    return true;
    }
    return false;
}


std::vector<CARD> ANSOLVER::find_usable_plays_ns(const STATE& state,
    const INTSET& dids)
{
    std::map<int, hand64_t> dds_wins = _dds_cache.solve_many(state, dids);
    std::map<int, hand64_t>::const_iterator itr;
    std::map<CARD, size_t> counts;

    hand64_t all = ALL_CARDS_BITS;
    for (itr = dds_wins.begin() ; itr != dds_wins.end() ; itr++)
	all &= itr->second;

    std::vector<CARD> out;
    while (all) {
	hand64_t bit = all & -all;
	out.push_back(handbit_to_card(bit));
	all &= ~bit;
    }
    return out;
}
