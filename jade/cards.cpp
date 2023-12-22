#include <assert.h>
#include <stdio.h>
#include "cards.h"

const char RANK_CHAR[] = "xx23456789TJQKA";
const char SUIT_CHAR[] = "CDHS";
const char DIR_CHAR[] = "WNES";

int rank_bit_to_index(unsigned int bit)
{
    const int map[] = { 0, 12, 1, 4, 2, 9, 5, 11, 3, 8, 10, 7, 6 };
    unsigned int x = bit >> 2;
    return 2 + map[(x - (x&1))%13];
}


CARD handbit_to_card(hand64_t bit)
{
    for (int s=0 ; s<4 ; s++) {
	if (bit & 0xffff) {
	    return CARD(s, rank_bit_to_index((unsigned int)(bit)));
	}
	bit >>= 16;
    }
    assert(false);
}


hand64_t card_to_handbit(const CARD& card)
{
    return 1ull << (16*card.suit + card.rank);
}


bool parse_hand(const char* hand, hand64_t& out)
{
    int nc = 0;
    int ns = 0;

    out = 0;

    for (const char* p = hand ; *p ; p++)
    {
	if (*p == '-')
	    continue;
	if (*p == '/') {
	    out <<= 16;
	    ns++;
	    if (ns == DDS_SUITS) {
		fprintf(stderr, "Too many suits: \"%s\"\n", hand);
		return false;
	    }
	    continue;
	}
	size_t r = 2;
	for ( ; r<sizeof RANK_CHAR ; r++)
	    if (*p == RANK_CHAR[r])
		break;

	if (r < sizeof RANK_CHAR) {
	    out |= (1 << r);
	    nc++;
	    if (nc > 13) {
		fprintf(stderr, "Too many cards: \"%s\"\n", hand);
		return false;
	    }
	} else {
	    fprintf(stderr, "Bad card: \'%c\' in \"%s\"\n", *p, hand);
	    return false;
	}
    }

    if (ns != DDS_SUITS - 1) {
	fprintf(stderr, "Too few suits: \"%s\"\n", hand);
	return false;
    }
    if (nc != 13) {
	fprintf(stderr, "Too few cards: \"%s\"\n", hand);
	return false;
    }

    return true;
}

std::string hand_to_string(hand64_t hand)
{
    char buf[13+3+3+1];
    char* p = buf;
    char rev[14];

    for (int s=0 ; s<DDS_SUITS ; s++) {
	if (s>0) {
	    *p++ = '/';
	}
	unsigned int x = (unsigned int) ((hand >> (16*(DDS_SUITS-1-s))) & 0xffff);
	if (x == 0) {
	    *p++ = '-';
	    continue;
	}

	int j = 0;
	while (x) {
	    unsigned int y = (x & -x);
	    rev[j++] = RANK_CHAR[rank_bit_to_index(y)];
	    x &= ~y;
	}
	for (j--; j>=0 ; j--)
	    *p++ = rev[j];
    }
    *p++ = 0;
    return std::string(buf);
}

bool parse_card(const char* card, CARD& out)
{
    for (int s=0 ; s<4 ; s++) {
	if (card[0] == SUIT_CHAR[s]) {
	    out.suit = s;
	    goto yay;
	}
    }
    fprintf(stderr, "Unknown suit `%c`\n", card[0]);
    return false;

  yay:
    for (size_t r=2 ; r<sizeof RANK_CHAR ; r++) {
	if (card[1] == RANK_CHAR[r]) {
	    out.rank = r;
	    return true;
	}
    }
    fprintf(stderr, "Unknown rank `%c`\n", card[1]);
    return false;
}

std::string card_to_string(const CARD& card)
{
    char buf[3];
    buf[0] = SUIT_CHAR[card.suit];
    buf[1] = RANK_CHAR[card.rank];
    buf[2] = 0;
    return std::string(buf);
}

void set_deal_cards(hand64_t hand, int pid, struct deal& dl)
{
    for (int s=0 ; s<4 ; s++) {
	dl.remainCards[pid][s] = (unsigned int)(hand & 0x7ffc);
	hand >>= 16;
    }
}

hand64_t dds_to_hand(const int array[4])
{
    hand64_t out = 0;
    for (int s=0 ; s<4 ; s++) {
	out |= hand64_t(array[s]) << (16*s);
    }
    return out;
}


int handbits_count(hand64_t hand)
{
    const hand64_t ones = ~hand64_t(0);
    const hand64_t am = ones / 3;
    const hand64_t bm = ones / 5;
    const hand64_t cm = ones / 17;
    const hand64_t dm = ones / 257;
    const hand64_t em = ones / 65537;

    hand64_t a = (hand & am) + ((hand >> 1) & am);
    hand64_t b = (a & bm) + ((a >> 2) & bm);
    hand64_t c = (b & cm) + ((b >> 4) & cm);
    hand64_t d = (c & dm) + ((c >> 8) & dm);
    hand64_t e = (d & em) + ((d >> 16) & em);
    hand64_t f = e + (e>>32);
    return int(f & 0xff);
}
