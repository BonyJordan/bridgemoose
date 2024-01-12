#include <assert.h>
#include <stdio.h>
#include "jassert.h"
#include "state.h"

STATE::STATE(int trump) :
    _played(0),
    _num_played(0),
    _ns_tricks(0),
    _ew_tricks(0),
    _to_play(J_WEST),
    _trump(trump)
{
    _leader[0] = J_WEST;
    _show_out_status[0] = 0;
}


STATE::~STATE()
{
}


void STATE::play(const CARD& card)
{
    assert(_num_played < 52);
    _history[_num_played] = card;
    _played |= card_to_handbit(card);

    int new_so_status = _show_out_status[_num_played];
    if (_num_played % 4 != 0 && to_play_ew()) {
	int sl = suit_led();
	if (card.suit != sl) {
	    int bn = sl * 2;
	    if (_to_play == J_EAST)
		bn += 1;
	    new_so_status |= (1 << bn);
	}
    }
    _num_played++;
    _show_out_status[_num_played] = new_so_status;

    assert(handbits_count(_played) == _num_played);

    if (_num_played % 4 == 0)
    {
	int winner = compute_winner();
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


int STATE::compute_winner() const
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
	} else if (card.suit == _trump) {
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
    hand64_t out = 0;
    // bits [12,64), whether each card has been played yet
    int so_key = 0;
    for (int suit=0 ; suit<4 ; suit++) {
	out <<= 13;
	out |= (_played >> (16*suit+2)) & 0x1fff;
	so_key *= 3;
	so_key += (_show_out_status[_num_played] >> (2*suit)) % 3;
    }

    out <<= 7;
    jassert(so_key >= 0 && so_key < 81);
    // bits [5,12), show out state
    out |= so_key;

    // bits [3,5), whose turn to play
    out <<= 2;
    jassert(_to_play >= 0 && _to_play < 4);
    out |= _to_play;

    // bits [0,3), num ew_tricks so far
    // This is assuming we are always trying to make a contract and have
    // failed out of a search by this point
    out <<= 3;
    jassert(_ew_tricks >= 0 && _ew_tricks < 8);
    out |= _ew_tricks;

    return out;
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
