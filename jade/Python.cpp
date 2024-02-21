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
} ANSolver_Object;


static PyObject*
bdt_to_py_list_of_cubes(BDT_MANAGER& b2, bdt_t key)
{
    std::vector<INTSET> cubes = b2.get_cubes(key);
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


// -1 on fail and sets a Python Error
// 0 on success
static int
pyo_to_west_east(PyObject* iterable, std::vector<hand64_t>& wests,
    std::vector<hand64_t>& easts, hand64_t north, hand64_t south)
{
    PyObject* tuple = NULL;
    PyObject* iter = PyObject_GetIter(iterable);
    if (iter == NULL) {
	return -1;
    }
    wests.clear();
    easts.clear();

    for (int index=0 ; true ; index++)
    {
	PyObject* tuple = PyIter_Next(iter);
	if (tuple == NULL)
	    break;

	PyObject* eobj;
	PyObject* wobj;

	if (!PyArg_ParseTuple(tuple, "OO", &wobj, &eobj))
	    goto errout;

	hand64_t whand, ehand;
	if (!hand_from_pyo(wobj, whand))
	    goto errout;
	if (!hand_from_pyo(eobj, ehand))
	    goto errout;

	if (handbits_count(whand) != handbits_count(north)) {
	    PyErr_Format(PyExc_ValueError, "West[%d] wrong number of cards",
		index);
	    goto errout;
	}
	if (handbits_count(ehand) != handbits_count(north)) {
	    PyErr_Format(PyExc_ValueError, "East[%d] wrong number of cards",
		index);
	    goto errout;
	}
	if ((whand & north) != 0) {
	    PyErr_Format(PyExc_ValueError, "North/West[%d] use same cards",
		index);
	    goto errout;
	}
	if ((whand & south) != 0) {
	    PyErr_Format(PyExc_ValueError, "South/West[%d] use same cards",
		index);
	    goto errout;
	}
	if ((ehand & north) != 0) {
	    PyErr_Format(PyExc_ValueError, "North/East[%d] use same cards",
		index);
	    goto errout;
	}
	if ((ehand & south) != 0) {
	    PyErr_Format(PyExc_ValueError, "South/East[%d] use same cards",
		index);
	    goto errout;
	}

	Py_DECREF(tuple);
	tuple = NULL;

	wests.push_back(whand);
	easts.push_back(ehand);
    }

    Py_DECREF(iter);
    if (PyErr_Occurred())
	return -1;
    else
	return 0;

  errout:
    Py_XDECREF(iter);
    Py_XDECREF(tuple);
    return -1;
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
    if (pyo_to_west_east(we_obj, wests, easts, north, south) < 0)
	return -1;


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
    PROBLEM problem;
    if (pyargs_to_problem(problem, args, kwds) < 0)
	return -1;

    self->ansolver = new ANSOLVER(problem);
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
    bdt_t out = so->solver->eval(plays);
    return bdt_to_py_list_of_cubes(so->solver->bdt_mgr(), out);
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
	    if ((size_t)itr.current() > so->ansolver->problem().wests.size())
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


static PyObject*
ANSolver_write_to_file(PyObject* self, PyObject* args)
{
    ANSolver_Object* so = (ANSolver_Object*)self;
    const char* file_name = NULL;
    if (!PyArg_ParseTuple(args, "s", &file_name))
	return NULL;

    std::string res = so->ansolver->write_to_file(file_name);
    if (res != "") {
	PyErr_SetString(PyExc_OSError, res.c_str());
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject*
ANSolver_read_from_file(PyObject* cls, PyObject* args)
{
    if (!PyType_Check(cls)) {
	PyErr_SetString(PyExc_TypeError, "expected a type object");
	return NULL;
    }
    PyTypeObject* type = (PyTypeObject*)cls;
    const char* file_name = NULL;
    if (!PyArg_ParseTuple(args, "s", &file_name))
	return NULL;

    RESULT<ANSOLVER> res = ANSOLVER::read_from_file(file_name);
    if (res.err != "") {
	PyErr_SetString(PyExc_OSError, res.err.c_str());
	return NULL;
    }

    ANSolver_Object* self = (ANSolver_Object*) type->tp_alloc(type, 0);
    if (self != NULL)
	self->ansolver = res.ok;
    else
	delete res.ok;

    return (PyObject*)self;
}


static PyObject*
ANSolver_fill_tt(PyObject* self, PyObject* args)
{
    ANSolver_Object* so = (ANSolver_Object*)self;
    PyObject* play_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &play_list))
	return NULL;

    std::vector<CARD> plays;
    if (!pylist_to_cardlist(plays, play_list))
	return NULL;

    so->ansolver->fill_tt(plays);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject*
ANSolver_compare_tt(PyObject* self, PyObject* args)
{
    ANSolver_Object* left = (ANSolver_Object*)self;
    PyTypeObject* type = Py_TYPE(left);
    if (type == NULL)
	return NULL;

    ANSolver_Object* right = NULL;
    if (!PyArg_ParseTuple(args, "O!", type, &right)) {
	return NULL;
    }

    left->ansolver->compare_tt(*right->ansolver);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject*
ANSolver_trump(PyObject* self, PyObject* Py_UNUSED(args))
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    static const char* outs = "CDHSN";
    int trump = an->problem().trump;
    if (trump < 0 || trump > 5) {
	PyErr_Format(PyExc_AssertionError, "Bad trump_id: %d", trump);
	return NULL;
    }
    return Py_BuildValue("C", outs[trump]);
}


static PyObject*
ANSolver_target(PyObject* self, PyObject* Py_UNUSED(args))
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    int target = an->problem().target;
    if (target < 1 || target > 13) {
	return PyErr_Format(PyExc_AssertionError,
	    "Bad trick target: %d", target);
    }
    return Py_BuildValue("i", target);
}


static PyObject*
hand_to_pyo(hand64_t hand)
{
    if (handbits_count(hand) != 13)
	return PyErr_Format(PyExc_AssertionError,
	    "Bad hand found: %016llx", hand);

    PyObject* args = Py_BuildValue("(s)", hand_to_string(hand).c_str());
    return PyObject_CallObject(_hand_type, args);
}


static PyObject*
ANSolver_north(PyObject* self, PyObject* Py_UNUSED(args))
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    return hand_to_pyo(an->problem().north);
}


static PyObject*
ANSolver_south(PyObject* self, PyObject* Py_UNUSED(args))
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    return hand_to_pyo(an->problem().south);
}


static PyObject*
ANSolver_west_east(PyObject* self, PyObject* Py_UNUSED(args))
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    size_t sz = an->problem().wests.size();

    PyObject* out = PyList_New(sz);
    if (out == NULL)
	return out;
    for (size_t i=0 ; i<sz ; i++)
    {
	PyObject* w = hand_to_pyo(an->problem().wests[i]);
	if (w == NULL) {
	    Py_DECREF(out);
	    return NULL;
	}
	PyObject* e = hand_to_pyo(an->problem().wests[i]);
	if (w == NULL) {
	    Py_DECREF(w);
	    Py_DECREF(out);
	    return NULL;
	}
	PyObject* pair = PyTuple_Pack(2, w, e);
	if (pair == NULL) {
	    Py_DECREF(w);
	    Py_DECREF(e);
	    Py_DECREF(out);
	    return NULL;
	}
	PyList_SET_ITEM(out, i, pair);
    }

    return out;
}


static PyObject*
ANSolver_add_west_east(PyObject* self, PyObject* iterable)
{
    ANSolver_Object* anso = (ANSolver_Object*)self;
    ANSOLVER* an = anso->ansolver;
    std::vector<hand64_t> wests;
    std::vector<hand64_t> easts;

    if (pyo_to_west_east(iterable, wests, easts,
	an->problem().north, an->problem().south) < 0)
    {
	return NULL;
    }

    an->add_westeast(wests, easts);

    Py_RETURN_NONE;
}



static PyMethodDef Solver_RegularMethods[] = {
    { "eval", Solver_eval, METH_VARARGS, "Return a JBDD encoding the existence of play lines which cover various subsets of west/east possibilities" },
    { "stats", Solver_stats, METH_VARARGS, "Return a dict of statistics" },
    { NULL, NULL, 0,  NULL },
};

static PyMethodDef ANSolver_RegularMethods[] = {
    { "eval", ANSolver_eval, METH_VARARGS, "Compute the existence of a play line which covers all west/east possibilities.  Takes as input a history of card plays and an optional list of deal ids." },
    { "stats", ANSolver_stats, METH_VARARGS, "Return a dict of statistics" },
    { "write_to_file", ANSolver_write_to_file, METH_VARARGS, "Save search cache to a file.  Takes as input a file name." },
    { "read_from_file", ANSolver_read_from_file, METH_VARARGS|METH_CLASS, "Read search cache from a file.  Takes as input a file name." },
    { "fill_tt", ANSolver_fill_tt, METH_VARARGS, "Forcibly fill transition table with all states.  Takes as input a history of card plays." },
    { "compare_tt", ANSolver_compare_tt, METH_VARARGS, "This is a stupid method.Takes as input two ANSolvers." },
    { "trump", ANSolver_trump, METH_NOARGS, "Get the trump suit or 'N'" },
    { "target", ANSolver_target, METH_NOARGS, "Get the target trick count" },
    { "north", ANSolver_north, METH_NOARGS, "Get the north hand" },
    { "south", ANSolver_south, METH_NOARGS, "Get the south hand" },
    { "west_east", ANSolver_west_east, METH_NOARGS, "Get the list of west-east hand pairs" },
    { "add_west_east", ANSolver_add_west_east, METH_O, "Add a list of west-east hand pairs" },
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

//static
PyTypeObject ANSolver_Type = {
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
