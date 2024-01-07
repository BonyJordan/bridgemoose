#include "ddscache.h"
#include "jassert.h"
#include "solutil.h"

//static
DDS_KEY DDS_KEY::from_state(const STATE& state)
{
    DDS_KEY dk;
    dk.key = state.to_key();
    dk.did = 0;
    dk.trick_card_bits = 0;

    for (int i=0 ; i<3 ; i++) {
        CARD card = state.trick_card(i);
        dk.trick_card_bits <<= 6;
        dk.trick_card_bits |= (card.suit & 0x3) << 4;
        dk.trick_card_bits |= card.rank & 0xf;
    }
    return dk;
}


DDS_CACHE::DDS_CACHE(const PROBLEM& problem) :
    _problem(problem)
{
}


DDS_CACHE::~DDS_CACHE()
{
}


std::map<int, hand64_t> DDS_CACHE::solve_many(const STATE& state,
    const INTSET& dids)
{
    DDS_KEY key = DDS_KEY::from_state(state);
    std::map<int, hand64_t> out;
    INTSET work;

    for (INTSET_ITR itr(dids) ; itr.more() ; itr.next())
    {
	int did = itr.current();
	key.did = did;
	std::map<DDS_KEY, hand64_t>::const_iterator f = _data.find(key);
	if (f == _data.end())
	    work.insert(did);
	else
	    out[did] = f->second;
    }

    if (work.empty())
	return out;

    DDS_LOADER loader(_problem, state, work, 1, 2);
    for ( ; loader.more() ; loader.next())
    {
	for (int i=0 ; i<loader.chunk_size() ; i++) {
	    const futureTricks& sb = loader.chunk_solution(i);
	    hand64_t wins = 0;
	    for (int j=0 ; j<sb.cards ; j++) {
		jassert(sb.score[j] != -2);
		if (sb.score[j] == 0 || sb.score[j] == -1)
		    assert(j == 0);
		else {
		    CARD card(sb.suit[j], sb.rank[j]);
		    jassert(card.valid());
		    wins |= card_to_handbit(card);
		}
	    }
	    key.did = loader.chunk_did(i);
	    out[key.did] = wins;
	    _data[key] = wins;
	}
    }

    return out;
}
