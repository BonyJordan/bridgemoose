#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <inttypes.h>
#include "ansolver.h"
#include "solver.h"


PyObject* _hand_type = NULL;
PyObject* _partial_hand_type = NULL;
PyObject* _jbdd_type = NULL;

const char* SUITS = "CDHS";
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
int char_to_suit(char c)
{
    for (int i=0 ; i<4 ; i++)
	if (SUITS[i] == c)
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
    PROBLEM problem;
} Solver_Object;

typedef struct {
    PyObject_HEAD
    ANSOLVER* ansolver;
    PROBLEM problem;
} ANSolver_Object;


static PyObject*
bdt_to_py_list_of_cubes(BDT bdt)
{
    std::vector<INTSET> cubes = bdt.get_cubes();
    PyObject* outer = PyList_New(cubes.size());
    if (outer == NULL)
	return NULL;

    std::vector<INTSET>::const_iterator itr;
    Py_ssize_t i, j;
    for (i=0,itr = cubes.begin() ; itr != cubes.end() ; itr++,i++)
    {
	PyObject* inner = PyList_New(itr->size());
	if (inner == NULL) {
	    Py_DECREF(outer);
	    return NULL;
	}
	j = 0;
	for (INTSET_ITR jtr(*itr) ; jtr.more() ; jtr.next(),j++) {
	    PyObject* num = Py_BuildValue("i", jtr.current());
	    if (num == NULL) {
		Py_DECREF(inner);
		Py_DECREF(outer);
		return NULL;
	    }
	    PyList_SET_ITEM(inner, j, num);
	}
	PyList_SET_ITEM(outer, i, inner);
    }
    return outer;
}


static PyObject*
Solver_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    Solver_Object* self = (Solver_Object*) type->tp_alloc(type, 0);
    if (self != NULL) {
	self->solver = NULL;
    }
    return (PyObject*) self;
}

static PyObject*
ANSolver_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    ANSolver_Object* self = (ANSolver_Object*) type->tp_alloc(type, 0);
    if (self != NULL) {
	self->ansolver = NULL;
    }
    return (PyObject*) self;
}


static void
Solver_dealloc(Solver_Object* self)
{
    if (self->solver != NULL) {
	delete self->solver;
	self->solver = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static void
ANSolver_dealloc(ANSolver_Object* self)
{
    if (self->ansolver != NULL) {
	delete self->ansolver;
	self->ansolver = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static int
pyargs_to_problem(PROBLEM& problem, PyObject* args, PyObject* kwds)
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
    PyObject* we_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOCiO", (char**)keywords,
	&north_obj, &south_obj, &trump_char, &target, &we_obj))
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

    PyObject* iter = PyObject_GetIter(we_obj);
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

	if (!PyArg_ParseTuple(tuple, "OO", &wobj, &eobj))
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

    problem.north = north;
    problem.south = south;
    problem.trump = trump;
    problem.target = target;
    problem.wests = wests;
    problem.easts = easts;
    return 0;
}


static int
Solver_init(Solver_Object* self, PyObject* args, PyObject* kwds)
{
    if (pyargs_to_problem(self->problem, args, kwds) < 0)
	return -1;

    self->solver = new SOLVER(self->problem);
    return 0;
}


static int
ANSolver_init(ANSolver_Object* self, PyObject* args, PyObject* kwds)
{
    if (pyargs_to_problem(self->problem, args, kwds) < 0)
	return -1;

    self->ansolver = new ANSOLVER(self->problem);
    return 0;
}


static bool
pylist_to_intlist(INTSET& intlist, PyObject* pylist)
{
    PyObject* iter = PyObject_GetIter(pylist);
    if (iter == NULL)
	return false;

    PyObject* o;
    while ((o = PyIter_Next(iter)) != NULL) {
	long n = PyLong_AsLong(o);
	if (n == -1 && PyErr_Occurred())
	    return false;
	intlist.insert(n);
    }
    return true;
}


static bool
pylist_to_cardlist(std::vector<CARD>& plays, PyObject* play_list)
{
    PyObject* iter = PyObject_GetIter(play_list);
    if (iter == NULL)
	return false;

    for (int i=0 ; ; i++)

    {
	PyObject* py_card = PyIter_Next(iter);
	if (py_card == NULL)
	    break;

	PyObject* py_str = PyObject_Str(py_card);
	if (py_str == NULL) {
	    Py_DECREF(iter);
	    return false;
	}
	const char* card_str = PyUnicode_AsUTF8(py_str);
	if (card_str == NULL) {
	    Py_DECREF(py_str);
	    Py_DECREF(iter);
	    return false;
	}

	if (!card_str[0] || !card_str[1]) {
	    Py_DECREF(py_str);
	    Py_DECREF(iter);
	    PyErr_Format(PyExc_ValueError, "Malformed Card index %d", i);
	    return false;
	}
	    
	int suit_i = char_to_suit(card_str[0]);
	if (suit_i < 0) {
	    Py_DECREF(py_str);
	    Py_DECREF(iter);
	    PyErr_Format(PyExc_ValueError, "Bad suit '%c' at index %d",
		card_str[0], i);
	    return false;
	}
	int rank_i = char_to_rank(card_str[1]);
	if (rank_i < 0) {
	    Py_DECREF(py_str);
	    Py_DECREF(iter);
	    PyErr_Format(PyExc_ValueError, "Bad rank '%c' at index %d",
		card_str[1], i);
	    return false;
	}

	plays.push_back(CARD(suit_i, 2+rank_i));
	Py_DECREF(py_str);
    }
    Py_DECREF(iter);

    return true;
}


static bool
pyargs_to_cardlist(std::vector<CARD>& plays, PyObject* args)
{
    PyObject* play_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &play_list))
	return false;

    return pylist_to_cardlist(plays, play_list);
}


static PyObject*
Solver_eval(PyObject* self, PyObject* args)
{
    std::vector<CARD> plays;
    if (!pyargs_to_cardlist(plays, args))
	return NULL;

    Solver_Object* so = (Solver_Object*)self;
    BDT out = so->solver->eval(plays);
    return bdt_to_py_list_of_cubes(out);
}


static PyObject*
ANSolver_eval(PyObject* self, PyObject* args)
{
    ANSolver_Object* so = (ANSolver_Object*)self;
    PyObject* play_list = NULL;
    PyObject* did_list = NULL;
    if (!PyArg_ParseTuple(args, "O|O", &play_list, &did_list))
	return NULL;

    std::vector<CARD> plays;
    if (!pylist_to_cardlist(plays, play_list))
	return NULL;

    if (did_list != NULL) {
	INTSET dids;
	if (!pylist_to_intlist(dids, did_list))
	    return NULL;

	for (INTSET_ITR itr(dids) ; itr.more() ; itr.next()) {
	    if ((size_t)itr.current() > so->problem.wests.size())
		return PyErr_Format(PyExc_IndexError, "%d", itr.current());
	}

	bool out = so->ansolver->eval(plays, dids);
	return PyBool_FromLong(out);
    } else {
	bool out = so->ansolver->eval(plays);
	return PyBool_FromLong(out);
    }
}


static PyObject*
stats_to_pydict(const std::map<std::string, stat_t>& stats)
{
    PyObject* out = PyDict_New();
    if (out == NULL)
	return NULL;

    std::map<std::string, stat_t>::const_iterator itr;
    for (itr = stats.begin() ; itr != stats.end() ; itr++) {
	PyObject* count = PyLong_FromUnsignedLong(itr->second);
	if (count == NULL) {
	    Py_DECREF(out);
	    return NULL;
	}
	if (PyDict_SetItemString(out, itr->first.c_str(), count) < 0) {
	    Py_DECREF(count);
	    Py_DECREF(out);
	    return NULL;
	}
	Py_DECREF(count);
    }
    return out;
}


static PyObject*
Solver_stats(PyObject* self, PyObject* args)
{
    Solver_Object* so = (Solver_Object*)self;
    if (!PyArg_ParseTuple(args, ""))
	return NULL;

    return stats_to_pydict(so->solver->get_stats());
}


static PyObject*
ANSolver_stats(PyObject* self, PyObject* args)
{
    ANSolver_Object* so = (ANSolver_Object*)self;
    if (!PyArg_ParseTuple(args, ""))
	return NULL;

    PyObject* out = PyDict_New();
    if (out == NULL)
	return NULL;

    return stats_to_pydict(so->ansolver->get_stats());
}


static PyMethodDef Solver_RegularMethods[] = {
    { "eval", Solver_eval, METH_VARARGS, "Return a JBDD encoding the existence of play lines which cover various subsets of west/east possibilities" },
    { "stats", Solver_stats, METH_VARARGS, "Return a dict of statistics" },
    { NULL, NULL, 0,  NULL },
};

static PyMethodDef ANSolver_RegularMethods[] = {
    { "eval", ANSolver_eval, METH_VARARGS, "Compute the existence of a play line which covers all west/east possibilities" },
    { "stats", ANSolver_stats, METH_VARARGS, "Return a dict of statistics" },
    { NULL, NULL, 0,  NULL },
};


static PyTypeObject Solver_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "bridgemoose.jade.Solver",
    .tp_basicsize = sizeof(Solver_Object),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = PyDoc_STR("Jordan's Amazing Declarer Evaluater - Solver Object"),
    .tp_methods = Solver_RegularMethods,
    .tp_init = (initproc) Solver_init,
    .tp_new = Solver_new,
    .tp_dealloc = (destructor)Solver_dealloc,
};

static PyTypeObject ANSolver_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "bridgemoose.jade.ANSolver",
    .tp_basicsize = sizeof(ANSolver_Object),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = PyDoc_STR("Jordan's Amazing Declarer Evaluater - All-or-None Solver Object"),
    .tp_methods = ANSolver_RegularMethods,
    .tp_init = (initproc) ANSolver_init,
    .tp_new = ANSolver_new,
    .tp_dealloc = (destructor)ANSolver_dealloc,
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
    PyObject* mod = NULL;
    PyObject* jb_mod = NULL;
    PyObject* bm_mod = PyImport_AddModule("bridgemoose");
    if (bm_mod == NULL)
	goto fie;

    _hand_type = PyObject_GetAttrString(bm_mod, "Hand");
    if (_hand_type == NULL)
	goto fie;

    _partial_hand_type = PyObject_GetAttrString(bm_mod, "PartialHand");
    if (_partial_hand_type == NULL)
	goto fie;

    jb_mod = PyObject_GetAttrString(bm_mod, "jbdd");
    if (jb_mod == NULL)
	goto fie;
    _jbdd_type = PyObject_GetAttrString(jb_mod, "BDD");
    if (_jbdd_type == NULL)
	goto fie;

    /////


    if (PyType_Ready(&Solver_Type) < 0)
	goto fie;
    if (PyType_Ready(&ANSolver_Type) < 0)
	goto fie;

    mod = PyModule_Create(&jade_module);
    if (mod == NULL)
	goto fie;

    Py_INCREF(&Solver_Type);
    if (PyModule_AddObject(mod, "Solver", (PyObject*)&Solver_Type) < 0) {
	Py_DECREF(&Solver_Type);
	goto fie;
    }
    Py_INCREF(&ANSolver_Type);
    if (PyModule_AddObject(mod, "ANSolver", (PyObject*)&ANSolver_Type) < 0) {
	Py_DECREF(&ANSolver_Type);
	goto fie;
    }

    return mod;

  fie:
    Py_XDECREF(mod);
    Py_XDECREF(_jbdd_type);
    Py_XDECREF(_partial_hand_type);
    Py_XDECREF(_hand_type);
    return NULL;
}
