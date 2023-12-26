#include "jassert.h"
#include "solver.h"

//////////////

class DDS_LOADER
{
    const PROBLEM& _problem;
    const STATE&   _state;

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
        jassert(_solved.noOfBoards == _bo.noOfBoards);
    }


    void load_some()
    {
        int k = 0;
        int target;

        if (_state.to_play_ns()) {
            target = _problem.target - _state.ns_tricks();
        } else {
            target = handbits_count(_problem.north) - _problem.target + 1 - _state.ew_tricks();
        }

        while (true)
        {
            if (!_itr.more() || k == MAXNOOFBOARDS) {
                _bo.noOfBoards = k;
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
            _bo.mode[k] = _mode;
            _bo.solutions[k] = _solutions;
            _bo.target[k] = target;

            k++;
            _itr.next();
        }
    }

  public:
    DDS_LOADER(const PROBLEM& problem, const STATE &state, const INTSET& dids,
        int mode, int solutions)
        :
        _problem(problem),_state(state),_itr(dids),
        _mode(mode),_solutions(solutions)
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

static BDT expand_bdt(BDT x, const INTSET& big_dids, const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only())
            x = x.extrude(itr.current());
    }
    return x;
}

static BDT reduce_bdt(BDT x, const INTSET& big_dids, const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only())
            x = x.remove(itr.current());
    }
    return x;
}

static BDT reduce_require_bdt(BDT x, const INTSET& big_dids,
    const INTSET& small_dids)
{
    for (INTSET_PAIR_ITR itr(big_dids, small_dids) ; itr.more() ; itr.next())
    {
        if (itr.a_only()) {
            x = x.require(itr.current());
            x = x.remove(itr.current());
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
    _all_cube = set_to_cube(_all_dids);
}


SOLVER::~SOLVER()
{
}


BDT SOLVER::eval(const std::vector<CARD> plays_so_far)
{
    INTSET dids = _all_dids;
    STATE state(_p.trump);

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
		    if ((_p.easts[itr.current()] & bit) != 0)
			found = true;
		} else {
		    if ((_p.wests[itr.current()] & bit) != 0)
			found = true;
		}
		if (found)
		    good_dids.insert(itr.current());
	    }
	    dids = good_dids;
	} else if (state.to_play() == J_NORTH) {
	    if ((_p.north & bit) == 0)
		dids.remove_all();
	} else if (state.to_play() == J_SOUTH) {
	    if ((_p.south & bit) == 0)
		dids.remove_all();
	} else {
	    // Impossible!
	    jassert(false);
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

    DDS_LOADER loader(_p, state, dids, 1, 1);
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
    if (state.ns_tricks() >= _p.target) {
	BDT cube = set_to_cube(dids);
	return LUBDT(cube, cube);
    } else if (handbits_count(_p.north) - state.ew_tricks() < _p.target) {
	// I think this never happens
	jassert(false);
    }

    LUBDT node_bounds(BDT(), _all_cube);
    bool new_trick = state.new_trick();
    hand64_t state_key = state.to_key();

    if (new_trick) {
        TTMAP::iterator f = _tt.find(state_key);
        if (f != _tt.end()) {
            node_bounds = f->second;
        }
    }

    INTSET node_dids = node_bounds.lower.get_used_vars();
    for (INTSET_PAIR_ITR itr(dids, node_dids) ; itr.more() ; itr.next()) {
        if (itr.a_only()) {
            node_bounds.lower |= BDT::atom(itr.current());
            node_bounds.upper = node_bounds.upper.extrude(itr.current());
        }
    }

    search_bounds.lower |= node_bounds.lower;
    search_bounds.upper &= node_bounds.upper;
    if (search_bounds.upper.subset_of(search_bounds.lower))
        return node_bounds;

    LUBDT out;
    if (state.to_play_ew())
        out = doit_ew(state, dids, search_bounds, node_bounds);
    else
        out = doit_ns(state, dids, search_bounds, node_bounds);

    if (new_trick) {
        _tt[state_key] = out;
    }
    return out;
}


LUBDT SOLVER::doit_ew(STATE& state, const INTSET& dids, LUBDT search_bounds,
    LUBDT node_bounds)
{
    UPMAP plays = find_usable_plays_ew(state, dids);

    BDT cum_lower = node_bounds.upper;

    UPMAP::const_iterator itr;
    for (itr=plays.begin() ; itr != plays.end() ; itr++)
    {
        CARD card = itr->first;
        const INTSET& sub_dids = itr->second;
        if (sub_dids.size() == 1) {
            continue;
        }

        BDT sub_lower = reduce_require_bdt(search_bounds.lower,
            dids, sub_dids);
        BDT sub_upper = reduce_bdt(search_bounds.upper,
            dids, sub_dids);
        LUBDT sub_bounds(sub_lower, sub_upper);

        state.play(card);
        LUBDT result = doit(state, sub_dids, sub_bounds);
        state.undo();

        search_bounds.upper &= expand_bdt(result.upper, dids, sub_dids);
        node_bounds.upper &= expand_bdt(result.upper, _all_dids, sub_dids);
        cum_lower &= expand_bdt(result.upper, _all_dids, sub_dids);

        if (search_bounds.upper.subset_of(search_bounds.lower))
            return node_bounds;
    }

    node_bounds.lower |= cum_lower;
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
    BDT cum_upper = node_bounds.lower;

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
        BDT sub_lower = reduce_bdt(search_bounds.lower, dids, sub_dids);
        BDT sub_upper = reduce_bdt(search_bounds.upper, dids, sub_dids);
        LUBDT sub_bounds(sub_lower, sub_upper);

        state.play(card);
        LUBDT result = doit(state, sub_dids, sub_bounds);
        state.undo();

        search_bounds.lower |= result.lower;
        node_bounds.lower |= result.lower;
        cum_upper |= result.upper;

        // Did we cutoff?
        if (search_bounds.upper.subset_of(search_bounds.lower)) {
            return node_bounds;
        }
    }

    node_bounds.upper &= cum_upper;
    // This will be true eventually, but not while doit_ew is empty!
    //jassert((search_bounds.upper & cum_upper).subset_of(search_bounds.lower));
    return node_bounds;
}


UPMAP SOLVER::find_usable_plays_ew(const STATE& state, const INTSET& dids) const
{
    UPMAP plays;
    hand64_t follow_bits = 0;
    if (!state.new_trick()) {
        follow_bits = suit_bits(state.trick_card(0).suit);
    }
    bool is_east = (state.to_play() == J_EAST);

    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next())
    {
        hand64_t hand =
            (is_east ? _p.easts[itr.current()] : _p.wests[itr.current()]);
        hand &= ~state.played();

        if ((hand & follow_bits) != 0)
            hand &= follow_bits;

        for (HAND_ITR hitr(hand) ; hitr.more() ; hitr.next()) {
            plays[hitr.current()].insert(itr.current());
        }
    }
    return plays;
}


UPMAP SOLVER::find_usable_plays_ns(const STATE& state, const INTSET& dids)
    const
{
    UPMAP out;
    DDS_LOADER loader(_p, state, dids, 0, 2);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
            const futureTricks& sb = loader.chunk_solution(i);
	    jassert(sb.score[0] != 0 && sb.score[0] != 1);
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

//////////////

std::string bdt_to_string(BDT bdt)
{
    bool first = true;
    std::vector<INTSET> cubes = bdt.get_cubes();
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
