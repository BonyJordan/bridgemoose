#include "ansolver.h"
#include "jassert.h"


ANSOLVER::ANSOLVER(const PROBLEM& problem) :
    _p(problem),_dds_cache(problem)
{
    jassert(_p.wests.size() == _p.easts.size());
    _all_dids = INTSET::full_set((int)_p.wests.size());
    _all_cube = set_to_cube2(_b2, _all_dids);
#define A(x)	_ ## x = 0;
    ANSOLVER_STATS(A)
#undef A
}


ANSOLVER::~ANSOLVER()
{
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far, const INTSET& dids)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far, dids);
    if (!is_target_achievable(_p, sd.first))
	return false;
    _dds_calls++;
    if (!all_can_win(_p, sd.first, sd.second))
	return false;
    return eval(sd.first, sd.second);
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far);
    if (!is_target_achievable(_p, sd.first))
	return false;
    _dds_calls++;
    if (!all_can_win(_p, sd.first, sd.second))
	return false;
    return eval(sd.first, sd.second);
}


bool ANSOLVER::eval(STATE& state, const INTSET& dids)
{
    _node_visits++;

    const bool debug = false;
    if (debug)
	printf("ANSOLVER::eval with [%s]\n", state.to_string().c_str());

    if (state.ns_tricks() >= _p.target)
	return true;
    else if (handbits_count(_p.north) - state.ew_tricks() < _p.target) {
	// this never happens.  we think.
	printf("ANSOLVER: I thought this never happened!\n");
	printf("handbits=(%d)  ew_tx=%d  p.target=%d\n",
	    handbits_count(_p.north), state.ew_tricks(), _p.target);
	printf("state=[%s]\n", state.to_string().c_str());
	jassert(false);
    }
    bool new_trick = state.new_trick();
    hand64_t state_key = state.to_key();

    if (new_trick) {
	TTMAP2::iterator f = _tt.find(state_key);
	if (f != _tt.end()) {
	    _cache_hits++;
	    if (_b2.contains(f->second.lower, dids)) {
		if (debug)
		    printf("ANSOLVER::eval cache hit True\n"); 
		_cache_cutoffs++;
		return true;
	    }
	    if (!_b2.contains(f->second.upper, dids)) {
		if (debug)
		    printf("ANSOLVER::eval cache hit False\n"); 
		_cache_cutoffs++;
		return false;
	    }
	} else {
	    _cache_misses++;
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
	TTMAP2::iterator f = _tt.find(state_key);
	if (f == _tt.end()) {
	    _tt[state_key] = LUBDT2(set_to_atoms2(_b2, dids), _all_cube);
	    _cache_size++;
	}

	if (result)
	    _tt[state_key].lower = _b2.unionize(
		_tt[state_key].lower,
		set_to_cube2(_b2, dids));
	else
	    _tt[state_key].upper = _b2.intersect(
		_tt[state_key].upper,
		bdt_anti_cube2(_b2, _all_dids, dids));
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
    _dds_calls++;

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


std::map<std::string, stat_t> ANSOLVER::get_stats() const
{
    std::map<std::string, stat_t> out;
#define A(x)	out[# x] = _ ## x;
    ANSOLVER_STATS(A)
#undef A

    return out;
}
