#ifndef _BDT_H_
#define _BDT_H_

#include <cstdint>
#include <vector>
#include "xxhash.h"
#include "intset.h"

typedef uint64_t bdt_key_t;
typedef uint32_t bdt_var_t;

class BDT_NODE {
    bdt_key_t	_avec;
    bdt_key_t	_sans;
    bdt_var_t	_var;

  public:
    BDT_NODE() : _avec(~0),_sans(~0),_var(~0) {}
    BDT_NODE(bdt_var_t v, bdt_key_t a, bdt_key_t s) :
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
    BDT_NODE& operator=(const BDT_NODE& o) {
	if (this != &o) {
	    _avec = o._avec;
	    _sans = o._sans;
	    _var = o._var;
	}
	return *this;
    }

    bdt_var_t var() const { return _var; }
    bdt_key_t avec() const { return _avec; }
    bdt_key_t sans() const { return _sans; }

    bdt_key_t key() const {
	return (uint64_t) XXH3_64bits((const char*)this, sizeof (BDT_NODE));
    }
} __attribute__((packed));



class BDT
{
    bdt_key_t _key;
    static bdt_key_t make(bdt_var_t var, bdt_key_t avec, bdt_key_t sans);
    static bdt_key_t intersect(bdt_key_t a, bdt_key_t b);
    static bdt_key_t unionize(bdt_key_t a, bdt_key_t b);
    static bdt_key_t extrude_inner(bdt_var_t var, bdt_key_t key);
    static bdt_key_t remove_inner(bdt_var_t var, bdt_key_t key);
    BDT(bdt_key_t key) : _key(key) {}
    void get_cubes_inner(std::vector<INTSET>& out, INTSET head, BDT seen,
	bool stoppable) const;

  public:
    BDT() : _key(0) {}
    BDT(const BDT& b) : _key(b._key) {}
    ~BDT() {}

    bool subset_of(const BDT& o) const { return (*this & o) == *this; }
    bool superset_of(const BDT& o) const { return (*this & o) == o; }
    bool operator==(const BDT& o) const { return _key == o._key; }
    bool operator!=(const BDT& o) const { return _key != o._key; }
    bool operator<(const BDT& o) const { return _key < o._key; }
    BDT& operator=(const BDT& o) { _key = o._key; return *this; }
    BDT operator|(const BDT& o) const { return BDT(unionize(_key, o._key)); }
    BDT& operator|=(const BDT& o) { _key = unionize(_key, o._key); return *this; }
    BDT operator&(const BDT& o) const { return BDT(intersect(_key, o._key)); }
    BDT& operator&=(const BDT& o) { _key = intersect(_key, o._key); return *this; }

    bool is_null() const { return _key == 0; }
    BDT extrude(bdt_var_t var) const;
    BDT remove(bdt_var_t var) const;
    BDT require(bdt_var_t var) const;
    INTSET get_used_vars() const;
    static BDT atom(bdt_var_t var);
    static BDT cube(const INTSET& a);
    struct BDT_EXPANSION expand() const;
    std::vector<INTSET> get_cubes() const;
    bool contains(const INTSET& a) const;

    BDT_NODE explain() const;
    bdt_key_t key() const { return _key; }
    static BDT lookup(bdt_key_t key);

    enum { MAP_NUM = 6 };
    static void get_map_sizes(size_t sizes[MAP_NUM]);
};


struct BDT_EXPANSION
{
    bdt_var_t	var;
    BDT		avec;
    BDT		sans;
};

#endif // _BDT_H_
