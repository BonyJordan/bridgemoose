#include <assert.h>
#include <stdio.h>
#include "state.h"

STATE::STATE() :
    _played(0),
    _num_played(0),
    _ns_tricks(0),
    _ew_tricks(0),
    _to_play(J_WEST)
{
    _leader[0] = J_WEST;
}


STATE::~STATE()
{
}


void STATE::play(const CARD& card, int trump)
{
    assert(_num_played < 52);
    _history[_num_played] = card;
    _played |= card_to_handbit(card);
    _num_played++;
    assert(handbits_count(_played) == _num_played);

    if (_num_played % 4 == 0)
    {
	int winner = compute_winner(trump);
	if (winner == J_EAST || winner == J_WEST) {
	    _ew_tricks++;
	} else {
	    _ns_tricks++;
	}

	int n = _num_played / 4;
	assert(n >= 0 && n < 13);
	_leader[n] = winner;
	_to_play = winner;
    } else {
	_to_play = (_to_play + 1) % 4;
    }
}


void STATE::undo()
{
    if (_num_played % 4 == 0)
    {
	int winner = _leader[_num_played / 4];
	if (winner == J_EAST || winner == J_WEST)
	    _ew_tricks--;
	else
	    _ns_tricks--;

	_to_play = (_leader[_num_played/4-1] + 3) % 4;
    } else {
	_to_play = (_to_play + 3) % 4;
    }

    _num_played--;
    _played &= ~card_to_handbit(_history[_num_played]);
    assert(handbits_count(_played) == _num_played);
}


int STATE::compute_winner(int trump) const
{
    assert(_num_played % 4 == 0);
    CARD winning_card = _history[_num_played - 4];
    int n = _num_played/4 - 1;
    assert(n >= 0 && n<13);
    int winner = _leader[n];

    for (int i=0 ; i<3 ; i++) {
	CARD card = _history[_num_played - 3 + i];
	if (card.suit == winning_card.suit) {
	    if (card.rank > winning_card.rank) {
		winning_card = card;
		int n = _num_played/4 - 1;
		assert(n >= 0 && n<13);
		winner = (_leader[n] + i + 1) % 4;
	    }
	} else if (card.suit == trump) {
	    winning_card = card;
	    int n = _num_played/4 - 1;
	    assert(n >= 0 && n<13);
	    winner = (_leader[n] + i + 1) % 4;
	}
    }
    return winner;
}


hand64_t STATE::to_key() const
{
    hand64_t nst = _ns_tricks;
    return _played | ((nst & 0xc) << 30) | ((nst & 0x3) << 16) | (_to_play & 0x3);
}


std::string STATE::to_string() const
{
    char buf[52*3];
    char *p = buf;
    for (int i=0 ; i<_num_played ; i++) {
	if (i == 0)
	    ;
	else if (i % 4 == 0)
	    *p++ = ' ';
	else
	    *p++ = ',';

	std::string cs = card_to_string(_history[i]);
	*p++ = cs[0];
	*p++ = cs[1];
    }
    *p = 0;
    return std::string(buf);
}
