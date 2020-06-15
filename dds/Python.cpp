#include <Python.h>
#include "dll.h"

static PyObject* _deal_type = NULL;
static PyObject* _hand_type = NULL;

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


static int char_to_rank(char c)
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
    defaukt:
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
                int r = char_to_rank(*pyu);
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
dds_solve_many_deals(PyObject* self, PyObject* args)
{
    struct boards boards;
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list))
        return NULL;

    PyObject* iter = PyObject_GetIter(py_list);
    if (iter == NULL)
        return NULL;

    int num_deals = 0;
    PyObject* py_deal;
    while ((py_deal = PyIter_Next(iter))) {
        if (num_deals >= MAXNOOFBOARDS) {
            Py_DECREF(py_deal);
            return PyErr_Format(PyExc_IndexError,
                "At most %d deals at once", MAXNOOFBOARDS);
        }

        PyObject* py_ret = python_tuple_to_deal(boards.deals[num_deals], py_deal);
        if (py_ret != Py_None)
            return py_ret;

        boards.target[num_deals] = -1;
        boards.solutions[num_deals] = 1;
        boards.mode[num_deals] = 0;
        num_deals++;
        Py_DECREF(py_deal);
    }

    struct solvedBoards solves;
    boards.noOfBoards = num_deals;
    int ret = SolveAllChunksBin(&boards, &solves, 1);
    if (ret < 0)
        return dds_error(ret);

    PyObject* out_list = PyList_New(solves.noOfBoards);
    for (int i=0 ; i<solves.noOfBoards ; i++) {
        PyObject* val = PyLong_FromLong(13 - solves.solvedBoard[i].score[0]);
        if (val == NULL) {
            Py_DECREF(out_list);
            return NULL;
        }
        PyList_SET_ITEM(out_list, i, val);
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

// Returns Py_None if okay, else an error of some sort to be passed along
static PyObject*
load_boards_pbn(PyObject* py_deal_list, struct boardsPBN& boards,
    int count_by_player[MAXNOOFBOARDS][4], const int ctSuit[3],
    const int ctRank[3], int strain_id, int first_dir_id)
{
    int num_deals = 0;

    PyObject* py_iter = PyObject_GetIter(py_deal_list);
    if (py_iter == NULL)
        return NULL;

    PyObject* py_deal;
    while ((py_deal = PyIter_Next(py_iter)))
    {
        if (num_deals >= MAXNOOFBOARDS) {
            Py_DECREF(py_deal);
            Py_DECREF(py_iter);
            return PyErr_Format(PyExc_IndexError,
                "At most %d deals at once", MAXNOOFBOARDS);
        }

        const char* h[4];

        if (!PyArg_ParseTuple(py_deal, "ssss", &h[0], &h[1], &h[2], &h[3]))
        {
            Py_DECREF(py_deal);
            Py_DECREF(py_iter);
            return NULL;
        }

        boards.target[num_deals] = -1;
        boards.solutions[num_deals] = 3;
        boards.mode[num_deals] = 0;

        boards.deals[num_deals].trump = strain_id;
        boards.deals[num_deals].first = first_dir_id; 
        for (int i=0 ; i<3 ; i++) {
            boards.deals[num_deals].currentTrickSuit[i] = ctSuit[i];
            boards.deals[num_deals].currentTrickRank[i] = ctRank[i];
        }

        int i = 0;
        boards.deals[num_deals].remainCards[i++] = 'N';
        boards.deals[num_deals].remainCards[i++] = ':';

        for (int j=0 ; j<4 ; j++) {
            int count = 0;
	    if (j != 0) {
		if (i >= 78) {
                    Py_DECREF(py_deal);
                    Py_DECREF(py_iter);
                    return PyErr_Format(PyExc_ValueError,
                        "Too many cards specified");
                }
		boards.deals[num_deals].remainCards[i++] = ' ';
	    }

            count_by_player[num_deals][j] = 0;
            for (const char* cp = h[j] ; *cp ; cp++) {
                if (i >= 78) {
                    Py_DECREF(py_deal);
                    return PyErr_Format(PyExc_ValueError,
                        "Too many cards specified");
                }

                if (*cp == '-')
                    continue;
                else if (*cp == '/')
                    boards.deals[num_deals].remainCards[i++] = '.';
                else {
                    count_by_player[num_deals][j] += 1;
                    boards.deals[num_deals].remainCards[i++] = *cp;
                }
            }
        }
	boards.deals[num_deals].remainCards[i] = 0;

        num_deals += 1;
        Py_DECREF(py_deal);
    }
    boards.noOfBoards = num_deals;

    Py_DECREF(py_iter);
    return Py_None;
}


static PyObject*
dds_solve_many_plays(PyObject* self, PyObject* args)
{
    PyObject* py_list;
    const char* play_dir;
    const char* strain;
    const char* trick_so_far;
    if (!PyArg_ParseTuple(args, "Osss", &py_list, &play_dir, &strain,
        &trick_so_far))
    {
        return NULL;
    }

    int ctSuit[3];
    int ctRank[3];
    int ctNonZero = 0;
    for (int i=0 ; i<3 ; i++) {
        ctSuit[i] = 0;
        ctRank[i] = 0;
    }

    for (int i=0 ; i<3 ; i++) {
        if (trick_so_far[i*2] == '\0')
            break;
        ctSuit[i] = char_to_suit(trick_so_far[i*2]);
        if (ctSuit[i] == -1)
            return PyErr_Format(PyExc_ValueError, "Bad suit in '%s'", trick_so_far);
        ctRank[i] = char_to_rank(trick_so_far[i*2+1]);
        if (ctRank[i] < 0)
            return PyErr_Format(PyExc_ValueError, "Bad rank in '%s'", trick_so_far);
        ctNonZero += 1;
    }

    int play_dir_id = string_to_dir(play_dir);
    if (play_dir_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad play direction '%s'", play_dir);

    int strain_id = string_to_strain(strain);
    if (strain_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad strain '%s'", strain);

    struct boardsPBN boards;
    int count_by_player[MAXNOOFBOARDS][4];
    PyObject* r = load_boards_pbn(py_list, boards, count_by_player, ctSuit,
        ctRank, strain_id, (play_dir_id + (4 - ctNonZero)) % 4);
    if (r != Py_None)
        return r;

    struct solvedBoards sb;
    int ret = SolveAllBoards(&boards, &sb);
    if (ret < 0)
        return dds_error(ret);

    /// Construct the result
    PyObject* out_list = PyList_New(sb.noOfBoards);
    if (out_list == NULL)
        return NULL;

    for (int i=0 ; i<sb.noOfBoards ; i++) {
        int num_cards = count_by_player[i][boards.deals[i].first];
        int num_card_classes = sb.solvedBoard[i].cards;

        PyObject* board_list = PyList_New(num_cards);
        if (board_list == NULL) {
            Py_DECREF(out_list);
            return NULL;
        }
        int ci = 0;
        for (int cc=0 ; cc<num_card_classes ; cc++) {
            char card[3];
            suit_rank_str(sb.solvedBoard[i].suit[cc],
                sb.solvedBoard[i].rank[cc], card);
            int tricks = sb.solvedBoard[i].score[cc];
            PyObject* py_score = Py_BuildValue("si", card, tricks);
            if (py_score == NULL) {
                Py_DECREF(board_list);
                Py_DECREF(out_list);
                return NULL;
            }
            assert(ci < num_cards);
            PyList_SET_ITEM(board_list, ci, py_score);
            ci += 1;

            for (int r=0 ; r<13 ; r++) {
                if ((4<<r) & sb.solvedBoard[i].equals[cc]) {
                    suit_rank_str(sb.solvedBoard[i].suit[cc], r+2, card);
                    py_score = Py_BuildValue("si", card, tricks);
                    if (py_score == NULL) {
                        Py_DECREF(board_list);
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
            Py_DECREF(board_list);
            Py_DECREF(out_list);
            return PyErr_Format(PyExc_RuntimeError, "Internal Error 1");
        }
        PyList_SET_ITEM(out_list, i, board_list);
    }
    return out_list;
}

static PyObject*
dds_analyze_many_plays(PyObject* self, PyObject* args)
{
    PyObject* py_list;
    const char* declarer_dir;
    const char* strain;
    const char* history;
    if (!PyArg_ParseTuple(args, "Osss", &py_list, &declarer_dir, &strain,
        &history))
    //
        return NULL;

    int historyLen = strlen(history);
    if (historyLen > 104)
        return PyErr_Format(PyExc_ValueError, "Play history too long");
    if (historyLen % 2 != 0)
        return PyErr_Format(PyExc_ValueError, "Play history odd length");

    int ctSuit[3];
    int ctRank[3];
    for (int i=0 ; i<3 ; i++) {
        ctSuit[i] = 0;
        ctRank[i] = 0;
    }

    int declarer_dir_id = string_to_dir(declarer_dir);
    if (declarer_dir_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad declarer '%s'", declarer_dir);

    int strain_id = string_to_strain(strain);
    if (strain_id == -1)
        return PyErr_Format(PyExc_ValueError, "Bad strain '%s'", strain);

    struct boardsPBN boards;
    int count_by_player[MAXNOOFBOARDS][4];

    PyObject* r = load_boards_pbn(py_list, boards,
        count_by_player, ctSuit, ctRank, strain_id, (declarer_dir_id+1)%4);
    if (r != Py_None)
        return r;

    struct playTracesPBN traces;
    traces.noOfBoards = boards.noOfBoards;
    for (int i=0 ; i<boards.noOfBoards ; i++) {
        traces.plays[i].number = historyLen / 2;
        strcpy(traces.plays[i].cards, history);
    }

    struct solvedPlays solved;

    int ret = AnalyseAllPlaysPBN(&boards, &traces, &solved, 1);
    if (ret < 0)
        return dds_error(ret);

    /// Construct the result
    PyObject* out_list = PyList_New(solved.noOfBoards);
    if (out_list == NULL)
        return NULL;
        
    for (int i=0 ; i<solved.noOfBoards ; i++) {
        PyObject* board_list = PyList_New(solved.solved[i].number);
        if (board_list == NULL) {
            Py_DECREF(out_list);
            return NULL;
        }
        for (int j=0 ; j<solved.solved[i].number ; j++) {
            PyObject* py_long = PyLong_FromLong(solved.solved[i].tricks[j]);
            if (py_long == NULL) {
                Py_DECREF(board_list);
                Py_DECREF(out_list);
                return NULL;
            }
            PyList_SET_ITEM(board_list, j, py_long);
        }
        PyList_SET_ITEM(out_list, i, board_list);
    }
    return out_list;
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
"Takes four parameters:\n"
"   1. A list of 4-tuples, where each item is a (partial) hand in string\n"
"      format, starting with West.\n"
"   2. The direction of the player on play ('W','N','E', or 'S')\n"
"   3. Strain ('C','D','H','S', or 'N')\n"
"   4. Trick so far; a string like 'C5CT' of up to 3 cards\n"
"Output is a list (one per deal) of a list (one per card) of 2-tuples\n"
"   in (card, tricks) format, such as ('C5', 3)\n"
"   tricks are counted from the PoV of the player on play\n";

const char* analyze_many_plays_desc =
"Analyze many plays\n"
"Takes four parameters:\n"
"   1. A list of 4-tuples, where each item is hand in string\n"
"      format, starting with West.\n"
"   2. The direction of the declarer\n"
"   3. Strain ('C',etc)\n"
"   4. The common play history as a compressed string format, example:\n"
"      'S4SJSQSAC4D2CKC5'\n"
"\n"
"Output is a list (one per deal) or a list (one per card) of integers\n"
"   representing DD tricks (declarer's PoV).  This list is one longer\n"
"   than the number of cards in parameter 4";

static PyMethodDef DdsMethods[] = {
    {"solve_deal", dds_solve_deal, METH_VARARGS, solve_deal_desc},
    {"solve_many_deals", dds_solve_many_deals, METH_VARARGS, solve_many_deals_desc},
    {"solve_many_plays", dds_solve_many_plays, METH_VARARGS, solve_many_plays_desc},
    {"analyze_many_plays", dds_analyze_many_plays, METH_VARARGS, analyze_many_plays_desc},
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

    SetMaxThreads(1);

    return PyModule_Create(&ddsmodule);
}
