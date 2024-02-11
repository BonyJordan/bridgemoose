#include <stdio.h>
#include "ansolver.h"
#include "jassert.h"
#include "jadeio.h"


ANSOLVER::ANSOLVER(const PROBLEM& problem) :
    _p(problem),_hasher(_p),_dds_cache(_p)
{
    jassert(_p.wests.size() == _p.easts.size());
    _all_dids = INTSET::full_set((int)_p.wests.size());
    _all_cube = _b2.null();
#define A(x)	_ ## x = 0;
    ANSOLVER_STATS(A)
#undef A
}


ANSOLVER::~ANSOLVER()
{
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far, const INTSET& dids)
{
    const bool debug = false;
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far, dids);
    if (!is_target_achievable(_p, sd.first)) {
	if (debug)
	    fprintf(stderr, "ANSOLVER::eval -> target not achievable\n");
	return false;
    }
    _dds_calls++;
    if (!all_can_win(_p, sd.first, sd.second)) {
	if (debug)
	    fprintf(stderr, "ANSOLVER::eval -> not all can win\n");
	return false;
    }
    return eval(sd.first, sd.second);
}


bool ANSOLVER::eval(const std::vector<CARD>& plays_so_far)
{
    const bool debug = false;
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far);
    if (!is_target_achievable(_p, sd.first)) {
	if (debug)
	    fprintf(stderr, "ANSOLVER::eval -> target not achievable\n");
	return false;
    }
    _dds_calls++;
    if (!all_can_win(_p, sd.first, sd.second)) {
	if (debug)
	    fprintf(stderr, "ANSOLVER::eval -> not all can win\n");
	return false;
    }
    if (debug) {
	fprintf(stderr, "ANSOLVER::eval -> got a state and dids, let's go!\n");
    }
    return eval(sd.first, sd.second);
}


bool ANSOLVER::eval(STATE& state, const INTSET& dids)
{
    _node_visits++;

    const bool debug = false;

    if (debug)
	fprintf(stderr, "ANSOLVER::eval with [%s] aka %016llx\n", state.to_string().c_str(), state.played());

    if (state.ns_tricks() >= _p.target)
	return true;
    else if (handbits_count(_p.north) - state.ew_tricks() < _p.target) {
	// this never happens.  we think.
	fprintf(stderr, "ANSOLVER: I thought this never happened!\n");
	fprintf(stderr, "handbits=(%d)  ew_tx=%d  p.target=%d\n",
	    handbits_count(_p.north), state.ew_tricks(), _p.target);
	fprintf(stderr, "state=[%s]\n", state.to_string().c_str());
	jassert(false);
    }
    bool new_trick = state.new_trick();
    hand64_t state_key = _hasher.hash(state);

    if (new_trick) {
	TTMAP::iterator f = _tt.find(state_key);
	if (f != _tt.end()) {
	    _cache_hits++;
	    if (debug) {
		fprintf(stderr, "ANSOLVER: cache lookup <%s>\n",
		    _hasher.hash_to_string(state_key).c_str());
		fprintf(stderr, "ANSOLVER: raw key: %016llx\n", state_key);
		fprintf(stderr, "dids=%s low=%s  up=%s\n",
		    intset_to_string(dids).c_str(),
		    bdt_to_string(_b2, f->second.lower).c_str(),
		    bdt_to_string(_b2, f->second.upper).c_str());
	    }

	    if (_b2.contains(f->second.lower, dids)) {
		if (debug)
		    fprintf(stderr, "ANSOLVER::eval cache hit True\n"); 
		_cache_cutoffs++;
		return true;
	    }
	    if (!_b2.contains(f->second.upper, dids)) {
		if (debug)
		    fprintf(stderr, "ANSOLVER::eval cache hit False\n"); 
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
	fprintf(stderr, "ANSOLVER::eval [%s] => %d\n",
	    state.to_string().c_str(), result);

    if (state.new_trick()) {
	TTMAP::iterator f = _tt.find(state_key);
	if (f == _tt.end()) {
	    if (_all_cube.is_null())
		_all_cube = set_to_cube(_b2, _all_dids);
	    _tt[state_key] = LUBDT(set_to_atoms(_b2, dids), _all_cube);
	    _cache_size++;
	}

	if (result) {
	    bdt_t x1 = _tt[state_key].lower;
	    bdt_t x2 = set_to_cube(_b2, dids);

	    _tt[state_key].lower = _b2.unionize(
		_tt[state_key].lower,
		set_to_cube(_b2, dids));

	    if (!_b2.contains(_tt[state_key].lower, dids)) {
		fprintf(stderr, "Here we are, unhappy as a pig, dids=%s\n",
		    intset_to_string(dids).c_str());
		fprintf(stderr, "x1 = %s\n", bdt_to_string(_b2, x1).c_str());
		fprintf(stderr, "x2 = %s\n", bdt_to_string(_b2, x2).c_str());
		fprintf(stderr, "now = %s\n", bdt_to_string(_b2, _tt[state_key].lower).c_str());
	    }
	    //jassert(_b2.contains(_tt[state_key].lower, dids));
	} else {
	    _tt[state_key].upper = _b2.intersect(
		_tt[state_key].upper,
		bdt_anti_cube(_b2, _all_dids, dids));

	    if (_b2.contains(_tt[state_key].upper, dids)) {
		fprintf(stderr, "Here we are, unhappy as a DOG, dids=%s\n",
		    intset_to_string(dids).c_str());
	    }
	    //jassert(!_b2.contains(_tt[state_key].upper, dids));
	}
    }
    return result;
}


bool ANSOLVER::doit_ew(STATE& state, const INTSET& dids)
{
    const bool debug = false;

    UPMAP plays = find_usable_plays_ew(_p, state, dids);
    UPMAP::const_iterator itr;

    for (itr = plays.begin() ; itr != plays.end() ; itr++)
    {
	CARD card = itr->first;
	const INTSET& sub_dids = itr->second;
	if (sub_dids.size() == 1) {
	    if (debug)
		fprintf(stderr, "ANSOLVER::doit_ew; card=%s, dids=1\n",
		    card_to_string(card).c_str());
	    continue;
	}

	state.play(card);
	bool result = eval(state, sub_dids);
	state.undo();

	if (debug)
	    fprintf(stderr, "ANSOLVER::doit_ew; card=%s, result=%d\n",
		card_to_string(card).c_str(), result);

	if (!result)
	    return false;
    }
    return true;
}


bool ANSOLVER::doit_ns(STATE& state, const INTSET& dids)
{
    const bool debug = false;

    std::vector<CARD> usable_plays = find_usable_plays_ns(state, dids);
    std::vector<CARD>::const_iterator itr;

    if (debug)
	fprintf(stderr, "ANSOLVER::doit_ns -> #usable_plays = %zu\n",
	    usable_plays.size());

    for (itr = usable_plays.begin() ; itr != usable_plays.end() ; itr++)
    {
	state.play(*itr);
	bool result = eval(state, dids);
	state.undo();

	if (debug)
	    fprintf(stderr, "ANSOLVER::doit_ns -> tried %s got %d\n",
		card_to_string(*itr).c_str(), result);

	if (result)
	    return true;
    }
    return false;
}


std::vector<CARD> ANSOLVER::find_usable_plays_ns(const STATE& state,
    const INTSET& dids)
{
    const bool debug = false;
    _dds_calls++;

    std::map<int, hand64_t> dds_wins = _dds_cache.solve_many(state, dids);
    std::map<int, hand64_t>::const_iterator itr;
    std::map<CARD, size_t> counts;

    hand64_t all = ALL_CARDS_BITS;
    for (itr = dds_wins.begin() ; itr != dds_wins.end() ; itr++) {
	if (debug) {
	    fprintf(stderr, "ANSOLVER::find_usable_plays_ns; did=%d cards=%s\n",
		itr->first, hand_to_string(itr->second).c_str());
	}
	all &= itr->second;
    }

    if (debug) {
	fprintf(stderr, "ANSOLVER::find_usable_plays_ns; yay=%s\n",
	    hand_to_string(all).c_str());
    }

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
    out["tt_size"] = (stat_t)_tt.size();

    return out;
}

////////////////////////

static const uint32_t FILE_HEADER = 0xf136898;

std::string ANSOLVER::write_to_file(const char* filename)
{
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
	return oserr_str(filename);
    }

    std::string fn_colon = std::string(filename) + ": ";
    std::string err;
    if ((err = write_thing(FILE_HEADER, fp)) != "") {
	fclose(fp);
	return fn_colon + err;
    }
    if ((err = _p.write_to_filestream(fp)) != "") {
	fclose(fp);
	return fn_colon + err;
    }
    if ((err = _b2.write_to_filestream(fp)) != "") {
	fclose(fp);
	return fn_colon + err;
    }

    uint32_t sz = _tt.size();
    if (fwrite(&sz, sizeof sz, 1, fp) != 1) {
	fclose(fp);
	return oserr_str(filename);
    }

    for (TTMAP::const_iterator itr = _tt.begin() ; itr != _tt.end() ; itr++)
    {
	if (fwrite(&*itr, sizeof *itr, 1, fp) != 1) {
	    fclose(fp);
	    return oserr_str(filename);
	}
    }
    fclose(fp);
    return "";
}


//static
RESULT<ANSOLVER> ANSOLVER::read_from_file(const char* filename)
{
    std::string fn_colon = std::string(filename) + ": ";
    std::string err;

    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
	return RESULT<ANSOLVER>(oserr_str(filename));
    }

    uint32_t u;
    if ((err = read_thing(u, fp)) != "") {
	fclose(fp);
	return RESULT<ANSOLVER>(fn_colon + err);
    }
    if (u != FILE_HEADER) {
	fclose(fp);
	return RESULT<ANSOLVER>(fn_colon + "missing header");
    }

    PROBLEM problem;
    if ((err = problem.read_from_filestream(fp)) != "") {
	fclose(fp);
	return RESULT<ANSOLVER>(fn_colon + err);
    }

    RESULT<ANSOLVER> out(new ANSOLVER(problem));
    ANSOLVER* an = out.ok;

    if ((err = an->_b2.read_from_filestream(fp)) != "") {
	fclose(fp);
	return out.delete_and_error(fn_colon + err);
    }

    if ((err = read_thing(u, fp)) != "") {
	fclose(fp);
	return out.delete_and_error(err);
    }
    fprintf(stderr, "JORDAN: count of things to read: %u\n", u);
    for (uint32_t i=0 ; i<u ; i++) {
	TTMAP::value_type v;
	if ((err = read_thing(v, fp)) != "") {
	    fclose(fp);
	    return out.delete_and_error(err);
	}
	an->_tt.insert(v);
	if ((i&-i) == i) {
	    fprintf(stderr, "JORDAN: %u inserted %s  size=%zu\n",
		i, an->_hasher.hash_to_string(v.first).c_str(), an->_tt.size());
	}
    }
    fclose(fp);
    return out;
}


void ANSOLVER::fill_tt(const std::vector<CARD>& plays_so_far)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far);
    jassert(is_target_achievable(_p, sd.first));
    _dds_calls++;
    jassert(all_can_win(_p, sd.first, sd.second));

    std::map<hand64_t, bdt_t> visited;
    fill_tt_inner(visited, sd.first, sd.second);
}


void ANSOLVER::fill_tt_inner(std::map<hand64_t, bdt_t>& visited, STATE& state,
    const INTSET& dids)
{
    if (state.new_trick()) {
	hand64_t key = _hasher.hash(state);
	std::map<hand64_t, bdt_t>::iterator f = visited.find(key);
	if (f != visited.end()) {
	    if (_b2.contains(f->second, dids))
		return;
	    else
		f->second = _b2.unionize(f->second, _b2.cube(dids));
	} else {
	    visited[key] = _b2.cube(dids);
	}
    }

    bool res = eval(state, dids);
    if (!res)
	return;

    if (state.to_play_ns())
    {
	std::vector<CARD> usable_plays = find_usable_plays_ns(state, dids);
	std::vector<CARD>::const_iterator itr;

	for (itr = usable_plays.begin() ; itr != usable_plays.end() ; itr++)
	{
	    state.play(*itr);
	    fill_tt_inner(visited, state, dids);
	    state.undo();
	}
    }
    else
    {
	// state.to_play_ew()
	UPMAP plays = find_usable_plays_ew(_p, state, dids);
	UPMAP::const_iterator itr;
	size_t max_len = 0;

	for (itr = plays.begin() ; itr != plays.end() ; itr++)
	    if (itr->second.size() > max_len)
		max_len = itr->second.size();

	if (max_len == 1)
	    return;

	for (itr = plays.begin() ; itr != plays.end() ; itr++)
	{
	    CARD card = itr->first;
	    const INTSET& sub_dids = itr->second;
	    /* if (sub_dids.size() == 1)
		continue; */
	    if (sub_dids.size() != max_len)
		continue;

	    state.play(card);
	    fill_tt_inner(visited, state, sub_dids);
	    state.undo();
	}
    }
}


void ANSOLVER::compare_tt(const ANSOLVER& b) const
{
    TTMAP::const_iterator bitr;

    for (bitr = b._tt.begin() ; bitr != b._tt.end() ; bitr++) {
	const TTMAP::const_iterator f = _tt.find(bitr->first);
	if (f == _tt.end()) {
	    printf("Not in left: %016llx = %s\n", bitr->first,
		STATE_HASHER::hash_to_string(bitr->first).c_str());
	}
    }
}
