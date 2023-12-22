#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <inttypes.h>
#include "solver.h"


PyObject* _hand_type = NULL;
PyObject* _partial_hand_type = NULL;

const char* SUITS = "SHDC";
const char* RANKS = "23456789TJQKA";

static
int char_to_rank(char c)
{
    for (int i=0 ; i<13 ; i++)
	if (RANKS[i] == c)
	    return i;
    return -1;
}

static
int char_to_trump(char t)
{
    switch (t) {
	case 'N':
	case 'n':
	    return 4;

	case 'S':
	case 's':
	    return 3;

	case 'H':
	case 'h':
	    return 2;

	case 'D':
	case 'd':
	    return 1;

	case 'C':
	case 'c':
	    return 0;

	default:
	    PyErr_Format(PyExc_ValueError, "Bad trump specification '%c'", t);
	    return -1;
    }
}


static
bool hand_from_pystring(PyObject* pyo, hand64_t& hand)
{
    hand = 0;
    const char* utf = PyUnicode_AsUTF8(pyo);
    const char* s = utf;

    int suit = 0;
    for ( ; *s ; s++)
    {
	if (*s == '/') {
	    suit++;
	    if (suit >= 4) {
		PyErr_Format(PyExc_ValueError, "Too many suits '%s'", utf);
		return false;
	    }
	    hand <<= 16;
	    continue;
	} else if (*s == '-') {
	    // quietly ignore
	    continue;
	}
	int r = char_to_rank(*s);
	if (r < 0) {
	    PyErr_Format(PyExc_ValueError, "Bad card '%c'", *s);
	    return false;
	} else {
	    hand |= (4<<r);
	}
    }
    if (suit != 3) {
	PyErr_Format(PyExc_ValueError, "Too few suits '%s'", utf);
	return false;
    }

    return true;
}


static
bool hand_from_pyo(PyObject* pyo, hand64_t& hand)
{
    if (PyUnicode_Check(pyo))
	return hand_from_pystring(pyo, hand);
    else {
	PyObject* pys = PyObject_Str(pyo);
	if (pys == NULL)
	    return false;
	bool r = hand_from_pystring(pys, hand);
	Py_DECREF(pys);
	return r;
    }
}


typedef struct {
    PyObject_HEAD
    SOLVER* solver;
} SolverObject;

static PyObject*
Solver_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    SolverObject* self = (SolverObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
	self->solver = NULL;
    }
    return (PyObject*) self;
}

static int
Solver_init(SolverObject* self, PyObject* args, PyObject* kwds)
{
    const char* keywords[] = {
	"north",
	"south",
	"trump",
	"target",
	"ew",
	NULL
    };
    PyObject* north_obj = NULL;
    PyObject* south_obj = NULL;
    int trump_char = 0;
    int target = 0;
    PyObject* ew_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOCiO", (char**)keywords,
	&north_obj, &south_obj, &trump_char, &target, &ew_obj))
    {
	return -1;
    }

    hand64_t north, south;
    if (!hand_from_pyo(north_obj, north))
	return -1;
    if (!hand_from_pyo(south_obj, south))
	return -1;

    if ((north & south) != 0) {
	PyErr_Format(PyExc_ValueError, "North/South use same cards");
	return -1;
    }
    if (handbits_count(north) != handbits_count(south)) {
	PyErr_Format(PyExc_ValueError, "North/South different card counts");
	return -1;
    }
    int trump = char_to_trump(trump_char);
    if (trump < 0) {
	return -1;
    }
    if (target < 1 || target > 13) {
	PyErr_Format(PyExc_ValueError, "Weird target '%d'", target);
	return -1;
    }

    std::vector<hand64_t> easts;
    std::vector<hand64_t> wests;

    PyObject* iter = PyObject_GetIter(ew_obj);
    if (iter == NULL) {
	return -1;
    }
    for (int index=0 ; true ; index++)
    {
	PyObject* tuple = PyIter_Next(iter);
	if (tuple == NULL)
	    break;

	PyObject* eobj;
	PyObject* wobj;

	if (!PyArg_ParseTuple(tuple, "OO", &eobj, &wobj))
	    return -1;

	hand64_t whand, ehand;
	if (!hand_from_pyo(wobj, whand))
	    return -1;
	if (!hand_from_pyo(eobj, ehand))
	    return -1;

	if (handbits_count(whand) != handbits_count(north)) {
	    PyErr_Format(PyExc_ValueError, "West[%d] wrong number of cards",
		index);
	    return -1;
	}
	if (handbits_count(ehand) != handbits_count(north)) {
	    PyErr_Format(PyExc_ValueError, "East[%d] wrong number of cards",
		index);
	    return -1;
	}
	if ((whand & north) != 0) {
	    PyErr_Format(PyExc_ValueError, "North/West[%d] use same cards",
		index);
	    return -1;
	}
	if ((whand & south) != 0) {
	    PyErr_Format(PyExc_ValueError, "South/West[%d] use same cards",
		index);
	    return -1;
	}
	if ((ehand & north) != 0) {
	    PyErr_Format(PyExc_ValueError, "North/East[%d] use same cards",
		index);
	    return -1;
	}
	if ((ehand & south) != 0) {
	    PyErr_Format(PyExc_ValueError, "South/East[%d] use same cards",
		index);
	    return -1;
	}

	wests.push_back(whand);
	easts.push_back(ehand);
    }

    self->solver = new SOLVER(north, south, trump, target, wests, easts);
    printf("Created with %zu pairs\n", wests.size());
    return 0;
}


static PyTypeObject SolverType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "bridgemoose.jade.Solver",
    .tp_basicsize = sizeof(SolverObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = PyDoc_STR("Jordan's Amazing Declarer Evaluater - Solver Object"),
    .tp_init = (initproc) Solver_init,
    .tp_new = Solver_new,
};

static PyModuleDef jade_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "bridgemoose.jade",
    .m_doc = "Jordan's Amazing Declarer Evaluater",
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit_jade(void)
{
    PyObject* bm_mod = PyImport_AddModule("bridgemoose");
    if (bm_mod == NULL)
	return NULL;
    _hand_type = PyObject_GetAttrString(bm_mod, "Hand");
    if (_hand_type == NULL)
	return NULL;
    _partial_hand_type = PyObject_GetAttrString(bm_mod, "PartialHand");
    if (_partial_hand_type == NULL)
	return NULL;

    /////

    PyObject* mod;
    if (PyType_Ready(&SolverType) < 0)
	return NULL;

    mod = PyModule_Create(&jade_module);
    if (mod == NULL)
	return NULL;

    Py_INCREF(&SolverType);
    if (PyModule_AddObject(mod, "Solver", (PyObject*)&SolverType) < 0) {
	Py_DECREF(&SolverType);
	Py_DECREF(mod);
	return NULL;
    }

    return mod;
}
