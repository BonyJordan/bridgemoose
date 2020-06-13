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
    dl.first = dec_dir_id;       // person on lead. by lucky coincidence
                                // equal to our encoding for declarer!
    for (int i=0 ; i<3 ; i++) {
        dl.currentTrickSuit[i] = 0;
        dl.currentTrickRank[i] = 0;
    }

    // Load the cards from py_deal to dl.
    static const char* dds_dirs[] = {"N", "E", "S", "W"};
    static const char* dds_suits[] = {"S", "H", "D", "C"};

    for (int hid=0 ; hid<4 ; hid++)
    {
        PyObject* hand = PyObject_GetAttrString(py_deal, dds_dirs[hid]);
        if (hand == NULL)
            return NULL;

        for (int sid=0 ; sid<4 ; sid++)
        {
            dl.remainCards[hid][sid] = 0;
            PyObject* ranks = PyObject_GetAttrString(hand, dds_suits[sid]);
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
            static const char* rchars = "23456789TJQKA";

            while (*pyu) {
                int r;
                for (r=0 ; rchars[r] != *pyu && r<13 ; r++)
                    ;
                if (r == 13) {
                    Py_DECREF(ranks);
                    Py_DECREF(hand);
                    return PyErr_Format(PyExc_ValueError, "Bad rank '%c'",
                        *pyu);
                }
                dl.remainCards[hid][sid] |= (4 << r);
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


static PyMethodDef DdsMethods[] = {
    {"solve_deal", dds_solve_deal, METH_VARARGS, "Solve a deal"},
    {"solve_many_deals", dds_solve_many_deals, METH_VARARGS, "Solve many deals"},
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
