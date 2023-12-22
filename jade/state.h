#ifndef _STATE_H_
#define _STATE_H_

#include <assert.h>
#include <string>
#include "cards.h"

class STATE
{
    hand64_t	_played;
    CARD	_history[52];
    int		_leader[13];
    int		_num_played;
    int		_ns_tricks;
    int		_ew_tricks;
    int		_to_play;
    int		_trump;

    int compute_winner() const;

  public:
    STATE(int trump);
    ~STATE();

    void play(const CARD& card);
    void undo();

    hand64_t played() const { return _played; }
    bool new_trick() const { return _num_played % 4 == 0; }
    int suit_led() const {
	assert(_num_played % 4 != 0);
	return _history[_num_played & ~3].suit;
    }
    int to_play() const { return _to_play; }
    bool to_play_ns() const
	{ return (_to_play == J_NORTH || _to_play == J_SOUTH); }
    bool to_play_ew() const
	{ return (_to_play == J_EAST || _to_play == J_WEST); }
    int ns_tricks() const { return _ns_tricks; }
    int ew_tricks() const { return _ew_tricks; }
    int trick_leader() const {
	return _leader[_num_played / 4];
    }
    int current_trick_num() const {
	return _num_played / 4;
    }
    CARD trick_card(int i) const {
	int tf = _num_played & ~3;
	if (tf + i >= _num_played)
	    return CARD();
	else
	    return _history[tf + i];
    }
    std::string to_string() const;

    hand64_t to_key() const;

    int num_played() const { return _num_played; }
    CARD history(int i) const
	{ assert(i >= 0 && i < _num_played); return _history[i]; }
};

#endif // _STATE_H_
