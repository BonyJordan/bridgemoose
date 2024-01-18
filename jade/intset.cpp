#include <assert.h>
#include "intset.h"


void INTSET::insert(int x)
{
    _data.insert(x);
}

void INTSET::remove(int x)
{
    _data.erase(x);
}

void INTSET::remove_all()
{
    _data.clear();
}

bool INTSET::contains(int x) const
{
    std::set<int>::const_iterator f = _data.find(x);
    return f != _data.end();
}

int INTSET::pop_smallest()
{
    assert(!_data.empty());
    std::set<int>::iterator f = _data.begin();
    int out = *f;
    _data.erase(f);
    return out;
}

////////////////

INTSET_ITR::INTSET_ITR(const INTSET& iset) :
    _store(iset),
    _itr(_store._data.begin())
{
}


bool INTSET_ITR::more() const
{
    return _itr != _store._data.end();
}

int INTSET_ITR::current() const
{
    return *_itr;
}

void INTSET_ITR::next()
{
    _itr++;
}

//static
INTSET INTSET::combine(const INTSET& a, const INTSET& b)
{
    INTSET out;
    for (INTSET_PAIR_ITR pi(a,b) ; pi.more() ; pi.next())
	out.insert(pi.current());
    return out;
}

bool INTSET::operator==(const INTSET& a) const
{
    for (INTSET_PAIR_ITR pi(*this, a) ; pi.more() ; pi.next())
	if (!pi.both())
	    return false;
    return true;
}

bool INTSET::subset_of(const INTSET& other) const
{
    for (INTSET_PAIR_ITR itr(*this, other) ; itr.more() ; itr.next()) {
	if (itr.a_only())
	    return false;
    }
    return true;
}

bool INTSET::superset_of(const INTSET& other) const
{
    for (INTSET_PAIR_ITR itr(*this, other) ; itr.more() ; itr.next()) {
	if (itr.b_only())
	    return false;
    }
    return true;
}

////////////////

INTSET_PAIR_ITR::INTSET_PAIR_ITR(const INTSET& a, const INTSET& b) :
    _a(a),
    _b(b),
    _a_itr(_a._data.begin()),
    _b_itr(_b._data.begin())
{
    calc_only();
}


void INTSET_PAIR_ITR::calc_only()
{
    _a_only = false;
    _b_only = false;
    _both = false;

    if (_a_itr == _a._data.end()) {
	if (_b_itr == _b._data.end())
	    return;
	_b_only = true;
    } else if (_b_itr == _b._data.end()) {
	assert(_a_itr != _a._data.end());
	_a_only = true;
    } else if (*_a_itr < *_b_itr)
	_a_only = true;
    else if (*_a_itr > *_b_itr)
	_b_only = true;
    else
	_both = true;
}


bool INTSET_PAIR_ITR::more() const
{
    return _a_itr != _a._data.end() || _b_itr != _b._data.end();
}


int INTSET_PAIR_ITR::current() const
{
    if (_a_only || _both)
	return *_a_itr;
    else
	return *_b_itr;
}

void INTSET_PAIR_ITR::next()
{
    if (_a_only)
	_a_itr++;
    else if (_b_only)
	_b_itr++;
    else {
	_a_itr++;
	_b_itr++;
    }

    calc_only();
}


///////////////////

//static
INTSET INTSET::full_set(int n)
{
    INTSET out;
    for (int i=0 ; i<n ; i++)
	out.insert(i);
    return out;
}

/////////////////

std::string intset_to_string(const INTSET& intset)
{
    std::string out = "[";
    bool first = true;
    char buf[20];

    for (INTSET_ITR itr(intset) ; itr.more() ; itr.next()) {
        if (first)
            first = false;
        else
            out += ',';

        snprintf(buf, sizeof buf, "%d", itr.current());
        out += buf;
    }
    return out + ']';
}
