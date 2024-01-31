#include "sthash.h"
#include "jassert.h"


STATE_HASHER::STATE_HASHER(const PROBLEM& problem) :
    _problem(problem)
{
    precompute_table();
}


STATE_HASHER::~STATE_HASHER()
{
}


void STATE_HASHER::precompute_table()
{
    for (int suit=0 ; suit<4 ; suit++)
	for (uint16_t bits=0 ; bits<TBL_SIZE ; bits++)
	    _tbl[suit][bits] = compute_one(suit, bits << 2);
}


uint16_t STATE_HASHER::compute_one(int suit, uint16_t played) const
{
    const bool _debug = false;

    enum {
	NORTH, SOUTH, DEF, USED
    } owners[13];
    uint16_t north = hand_suit_bits(_problem.north, suit);
    uint16_t south = hand_suit_bits(_problem.south, suit);

    if (_debug)
	printf("START: %04x\n", played);

    for (int i=0 ; i<13 ; i++) {
	uint16_t bit = 4 << i;
	if (bit & played)
	    owners[i] = USED;
	else if (bit & north)
	    owners[i] = NORTH;
	else if (bit & south)
	    owners[i] = SOUTH;
	else
	    owners[i] = DEF;
    }

    int start = 0;
    uint16_t not_used = 0;
    while (start < 13)
    {
	int end = start;
	while (end < 13 && owners[end] != DEF)
	    end++;

	if (_debug)
	    printf("slice: start=%d end=%d\n", start, end);

	// Within the slice  start <= x < end
	// find the lowest unplayed card (x) and shift that down to 
	// the lowest index (y) for that player
	int y = start;
	int x = start;
	while (x < end)
	{
	    uint16_t find_in;
	    if (owners[x] == NORTH)
		find_in = north;
	    else if (owners[x] == SOUTH)
		find_in = south;
	    else if (owners[x] == USED) {
		x++;
		continue;
	    } else
		jassert(false);

	    while ( (find_in & (4<<y)) == 0) {
		jassert(y <= x);
		y++;
	    }
	    if (_debug)
		printf("found here card: x=%d y=%d\n", x, y);
	    not_used |= (4<<y);
	    x++;
	    y++;
	}

	jassert(x <= end);
	if (end < 13) {
	    not_used |= (4<<end);
	    if (_debug)
		printf("found def card: end=%d\n", end);
	}
	start = end+1;
    }
    if (_debug)
	printf("end: not_used=%04x\n", not_used);
    return 0x7ffc ^ not_used;
}


uint64_t STATE_HASHER::hash(const STATE& state) const 
{
    uint64_t out = 0;
    // bits [12,64), compressed version of whether each card has been played
    int so_key = 0;
    for (int suit=0 ; suit<4 ; suit++)
    {
	out <<= 13;
	uint16_t suit_played = hand_suit_bits(state.played(), suit);
	jassert((suit_played & 0x3) == 0);
	suit_played >>= 2;
	jassert(suit_played < (1<<13));
	out |= _tbl[suit][suit_played];

	so_key *= 3;
	so_key += ((state.show_out_status() >> (2*suit)) % 4) % 3;
    }
    out <<= 7;

        jassert(so_key >= 0 && so_key < 81);
    // bits [5,12), show out state
    out |= so_key;

    // bits [3,5), whose turn to play
    out <<= 2;
    jassert(state.to_play() >= 0 && state.to_play() < 4);
    out |= state.to_play();

    // bits [0,3), num ew_tricks so far
    // This is assuming we are always trying to make a contract and have
    // failed out of a search by this point
    out <<= 3;
    jassert(state.ew_tricks() >= 0 && state.ew_tricks() < 8);
    out |= state.ew_tricks();

    return out;
}


//static
std::string STATE_HASHER::hash_to_string(uint64_t hash)
{
    int ew_trix = hash & 0x7;
    hash >>= 3;
    int turn = hash & 0x3;
    hash >>= 2;
    int so_key = hash & 0x7f;
    hash >>= 7;

    int showout_status[4];
    char sss[4][16];

    for (int i=0 ; i<4 ; i++) {
	int suit = 3-i;
	showout_status[suit] = so_key % 3;
	so_key /= 3;

	int played = (hash & 0x1fff);
	char* p = sss[suit];
	const char* ranks = "23456789TJQKA";
	for (int j=0 ; j<13 ; j++)
	    if ((1<<j) & played)
		*p++ = ranks[j];
	*p = 0;

	hash >>= 13;
    }

    char buf[200];
    const char* dirs = "WNES";
    const char* sochar = "-EW";
    snprintf(buf, sizeof buf, "ew=%d t=%c so(%c,%c,%c,%c) pl=%s/%s/%s/%s",
	ew_trix, dirs[turn],
	sochar[showout_status[3]],
	sochar[showout_status[2]],
	sochar[showout_status[1]],
	sochar[showout_status[0]],
	sss[3], sss[2], sss[1], sss[0]);
    return buf;
}
