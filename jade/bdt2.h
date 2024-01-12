#ifndef _BDT2_H_
#define _BDT2_H_

#include <cstdint>
#include <vector>
#include <map>
#include "jassert.h"
#include "intset.h"

typedef uint32_t bdt2_var_t;
typedef uint32_t bdt2_t;

typedef std::pair<bdt2_t, bdt2_t> BDT2_KEY_PAIR;
typedef std::pair<bdt2_t, bdt2_t> BDT2_VAR_KEY;

typedef std::map<BDT2_KEY_PAIR, bdt2_t> BDT2_OP_MAP;
typedef std::map<BDT2_VAR_KEY, bdt2_t> BDT2_VAR_MAP;

class BDT2_NODE
{
    bdt2_t	_avec;
    bdt2_t	_sans;
    bdt2_var_t	_var;

  public:
    BDT2_NODE() : _avec(~0),_sans(~0),_var(~0) {}
    BDT2_NODE(bdt2_var_t v, bdt2_t a, bdt2_t s) :
	_avec(a),_sans(s),_var(v) {}
    BDT2_NODE(const BDT2_NODE& x) :
	_avec(x._avec),_sans(x._sans),_var(x._var) {}
    ~BDT2_NODE() {}

    bool operator==(const BDT2_NODE& o) const {
	return _avec == o._avec && _sans == o._sans && _var == o._var;
    }
    bool operator!=(const BDT2_NODE& o) const {
	return _avec != o._avec || _sans != o._sans || _var != o._var;
    }
    bool operator<(const BDT2_NODE& o) const {
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

    BDT2_NODE& operator=(const BDT2_NODE& o) {
	if (this != &o) {
	    _avec = o._avec;
	    _sans = o._sans;
	    _var = o._var;
	}
	return *this;
    }

    bdt2_var_t var() const { return _var; }
    bdt2_t avec() const { return _avec; }
    bdt2_t sans() const { return _sans; }

} __attribute__((packed));



class BDT2_MANAGER
{
    std::vector<BDT2_NODE> _nodes;
    std::map<BDT2_NODE, bdt2_t> _node_rmap;
    BDT2_OP_MAP  _union_map;
    BDT2_OP_MAP  _intersect_map;
    BDT2_VAR_MAP _extrude_map;
    BDT2_VAR_MAP _remove_map;
    BDT2_VAR_MAP _require_map;

    bdt2_t make(bdt2_var_t var, bdt2_t avec, bdt2_t sans);
    void get_cubes_inner(bdt2_t key, std::vector<INTSET>& out,
	INTSET head, bdt2_t seen, bool stoppable);

  public:
    bdt2_t intersect(bdt2_t a, bdt2_t b);
    bdt2_t unionize(bdt2_t a, bdt2_t b);
    bdt2_t extrude(bdt2_t key, bdt2_var_t var);
    bdt2_t remove(bdt2_t key, bdt2_var_t var);
    bdt2_t require(bdt2_t key, bdt2_var_t var);
    bool contains(bdt2_t key, const INTSET& is);
    std::vector<INTSET> get_cubes(bdt2_t key);
    INTSET get_used_vars(bdt2_t key) const;
    bool subset_of(bdt2_t a, bdt2_t b) { return intersect(a,b) == a; }
    bool superset_of(bdt2_t a, bdt2_t b) { return intersect(a,b) == b; }

    BDT2_MANAGER();
    ~BDT2_MANAGER();

    bdt2_t atom(bdt2_var_t var);
    bdt2_t cube(const INTSET& a);
    bdt2_t null() { return bdt2_t(0); }
    BDT2_NODE expand(bdt2_t key) const;

    enum { MAP_NUM = 6 };
    void get_map_sizes(size_t sizes[MAP_NUM]);
};


#endif // _BDT2_H_
