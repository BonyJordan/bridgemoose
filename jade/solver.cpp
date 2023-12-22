#include "solver.h"

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


BDT SOLVER::eval(STATE& state, const INTSET& dids)
{
    LUBDT search_bounds(set_to_atoms(dids), set_to_cube(dids));
    LUBDT result = doit(state, dids, search_bounds);
    return result.lower;
}


LUBDT SOLVER::doit(STATE& state, const INTSET& dids, LUBDT search_bounds)
{
    printf("Welcome to doit!\n");
    return search_bounds;
}
