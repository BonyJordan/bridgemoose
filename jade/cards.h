#ifndef _CARDS_H_
#define _CARDS_H_

#include "../dds/dll.h"
#include "../dds/dds_api.h"
#include <string>

#define J_WEST 0
#define J_NORTH 1
#define J_EAST 2
#define J_SOUTH 3

#define ALL_CARDS_BITS  0x7ffc7ffc7ffc7ffc

typedef unsigned long long hand64_t;
struct CARD {
    CARD() : suit(0),rank(0) {}
    CARD(const CARD& c) : suit(c.suit),rank(c.rank) {}
    CARD(int s, int r) : suit(s),rank(r) {}

    operator bool() const { return rank != 0; }
    bool operator <(const CARD& a) const { return suit*16+rank < a.suit*16+a.rank; }
    bool operator ==(const CARD& a) const { return suit==a.suit && rank==a.rank; }
    bool valid() const { return suit>=0 && suit<=3 && rank>=2 && rank<=14; }

    int suit;	// 0 to 3
    int rank;	// 2 to 14
};

extern const char RANK_CHAR[];
extern const char SUIT_CHAR[];
extern const char DIR_CHAR[];
inline hand64_t suit_bits(int suit) { return 0x7ffcll << (16*suit); }
inline uint16_t hand_suit_bits(hand64_t hand, int suit) {
    return (hand >> (16*suit)) & 0x7ffc;
}
int rank_bit_to_index(unsigned int bit);
CARD handbit_to_card(hand64_t bit);
hand64_t card_to_handbit(const CARD& card);
void set_deal_cards(hand64_t hand, int pid, struct deal& dl);
hand64_t dds_to_hand(const int array[4]);
int handbits_count(hand64_t hand);

extern bool parse_hand(const char* hand, hand64_t& out);
extern std::string hand_to_string(hand64_t hand);
extern bool parse_card(const char* card, CARD& out);
extern std::string card_to_string(const CARD& card);


class HAND_ITR
{
  private:
    hand64_t	_left;

  public:
    HAND_ITR(hand64_t hand) : _left(hand) {}
    ~HAND_ITR() {}

    bool more() const { return _left != 0; }
    CARD current() const {
	hand64_t bit = _left & -_left;
	return handbit_to_card(bit);
    }
    hand64_t current_bit() const {
	return _left & -_left;
    }

    void next() {
	hand64_t bit = _left & -_left;
	_left &= ~bit;
    }
};

template <class T>
void bitsort_swap(T& lo, T& hi)
{
    T lo_left = lo;
    T hi_left = hi;

    while (lo_left && hi_left)
    {
	T lo_bit = (lo_left & -lo_left);
	T hi_bit = (hi_left & -hi_left);

	if (lo_bit < hi_bit) {
	    lo_left &= ~lo_bit;
	    continue;
	}

	// swaperoo
	T swap = lo_bit | hi_bit;
	lo ^= swap;
	hi ^= swap;
	hi_left ^= swap;
	lo_left &= ~lo_bit;
    }
}

#endif // _CARDS_H_
