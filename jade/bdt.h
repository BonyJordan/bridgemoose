#ifndef _BDT_H_
#define _BDT_H_

#include <cstdint>
#include <vector>
#include <map>
#include <stdio.h>
#include "jassert.h"
#include "intset.h"

typedef uint32_t bdt_var_t;

class bdt_t {
    uint32_t  _b;
    bdt_t(uint32_t b) : _b(b) {}

  public:
    bdt_t() : _b(0) {}
    bdt_t(const bdt_t& b) : _b(b._b) {}

    bool operator==(const bdt_t& b) const { return _b == b._b; }
    bool operator!=(const bdt_t& b) const { return _b != b._b; }
    bool operator<(const bdt_t& b) const { return _b < b._b; }
    bool operator>(const bdt_t& b) const { return _b > b._b; }
    bdt_t& operator=(const bdt_t& b) { _b = b._b; return *this; }
    uint32_t get() const { return _b; }
    static bdt_t from(uint32_t b) { return bdt_t(b); }
    bool is_null() const { return _b == 0; }
    bool in_range(size_t n) const { return _b > 0 && (size_t)_b < n; }

    bdt_t operator&(const bdt_t& b) const { jassert(false); return *this; }
    bdt_t operator|(const bdt_t& b) const { jassert(false); return *this; }
    bdt_t& operator&=(const bdt_t& b) { jassert(false); return *this; }
    bdt_t& operator|=(const bdt_t& b) { jassert(false); return *this; }
};


typedef std::pair<bdt_t, bdt_t> BDT_KEY_PAIR;
typedef std::pair<bdt_var_t, bdt_t> BDT_VAR_KEY;

typedef std::map<BDT_KEY_PAIR, bdt_t> BDT_OP_MAP;
typedef std::map<BDT_VAR_KEY, bdt_t> BDT_VAR_MAP;

class BDT_NODE
{
    bdt_t	_avec;
    bdt_t	_sans;
    bdt_var_t	_var;

  public:
    BDT_NODE() : _avec(),_sans(),_var(~0) {}
    BDT_NODE(bdt_var_t v, bdt_t a, bdt_t s) :
	_avec(a),_sans(s),_var(v) {}
    BDT_NODE(const BDT_NODE& x) :
	_avec(x._avec),_sans(x._sans),_var(x._var) {}
    ~BDT_NODE() {}

    bool operator==(const BDT_NODE& o) const {
	return _avec == o._avec && _sans == o._sans && _var == o._var;
    }
    bool operator!=(const BDT_NODE& o) const {
	return _avec != o._avec || _sans != o._sans || _var != o._var;
    }
    bool operator<(const BDT_NODE& o) const {
	if (_var < o._var)
	    return true;
	else if (_var > o._var)
	    return false;

	if (_avec < o._avec)
	    return true;
	else if (_avec > o._avec)
	    return false;

	if (_sans < o._sans)
	    return true;
	else if (_sans > o._sans)
	    return false;

	return false;
    }

    BDT_NODE& operator=(const BDT_NODE& o) {
	if (this != &o) {
	    _avec = o._avec;
	    _sans = o._sans;
	    _var = o._var;
	}
	return *this;
    }

    bdt_var_t var() const { return _var; }
    bdt_t avec() const { return _avec; }
    bdt_t sans() const { return _sans; }

} __attribute__((packed));



class BDT_MANAGER
{
    std::vector<BDT_NODE> _nodes;
    std::map<BDT_NODE, bdt_t> _node_rmap;
    BDT_OP_MAP  _union_map;
    BDT_OP_MAP  _intersect_map;
    BDT_VAR_MAP _extrude_map;
    BDT_VAR_MAP _remove_map;
    BDT_VAR_MAP _require_map;

    bdt_t make(bdt_var_t var, bdt_t avec, bdt_t sans);
    void get_cubes_inner(bdt_t key, std::vector<INTSET>& out,
	INTSET head, bdt_t seen, bool stoppable);

  public:
    bdt_t intersect(bdt_t a, bdt_t b);
    bdt_t unionize(bdt_t a, bdt_t b);
    bdt_t extrude(bdt_t key, bdt_var_t var);
    bdt_t remove(bdt_t key, bdt_var_t var);
    bdt_t require(bdt_t key, bdt_var_t var);
    bool contains(bdt_t key, const INTSET& is);
    std::vector<INTSET> get_cubes(bdt_t key);
    INTSET get_used_vars(bdt_t key) const;
    bool subset_of(bdt_t a, bdt_t b) { return intersect(a,b) == a; }
    bool superset_of(bdt_t a, bdt_t b) { return intersect(a,b) == b; }

    BDT_MANAGER();
    ~BDT_MANAGER();

    bdt_t atom(bdt_var_t var);
    bdt_t cube(const INTSET& a);
    bdt_t null() { return bdt_t::from(0); }
    BDT_NODE expand(bdt_t key) const;

    enum { MAP_NUM = 6 };
    void get_map_sizes(size_t sizes[MAP_NUM]) const;

    bool write_to_file(const char* filename); 
    bool read_from_file(const char* filename);
};


#endif // _BDT_H_
