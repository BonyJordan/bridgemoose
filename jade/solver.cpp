#include <utility>
#include "jassert.h"
#include "solutil.h"
#include "solver.h"

//////////////////////

static bdt_t expand_bdt(BDT_MANAGER& b2, bdt_t x,
    const INTSET& big_dids, const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only())
            x = b2.extrude(x, itr.current());
    }
    return x;
}

static bdt_t reduce_bdt(BDT_MANAGER& b2, bdt_t x,
    const INTSET& big_dids, const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only())
            x = b2.remove(x, itr.current());
    }
    return x;
}

static bdt_t reduce_require_bdt(BDT_MANAGER& b2, bdt_t x,
    const INTSET& big_dids, const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only()) {
            x = b2.require(x, itr.current());
            x = b2.remove(x, itr.current());
        }
    }
    return x;
}


//////////////////////

SOLVER::SOLVER(const PROBLEM& problem)
    :
    _p(problem)
{
    jassert(_p.wests.size() == _p.easts.size());
    _all_dids = INTSET::full_set((int)_p.wests.size());
    _all_cube = set_to_cube(_b2, _all_dids);
#define A(x)	_ ## x = 0;
    SOLVER_STATS(A)
#undef A
}


SOLVER::~SOLVER()
{
}


bdt_t SOLVER::eval(const std::vector<CARD> plays_so_far)
{
    std::pair<STATE, INTSET> sd = load_from_history(_p, plays_so_far);
    eval_2(sd.first, sd.second);

    return eval(sd.first, sd.second);
}


void SOLVER::eval_2(STATE& state, INTSET& dids)
{
    // The inner workings of SOLVER have the invariant that every
    // deal in <dids> has at least one double-dummy solution for the
    // target number of tricks... so we need to filter out those which
    // don't.

    // mode 1: find the actual score
    // solutions 1: find any card which succeeds

    DDS_LOADER loader(_p, state, dids, 1, 1);
    _dds_calls++;
    _dds_boards += dids.size();
    track_dds(state, dids);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
	    int score = loader.chunk_solution(i).score[0];
	    if (state.to_play_ns() && score <= 0) {
		printf("WARN: removing did %d for failure to win\n",
		    loader.chunk_did(i));
		dids.remove(loader.chunk_did(i));
	    }
	    if (state.to_play_ew() && score > 0) {
		printf("WARN: removing did %d for failure to win\n",
		    loader.chunk_did(i));
		dids.remove(loader.chunk_did(i));
	    }
	}
    }
}


bdt_t SOLVER::eval(STATE& state, const INTSET& dids)
{
    LUBDT search_bounds(set_to_atoms(_b2, dids), set_to_cube(_b2, dids));
    LUBDT result = doit(state, dids, search_bounds);
    return _b2.intersect(
	search_bounds.upper,
	_b2.unionize(result.lower, search_bounds.lower));
}


LUBDT SOLVER::doit(STATE& state, const INTSET& dids, LUBDT search_bounds)
{
    const bool debug = false;
    _node_visits++;
    
    if (state.ns_tricks() >= _p.target) {
	bdt_t cube = set_to_cube(_b2, dids);
	return LUBDT(cube, cube);
    } else if (handbits_count(_p.north) - state.ew_tricks() < _p.target) {
	// I think this never happens
	jassert(false);
    }

    LUBDT node_bounds(0, _all_cube);
    bool new_trick = state.new_trick();
    hand64_t state_key = state.to_key();

    if (new_trick) {
        TTMAP::iterator f = _tt.find(state_key);
        if (f != _tt.end()) {
            node_bounds = f->second;
	    _cache_hits++;
	    if (debug) {
		printf("SOLVER::doit cache hit lb=%s ub=%s\n",
		    bdt_to_string(_b2, node_bounds.lower).c_str(),
		    bdt_to_string(_b2, node_bounds.upper).c_str());
		}
        } else {
	    _cache_misses++;
	    if (debug) {
		printf("SOLVER::doit cache miss\n");
	    }
	}
    }

    INTSET node_dids = _b2.get_used_vars(node_bounds.lower);
    for (INTSET_PAIR_ITR itr(dids, node_dids) ; itr.more() ; itr.next()) {
        if (itr.a_only()) {
            node_bounds.lower = _b2.unionize(
		node_bounds.lower,
		_b2.atom(itr.current()));
            node_bounds.upper = _b2.extrude(node_bounds.upper, itr.current());
        }
    }

    search_bounds.lower |= node_bounds.lower;
    search_bounds.upper &= node_bounds.upper;
    if (_b2.subset_of(search_bounds.upper, search_bounds.lower))
        return node_bounds;

    if (debug) {
	printf("SOLVER::doit  about to compute. dids=%s  state=[%s]\n",
	    intset_to_string(dids).c_str(), state.to_string().c_str());
    }
    LUBDT out;
    if (state.to_play_ew())
        out = doit_ew(state, dids, search_bounds, node_bounds);
    else
        out = doit_ns(state, dids, search_bounds, node_bounds);

    if (debug) {
	printf("SOLVER::doit  done with compute. state=[%s]\n",
	    state.to_string().c_str());
	printf("out.lower = %s\n", bdt_to_string(_b2, out.lower).c_str());
	printf("out.upper = %s\n", bdt_to_string(_b2, out.upper).c_str());
    }

    if (new_trick) {
	if (_tt.find(state_key) == _tt.end())
	    _cache_size++;

        _tt[state_key] = out;
    }
    return out;
}


LUBDT SOLVER::doit_ew(STATE& state, const INTSET& dids, LUBDT search_bounds,
    LUBDT node_bounds)
{
    UPMAP plays = find_usable_plays_ew(_p, state, dids);

    bdt_t cum_lower = node_bounds.upper;

    UPMAP::const_iterator itr;
    for (itr=plays.begin() ; itr != plays.end() ; itr++)
    {
        CARD card = itr->first;
        const INTSET& sub_dids = itr->second;
        if (sub_dids.size() == 1) {
            continue;
        }

        bdt_t sub_lower = reduce_require_bdt(_b2, search_bounds.lower,
            dids, sub_dids);
        bdt_t sub_upper = reduce_bdt(_b2, search_bounds.upper,
            dids, sub_dids);
        LUBDT sub_bounds(sub_lower, sub_upper);

        state.play(card);
        LUBDT result = doit(state, sub_dids, sub_bounds);
        state.undo();

        search_bounds.upper = _b2.intersect(
	    search_bounds.upper,
	    expand_bdt(_b2, result.upper, dids, sub_dids));
        node_bounds.upper = _b2.intersect(
	    node_bounds.upper,
	    expand_bdt(_b2, result.upper, _all_dids, sub_dids));
        cum_lower = _b2.intersect(
	    cum_lower,
	    expand_bdt(_b2, result.upper, _all_dids, sub_dids));

        if (_b2.subset_of(search_bounds.upper, search_bounds.lower))
            return node_bounds;
    }

    node_bounds.lower = _b2.unionize(
	node_bounds.lower,
	cum_lower);
    return node_bounds;
}


LUBDT SOLVER::doit_ns(STATE& state, const INTSET& dids, LUBDT search_bounds,
    LUBDT node_bounds)
{
    const bool debug = false;
    if (debug) {
        printf("welcome to doit_ns\n");
    }

    UPMAP usable_plays = find_usable_plays_ns(state, dids);
    bdt_t cum_upper = node_bounds.lower;

    while (!usable_plays.empty())
    {
        CARD card = recommend_usable_play(usable_plays);
        // better copy 'cause we're about to delete it
        INTSET sub_dids = usable_plays[card];
        usable_plays.erase(card);

        jassert(sub_dids.size() > 0);

        // Given our invariants, no information is gleaned.
        if (sub_dids.size() == 1)
            continue;

        // Okay, try the play and see what happens!
        bdt_t sub_lower = reduce_bdt(_b2, search_bounds.lower, dids, sub_dids);
        bdt_t sub_upper = reduce_bdt(_b2, search_bounds.upper, dids, sub_dids);
        LUBDT sub_bounds(sub_lower, sub_upper);

        state.play(card);
        LUBDT result = doit(state, sub_dids, sub_bounds);
        state.undo();

	result.lower = reduce_bdt(_b2, result.lower, dids, sub_dids);
	result.upper = reduce_bdt(_b2, result.upper, dids, sub_dids);

        search_bounds.lower |= result.lower;
        node_bounds.lower |= result.lower;
        cum_upper |= result.upper;

        // Did we cutoff?
        if (_b2.subset_of(search_bounds.upper, search_bounds.lower)) {
            return node_bounds;
        }
    }

    node_bounds.upper &= cum_upper;
    // This will be true eventually, but not while doit_ew is empty!
    //jassert((search_bounds.upper & cum_upper).subset_of(search_bounds.lower));
    return node_bounds;
}


UPMAP SOLVER::find_usable_plays_ns(const STATE& state, const INTSET& dids)
{
    UPMAP out;
    DDS_LOADER loader(_p, state, dids, 0, 2);
    _dds_calls++;
    _dds_boards += dids.size();
    track_dds(state, dids);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
            const futureTricks& sb = loader.chunk_solution(i);
	    jassert(sb.score[0] != 0 && sb.score[0] != -1);
            for (int j=0 ; j<sb.cards ; j++) {
                CARD card(sb.suit[j], sb.rank[j]);
                out[card].insert(loader.chunk_did(i));
	    }
	}
    }

    return out;
}


CARD SOLVER::recommend_usable_play(const UPMAP& upmap) const
{
    UPMAP::const_iterator best = upmap.begin();
    UPMAP::const_iterator itr = best;
    jassert(itr != upmap.end());

    for (itr++ ; itr != upmap.end() ; itr++) {
        if (itr->second.size() > best->second.size())
            best = itr;
    }
    return best->first;
}

std::map<std::string, stat_t> SOLVER::get_stats() const
{
    std::map<std::string, stat_t> out;
#define A(x)	out[# x] = _ ## x;
    SOLVER_STATS(A)
#undef A 

    size_t bdt_sizes[BDT_MANAGER::MAP_NUM];
    _b2.get_map_sizes(bdt_sizes);

    size_t i=0;
    out["bdt_nodes"] = bdt_sizes[i++];
    out["bdt_union_map"] = bdt_sizes[i++];
    out["bdt_intersect_map"] = bdt_sizes[i++];
    out["bdt_extrude_map"] = bdt_sizes[i++];
    out["bdt_remove_map"] = bdt_sizes[i++];
    out["bdt_require_map"] = bdt_sizes[i++];
    assert(i == BDT_MANAGER::MAP_NUM);

    return out;
}

void SOLVER::track_dds(const STATE& state, const INTSET& dids)
{
    DDS_KEY dk = DDS_KEY::from_state(state);

    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next()) {
	dk.did = itr.current();
	if (_dds_tracker.find(dk) != _dds_tracker.end()) {
	    _dds_repeats++;
	} else {
	    _dds_tracker.insert(dk);
	}
    }
}


//////////////

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
