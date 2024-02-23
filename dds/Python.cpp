#include <Python.h>
#include "dll.h"
#include "dds_api.h"

static PyObject* _deal_type = NULL;
static PyObject* _hand_type = NULL;
static DDS_C_API _dds_c_api;

static PyObject*
dds_error(int r)
{
    char line[80];
    ErrorMessage(r, line);
    return PyErr_Format(PyExc_TypeError, "DDS Error: %s", line);
}

const char* SUITS = "SHDC";
const char* RANKS = "23456789TJQKA";
const char* DIRS = "WNES";

#define RETURN_ASSERT   return PyErr_Format(PyExc_AssertionError, "Horror occured at %s:%d", __FILE__, __LINE__)


static int bitcount(int a)
{
    int b = (a & 0x5555) + ((a>>1) & 0x5555);
    int c = (b & 0x3333) + ((b>>2) & 0x3333);
    int d = (c & 0x0f0f) + ((c>>4) & 0x0f0f);
    int e = (d & 0x00ff) + ((d>>8) & 0x00ff);
    return e;
}


static int string_to_dir(const char* s)
{
    switch (s[0]) {
    case 'W':
    case 'w':
        return 0;
    case 'N':
    case 'n':
        return 1;
    case 'E':
    case 'e':
        return 2;
    case 'S':
    case 's':
        return 3;
    default:
        return -1;
    }
}


static int char_to_rank_number(char c)
{
    for (int i=0 ; i<13 ; i++)
        if (RANKS[i] == c)
            return i + 2;
    return -1;
}


static int char_to_rank_bit(char c)
{
    for (int i=0 ; i<13 ; i++)
        if (RANKS[i] == c)
            return 4 << i;
    return -1;
}

static int char_to_suit(char c)
{
    switch (c) {
    case 'C':
    case 'c':
        return 3;
    case 'D':
    case 'd':
        return 2;
    case 'H':
    case 'h':
        return 1;
    case 'S':
    case 's':
        return 0;
    default:
        return -1;
    }
}


static int string_to_strain(const char* s)
{
    switch (s[0]) {
    case 'C':
    case 'c':
        return 3;
    case 'D':
    case 'd':
        return 2;
    case 'H':
    case 'h':
        return 1;
    case 'S':
    case 's':
        return 0;
    case 'N':
    case 'n':
        return 4;
    default:
        return -1;
    }
}

static void suit_rank_str(int suit, int rank, char* card)
{
    if (suit < 0 || suit >= 4)
        card[0] = '?';
    else
        card[0] = SUITS[suit];
    card[2] = '\0';

    if (rank < 2 || rank > 14) {
	printf("Weird ass rank: %d\n", rank);
	card[1] = '?';
    } else
	card[1] = RANKS[rank-2];
}

static void
set_win_rank_string(char wr_string[5], const int winRanks[4]) {
    for (int suit=0 ; suit<4 ; suit++) {
	int wr = winRanks[3-suit] | 0x8000;
	int rank;
	for (rank=0 ; rank<13 ; rank++)
	    if (wr & (8<<rank))
		break;
	if (rank < 13)
	    wr_string[suit] = RANKS[rank];
	else
	    wr_string[suit] = '?';
    }
    wr_string[4] = '\0';
}


struct CurrentTrick {
    int suit[3];
    int rank[3];
    int nonZero;

    PyObject* from_string(const char* trick_so_far) {
	nonZero = 0;
	for (int i=0 ; i<3 ; i++) {
	    suit[i] = 0;
	    rank[i] = 0;
	}

	for (int i=0 ; i<3 ; i++) {
	    if (trick_so_far[i*2] == '\0')
		break;
	    suit[i] = char_to_suit(trick_so_far[i*2]);
	    if (suit[i] == -1)
		return PyErr_Format(PyExc_ValueError, "Bad suit in '%s'",
		    trick_so_far);
	    rank[i] = char_to_rank_number(trick_so_far[i*2+1]);
	    if (rank[i] < 0)
            return PyErr_Format(PyExc_ValueError, "Bad rank in '%s'",
		trick_so_far);

	    nonZero += 1;
	}

	return Py_None;
    }
};


static PyObject*
python_objects_to_deal(struct deal& dl, PyObject* py_deal, const char* declarer,
    const char* strain)
{
    int dec_dir_id = string_to_dir(declarer);
    if (dec_dir_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad declarer '%s'", declarer);

    int strain_id = string_to_strain(strain);
    if (strain_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad strain '%s'", strain);

    // Now let's turn it into DDS format.
    dl.trump = strain_id;
    dl.first = (dec_dir_id + 1) % 4;
    for (int i=0 ; i<3 ; i++) {
        dl.currentTrickSuit[i] = 0;
        dl.currentTrickRank[i] = 0;
    }

    // Load the cards from py_deal to dl.

    for (int hid=0 ; hid<4 ; hid++)
    {
        char attr_name[2];
        snprintf(attr_name, sizeof attr_name, "%c", DIRS[hid]);
        PyObject* hand = PyObject_GetAttrString(py_deal, attr_name);
        if (hand == NULL)
            return NULL;

        for (int sid=0 ; sid<4 ; sid++)
        {
            dl.remainCards[hid][sid] = 0;
            snprintf(attr_name, sizeof attr_name, "%c", SUITS[sid]);
            PyObject* ranks = PyObject_GetAttrString(hand, attr_name);
            if (ranks == NULL) {
                Py_DECREF(hand);
                return NULL;
            }
            const char* pyu = PyUnicode_AsUTF8(ranks);
            if (pyu == NULL) {
                Py_DECREF(ranks);
                Py_DECREF(hand);
                return NULL;
            }

            while (*pyu) {
                int r = char_to_rank_bit(*pyu);
                if (r < 0) {
                    Py_DECREF(ranks);
                    Py_DECREF(hand);
                    return PyErr_Format(PyExc_ValueError, "Bad rank '%c'",
                        *pyu);
                }
                dl.remainCards[hid][sid] |= r;
                pyu++;
            }
            Py_DECREF(ranks);
        }
        Py_DECREF(hand);
    }

    // don't need to incref because my consumers don't use the value
    return Py_None;
}


static PyObject*
python_tuple_to_deal(struct deal& dl, PyObject* tuple)
{
    // deal, declarer, strain
    PyObject* py_deal;
    const char* declarer;
    const char* strain;
    if (!PyArg_ParseTuple(tuple, "O!ss", _deal_type, &py_deal, &declarer,
        &strain))
    {
        return NULL;
    }

    return python_objects_to_deal(dl, py_deal, declarer, strain);
}


static PyObject*
dds_solve_many_deals(PyObject* self, PyObject* args)
{
    struct boards boards;
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list))
        return NULL;

    PyObject* iter = PyObject_GetIter(py_list);
    if (iter == NULL)
        return NULL;

    PyObject* out_list = PyList_New(0);
    int num_deals = 0;
    while (true) {
	PyObject* py_deal = PyIter_Next(iter);
        if (py_deal == NULL || num_deals >= MAXNOOFBOARDS)
	{
	    // Time to run a chunk!!
	    struct solvedBoards solves;
	    boards.noOfBoards = num_deals;
	    int ret = SolveAllChunksBin(&boards, &solves, 1);
	    if (ret < 0) {
		if (py_deal != NULL)
		    Py_DECREF(py_deal);
		return dds_error(ret);
	    }

	    for (int i=0 ; i<solves.noOfBoards ; i++) {
		PyObject* val = PyLong_FromLong(13 - solves.solvedBoard[i].score[0]);
		if (val == NULL) {
		    if (py_deal != NULL)
			Py_DECREF(py_deal);
		    Py_DECREF(out_list);
		    return NULL;
		}
		if (PyList_Append(out_list,  val) < 0) {
		    if (py_deal != NULL)
			Py_DECREF(py_deal);
		    Py_DECREF(out_list);
		    return NULL;
		}
	    }
	    num_deals = 0;
	}

	if (py_deal == NULL)
	    break;

        PyObject* py_ret = python_tuple_to_deal(boards.deals[num_deals],
	    py_deal);
        if (py_ret != Py_None)
            return py_ret;

        boards.target[num_deals] = -1;
        boards.solutions[num_deals] = 1;
        boards.mode[num_deals] = 0;
        num_deals++;
        Py_DECREF(py_deal);
    }

    return out_list;
}


static PyObject*
dds_solve_deal(PyObject* self, PyObject* args)
{
    struct deal dl;
    PyObject* py_ret = python_tuple_to_deal(dl, args);

    if (py_ret != Py_None)
        return py_ret;

    struct futureTricks futs;
    int ret = SolveBoard(dl, -1, 1, 0, &futs, 0);
    if (ret < 0)
        return dds_error(ret);

    // SolveBoard gives us the number of tricks for "the side on lead",
    // but we really want it from declarer's point of view.
    return Py_BuildValue("i", 13-futs.score[0]);
}

//
// Returns Py_None if no error found
//
static PyObject* load_one_board_pbn(struct dealPBN& deal, PyObject* py_deal,
    const int ctSuit[3], const int ctRank[3], int strain_id, int first_dir_id)
{
    const char* h[4];
    if (!PyArg_ParseTuple(py_deal, "ssss", &h[0], &h[1], &h[2], &h[3]))
	return NULL;

    deal.trump = strain_id;
    deal.first = first_dir_id;
    for (int i=0 ; i<3 ; i++) {
	deal.currentTrickSuit[i] = ctSuit[i];
	deal.currentTrickRank[i] = ctRank[i];
    }

    int i = 0;
    deal.remainCards[i++] = 'N';
    deal.remainCards[i++] = ':';

    for (int j=0 ; j<4 ; j++) {
	if (j != 0) {
	    if (i >= 78) {
		return PyErr_Format(PyExc_ValueError,
                        "Too many cards specified");
	    }
	    deal.remainCards[i++] = ' ';
	}

	for (const char* cp = h[j] ; *cp ; cp++) {
	    if (i >= 78) {
		return PyErr_Format(PyExc_ValueError,
		    "Too many cards specified");
	    }

	    if (*cp == '-')
		continue;
	    else if (*cp == '/')
		deal.remainCards[i++] = '.';
	    else
		deal.remainCards[i++] = *cp;
	}
    }
    deal.remainCards[i] = 0;
    return Py_None;
}

// Returns:
//   -1 on error
//    0 if the iterator has more goodies left
//    1 if the iterator is done
// Returns Py_None if okay, else an error of some sort to be passed along
static int
load_boards_pbn(PyObject* py_iter, struct boardsPBN& boards,
    const int ctSuit[3], const int ctRank[3], int strain_id, int first_dir_id,
    PyObject** error)
{
    *error = NULL;
    int num_deals = 0;

    PyObject* py_deal;
    while ((py_deal = PyIter_Next(py_iter)))
    {
	*error = load_one_board_pbn(boards.deals[num_deals], py_deal,
	    ctSuit, ctRank, strain_id, first_dir_id);
	if (*error != Py_None) {
	    Py_DECREF(py_deal);
	    Py_DECREF(py_iter);
	    return -1;
	}

        boards.target[num_deals] = -1;
        boards.solutions[num_deals] = 3;
        boards.mode[num_deals] = 1;

        num_deals += 1;
        Py_DECREF(py_deal);

	if (num_deals == MAXNOOFBOARDS) {
	    boards.noOfBoards = num_deals;
	    return 0;
	}
    }
    boards.noOfBoards = num_deals;
    return 1;
}



static PyObject*
dds_solve_many_plays(PyObject* self, PyObject* args)
{
    PyObject* py_list;
    const char* play_dir;
    const char* strain;
    const char* trick_so_far;
    int want_win_ranks = 0;
    if (!PyArg_ParseTuple(args, "Osss|p", &py_list, &play_dir, &strain,
        &trick_so_far, &want_win_ranks))
    {
        return NULL;
    }

    CurrentTrick ct;
    PyObject* py_err = ct.from_string(trick_so_far);
    if (py_err != Py_None)
	return py_err;

    int play_dir_id = string_to_dir(play_dir);
    if (play_dir_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad play direction '%s'", play_dir);

    int strain_id = string_to_strain(strain);
    if (strain_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad strain '%s'", strain);

    PyObject* py_iter = PyObject_GetIter(py_list);
    if (py_iter == NULL)
	return NULL;

    PyObject* out_list = PyList_New(0);
    if (out_list == NULL)
	return NULL;

    while (true)
    {
	struct boardsPBN boards;
	PyObject* err = NULL;
	int load_result = load_boards_pbn(py_iter, boards, ct.suit,
	    ct.rank, strain_id, (play_dir_id + (4 - ct.nonZero)) % 4, &err);
	if (load_result < 0) {
	    Py_DECREF(py_iter);
	    Py_DECREF(out_list);
	    return err;
	}
	if (boards.noOfBoards == 0)
	    break;

	struct solvedBoards sb;
	int ret = SolveAllBoards(&boards, &sb);
	if (ret < 0) {
	    Py_DECREF(py_iter);
	    Py_DECREF(out_list);
	    return dds_error(ret);
	}

	/// Append the result
	for (int i=0 ; i<sb.noOfBoards ; i++)
	{
	    int num_cards = 0;
	    int num_card_classes = sb.solvedBoard[i].cards;
	    for (int cc=0 ; cc<num_card_classes ; cc++)
		num_cards += 1 + bitcount(sb.solvedBoard[i].equals[cc]);

	    PyObject* board_list = PyList_New(num_cards);
	    if (board_list == NULL) {
		Py_DECREF(py_iter);
		Py_DECREF(out_list);
		return NULL;
	    }
	    int ci = 0;
	    for (int cc=0 ; cc<num_card_classes ; cc++) {
		char card[3];
		char wr_string[5];
		suit_rank_str(sb.solvedBoard[i].suit[cc],
		    sb.solvedBoard[i].rank[cc], card);
		int tricks = sb.solvedBoard[i].score[cc];
		if (want_win_ranks)
		    set_win_rank_string(wr_string, sb.solvedBoard[i].winRanks[cc]);
		PyObject* py_score = want_win_ranks ?
		    Py_BuildValue("sis", card, tricks, wr_string) :
		    Py_BuildValue("si", card, tricks);
		if (py_score == NULL) {
		    Py_DECREF(board_list);
		    Py_DECREF(py_iter);
		    Py_DECREF(out_list);
		    return NULL;
		}
		assert(ci < num_cards);
		PyList_SET_ITEM(board_list, ci, py_score);
		ci += 1;

		for (int r=0 ; r<13 ; r++) {
		    if ((4<<r) & sb.solvedBoard[i].equals[cc]) {
			suit_rank_str(sb.solvedBoard[i].suit[cc], r+2, card);
			py_score = want_win_ranks ?
			    Py_BuildValue("sis", card, tricks, wr_string) :
			    Py_BuildValue("si", card, tricks);
			if (py_score == NULL) {
			    Py_DECREF(board_list);
			    Py_DECREF(py_iter);
			    Py_DECREF(out_list);
			    return NULL;
			}
			assert(ci < num_cards);
			PyList_SET_ITEM(board_list, ci, py_score);
			ci += 1;
		    }
		}
	    }
	    if (ci != num_cards) {
		printf("ci=%d num_cards=%d\n", ci, num_cards);
		Py_DECREF(board_list);
		Py_DECREF(out_list);
		Py_DECREF(py_iter);
		RETURN_ASSERT;
	    }

	    if (PyList_Append(out_list, board_list) < 0) {
		Py_DECREF(out_list);
		Py_DECREF(py_iter);
		return NULL;
	    }
	}

	if (load_result == 1)
	    break;
    }
    return out_list;
}

static PyObject*
dds_analyze_deal_play(PyObject* self, PyObject* args)
{
    PyObject* py_deal;
    const char* declarer_dir;
    const char* strain;
    const char* history;
    if (!PyArg_ParseTuple(args, "O!sss", _deal_type, &py_deal,
        &declarer_dir, &strain, &history))
    //
        return NULL;

    struct deal the_deal;
    python_objects_to_deal(the_deal, py_deal, declarer_dir, strain);

    int historyLen = strlen(history);
    if (historyLen > 104)
        return PyErr_Format(PyExc_ValueError, "Play history too long");
    if (historyLen % 2 != 0)
        return PyErr_Format(PyExc_ValueError, "Play history odd length");

    int player_id = the_deal.first;
    int goods[52], bads[52];
    bool was_good[52];

    for (int i=0 ; i<historyLen ; i+=2)
    {
        struct futureTricks futs;
        int ret = SolveBoard(the_deal, -1, 3, 0, &futs, 0);
        if (ret < 0)
            return dds_error(ret);

        int best_score = futs.score[0];
        int played_suit = char_to_suit(history[i]);
        if (played_suit == -1)
            return PyErr_Format(PyExc_ValueError, "Bad suit '%c' at position %d of history", history[i], i);
        int played_rank = char_to_rank_number(history[i+1]);
        if (played_rank == -1)
            return PyErr_Format(PyExc_ValueError, "Bad rank '%c' at position %d of history", history[i+1], i+1);

        if ((the_deal.remainCards[player_id][played_suit] & (1 << played_rank)) == 0)
            return PyErr_Format(PyExc_ValueError,
                "Card '%c%c' at position %d not in player %c's hand",
                history[i], history[i+1], i, DIRS[player_id]);

        bool found_played = false;
        goods[i/2] = 0;
        bads[i/2] = 0;
        for (int cc=0 ; cc<futs.cards ; cc++)
        {
            bool this_is_played = false;
            if (played_suit == futs.suit[cc]) {
                if (played_rank == futs.rank[cc] ||
                    ((1<<played_rank) & futs.equals[cc]) != 0)
                {
                    if (found_played) 
                        RETURN_ASSERT;
                    this_is_played = true;
                    found_played = true;
                }
            }

            int n = 1 + bitcount(futs.equals[cc]);
            if (futs.score[cc] == best_score) {
                goods[i/2] += n;
                if (this_is_played)
                    was_good[i/2] = true;
            } else if (futs.score[cc] < best_score) {
                bads[i/2] += n;
                if (this_is_played)
                    was_good[i/2] = false;
            } else {
                RETURN_ASSERT;
            }
        }
        if (!found_played)
            RETURN_ASSERT;

        /// Prepare the board for the next trick!
        the_deal.remainCards[player_id][played_suit] &= ~(1<<played_rank);

        int cit = (i%8) / 2 + 1;
        if (cit < 4) {
            the_deal.currentTrickSuit[cit-1] = played_suit;
            the_deal.currentTrickRank[cit-1] = played_rank;
            player_id = (player_id+1)%4;
        } else {
            int i0 = i - 6;
            int best_j = 0;
            int best_suit = char_to_suit(history[i0]);
            int best_rank = char_to_rank_number(history[i0+1]);
            for (int j=1 ; j<4 ; j++) {
                int suit = char_to_suit(history[i0+j*2]);
                int rank = char_to_rank_number(history[i0+j*2+1]);

                if (suit == best_suit) {
                    if (rank > best_rank) {
                        best_j = j;
                        best_rank = rank;
                    }
                } else if (suit == the_deal.trump) {
                    best_j = j;
                    best_suit = suit;
                    best_rank = rank;
                }
            }

            the_deal.first = (the_deal.first + best_j) % 4;
            player_id = the_deal.first;
            for (int j=0 ; j<3 ; j++) {
                the_deal.currentTrickSuit[j] = 0;
                the_deal.currentTrickRank[j] = 0;
            }
        }
    }

    // Analysis done!  Prepare the result
    PyObject* out_list = PyList_New(historyLen/2);
    if (out_list == NULL)
        return NULL;

    for (int i=0 ; i<historyLen/2 ; i++) {
        PyObject* py_tupe = Py_BuildValue("iiO", goods[i], bads[i],
            was_good[i] ? Py_True : Py_False);

        if (py_tupe == NULL) {
            Py_DECREF(out_list);
            return NULL;
        }
        PyList_SET_ITEM(out_list, i, py_tupe);
    }

    return out_list;
}


static PyObject*
dds_play_menu(PyObject* self, PyObject* args)
{
    PyObject* py_tuple;
    const char* play_dir;
    const char* strain;
    const char* trick_so_far;

    if (!PyArg_ParseTuple(args, "Osss", &py_tuple, &play_dir, &strain,
	&trick_so_far))
    {
	return NULL;
    }

    CurrentTrick ct;
    PyObject* py_err = ct.from_string(trick_so_far);
    if (py_err != Py_None)
	return py_err;

    int play_dir_id = string_to_dir(play_dir);
    if (play_dir_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad play direction '%s'", play_dir);

    int strain_id = string_to_strain(strain);
    if (strain_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad strain '%s'", strain);

    struct dealPBN board;
    py_err = load_one_board_pbn(board, py_tuple, ct.suit, ct.rank,
	strain_id, (play_dir_id + (4 - ct.nonZero)) % 4);
    if (py_err != Py_None)
	return py_err;

    struct futureTricks ft;
    int ret = SolveBoardPBN(board, 0, 2, 0, &ft, 0);
    if (ret < 0)
	return dds_error(ret);

    PyObject* py_out_list = PyList_New(ft.cards);
    for (int i=0 ; i<ft.cards ; i++) {
	PyObject* py_equals_list = PyList_New(1 + bitcount(ft.equals[i]));
	char card[3];
	suit_rank_str(ft.suit[i], ft.rank[i], card);
	PyList_SET_ITEM(py_equals_list, 0, Py_BuildValue("s", card));
	int j = 1;
	for (int r=0 ; r<13; r++) {
	    if ((4<<r) & ft.equals[i]) {
		suit_rank_str(ft.suit[i], r+2, card);
		PyList_SET_ITEM(py_equals_list, j, Py_BuildValue("s", card));
		j++;
	    }
	}
	    

	PyList_SET_ITEM(py_out_list, i, py_equals_list);
    }

    return py_out_list;
}


const char* solve_deal_desc =
"Solve a single deal\n"
"Takes three parameters:\n"
"   1. A bridgemoose.Deal object\n"
"   2. Declarer (string 'W','N','E', or 'S')\n"
"   3. Strain (string 'C','D','H','S', or 'N')\n"
"Returns an integer number of tricks\n";

const char* solve_many_deals_desc =
"Solve many deals\n"
"Takes one parameter, an iterable of 3-tuples.  Each tuple is:\n"
"   1. A bridgemoose.Deal object\n"
"   2. Declarer (string 'W','N','E', or 'S')\n"
"   3. Strain (string 'C','D','H','S', or 'N')\n"
"Returns a list of integer number of tricks\n";

const char* solve_many_plays_desc =
"Solve many plays\n"
"Takes four or five paramters:\n"
"   1. A list of 4-tuples, where each item is a (partial) hand in string\n"
"      format, starting with West.\n"
"   2. The direction of the player on play ('W','N','E', or 'S')\n"
"   3. Strain ('C','D','H','S', or 'N')\n"
"   4. Trick so far; a string like 'C5CT' of up to 3 cards\n"
"   5. (optional) True if win ranks are requested.\n"
"Output is a list (one per deal) of a list (one per card) of 2-tuples\n"
"   in (card, tricks) format, such as ('C5', 3)\n"
"   tricks are counted from the PoV of the player on play\n"
"   If win ranks are requested, the tuples are (card, tricks, string)\n"
"   Where string returns one below the lowest win rank for each suit\n"
"   In SHDC order.  If there are no win ranks, 'A' is returned.\n";

const char* analyze_deal_play_desc =
"Analayze plays from a single deal\n"
"Takes four parameters:\n"
"   1. A bridgemoose.Deal object\n"
"   2. Declarer (string 'W','N','E', or 'S')\n"
"   3. Strain (string 'C','D','H','S', or 'N')\n"
"   4. The play history as a compressed string format, example:\n"
"      'S4SJSQSAC4D2CKC5'\n"
"\n"
"Returns a list of three-tuples, one for each play in parameter 4\n"
"of the format (num_good_moves, num_bad_moves, was_good)\n"
"of type (int, int, bool)\n";


const char* play_menu_desc =
"Play Menu\n"
"Takes four parameters:\n"
"   1. A single 4-tuple, where each item is a (partial) hand in string\n"
"      format, starting with West.\n"
"   2. The direction of the player on play ('W','N','E', or 'S')\n"
"   3. Strain ('C','D','H','S', or 'N')\n"
"   4. Trick so far; a string like 'C5CT' of up to 3 cards\n"
"\n"
"Output list of tuples of legal moves.\n"
"Moves in the same tuple are equivalent.\n";


static PyMethodDef DdsMethods[] = {
    {"solve_deal", dds_solve_deal, METH_VARARGS, solve_deal_desc},
    {"solve_many_deals", dds_solve_many_deals, METH_VARARGS, solve_many_deals_desc},
    {"solve_many_plays", dds_solve_many_plays, METH_VARARGS, solve_many_plays_desc},
    {"analyze_deal_play", dds_analyze_deal_play, METH_VARARGS, analyze_deal_play_desc},
    {"play_menu", dds_play_menu, METH_VARARGS, play_menu_desc},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef ddsmodule = {
    PyModuleDef_HEAD_INIT,
    "bridgemoose.dds",
    NULL,
    -1,
    DdsMethods
};

PyMODINIT_FUNC
PyInit_dds(void)
{
    PyObject* mod = PyImport_AddModule("bridgemoose");
    if (mod == NULL)
        return NULL;
    _deal_type = PyObject_GetAttrString(mod, "Deal");
    if (_deal_type == NULL)
        return NULL;
    _hand_type = PyObject_GetAttrString(mod, "Hand");
    if (_hand_type == NULL)
        return NULL;

    _dds_c_api.pErrorMessage = ErrorMessage;
    _dds_c_api.pSolveAllBoardsBin = SolveAllBoardsBin;

    SetMaxThreads(1);

    PyObject* dds_mod = PyModule_Create(&ddsmodule);
    if (dds_mod == NULL)
	return NULL;

    PyObject* c_api_obj = PyCapsule_New((void*)&_dds_c_api, "bridgemoose.dds._C_API", NULL);
    if (PyModule_AddObject(dds_mod, "_C_API", c_api_obj) < 0) {
	Py_XDECREF(c_api_obj);
	Py_DECREF(dds_mod);
	return NULL;
    }

    return dds_mod;
}
