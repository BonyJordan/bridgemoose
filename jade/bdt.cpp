#include <assert.h>
#include "bdt.h"
#include <map>

// #define USE_SKA

typedef std::pair<bdt_key_t, bdt_key_t> BDT_KEY_PAIR;
typedef std::pair<bdt_var_t, bdt_key_t> BDT_VAR_KEY;

template<>
struct std::hash<BDT_KEY_PAIR>
{
    std::size_t operator()(BDT_KEY_PAIR const& p) const
    {
	std::size_t a = std::hash<bdt_key_t>{}(p.first);
	std::size_t b = std::hash<bdt_key_t>{}(p.second);
	return a ^ (b << 1);
    }
};

template<>
struct std::hash<BDT_VAR_KEY>
{
    std::size_t operator()(BDT_VAR_KEY const& p) const
    {
	std::size_t a = std::hash<bdt_var_t>{}(p.first);
	std::size_t b = std::hash<bdt_key_t>{}(p.second);
	return a ^ (b << 1);
    }
};

#ifdef USE_SKA
typedef ska::flat_hash_map<bdt_key_t, BDT_NODE> BDT_MAP;
typedef ska::flat_hash_map<BDT_KEY_PAIR, bdt_key_t> BDT_OP_MAP;
typedef ska::flat_hash_map<BDT_VAR_KEY, bdt_key_t> BDT_EXTRUDE_MAP;
#else
typedef std::map<bdt_key_t, BDT_NODE> BDT_MAP;
typedef std::map<BDT_KEY_PAIR, bdt_key_t> BDT_OP_MAP;
typedef std::map<BDT_VAR_KEY, bdt_key_t> BDT_EXTRUDE_MAP;
#endif

static BDT_MAP _nodes;
static BDT_OP_MAP _union_map;
static BDT_OP_MAP _intersect_map;
static BDT_EXTRUDE_MAP _extrude_map;
static BDT_EXTRUDE_MAP _remove_map;
static BDT_EXTRUDE_MAP _require_map;

//static
void BDT::get_map_sizes(size_t sizes[MAP_NUM])
{
    size_t i=0;
    sizes[i++] = _nodes.size();
    sizes[i++] = _union_map.size();
    sizes[i++] = _intersect_map.size();
    sizes[i++] = _extrude_map.size();
    sizes[i++] = _remove_map.size();
    sizes[i++] = _require_map.size();
    assert(i == MAP_NUM);
}


//static
BDT BDT::atom(bdt_var_t var)
{
    return BDT(make(var, 0, 0));
}


//static
BDT BDT::cube(const INTSET& is)
{
    BDT out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
	out = out.extrude(itr.current());
    return out;
}


bdt_key_t BDT::make(bdt_var_t var, bdt_key_t avec, bdt_key_t sans)
{
    BDT_NODE node(var, avec, sans);
    _nodes.insert(std::make_pair(node.key(), node));
    return node.key();
}

BDT_NODE BDT::explain() const
{
    const BDT_NODE& an = _nodes[_key];
    return an;
}

//static
BDT BDT::lookup(bdt_key_t key)
{
    BDT_MAP::iterator f = _nodes.find(key);
    if (f == _nodes.end())
	return BDT();
    else
	return BDT(key);
}

//static
bdt_key_t BDT::unionize(bdt_key_t a, bdt_key_t b)
{
    if (a == 0)
	return b;
    if (b == 0)
	return a;
    if (a == b)
	return a;

    BDT_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT_OP_MAP::const_iterator f = _union_map.find(kp);
    if (f != _union_map.end()) {
	return f->second;
    }

    const BDT_NODE& an = _nodes[a];
    const BDT_NODE& bn = _nodes[b];
    bdt_key_t out;

    if (an.var() < bn.var()) {
	bdt_key_t new_sans = unionize(an.sans(), b);
	out = BDT::make(an.var(), an.avec(), new_sans);
    } else if (an.var() > bn.var()) {
	bdt_key_t new_sans = unionize(bn.sans(), a);
	out = BDT::make(bn.var(), bn.avec(), new_sans);
    } else {
	bdt_key_t new_avec = unionize(an.avec(), bn.avec());
	bdt_key_t new_sans = unionize(an.sans(), bn.sans());
	out = BDT::make(an.var(), new_avec, new_sans);
    }

    _union_map[kp] = out;
    return out;
}


//static
bdt_key_t BDT::intersect(bdt_key_t a, bdt_key_t b)
{
    if (a == 0 || b == 0)
	return 0;
    if (a == b)
	return a;

    BDT_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT_OP_MAP::const_iterator f = _intersect_map.find(kp);
    if (f != _intersect_map.end()) {
	return f->second;
    }

    const BDT_NODE& an = _nodes[a];
    const BDT_NODE& bn = _nodes[b];
    bdt_key_t out;

    if (an.var() < bn.var()) {
	out = intersect(an.sans(), b);
    } else if (an.var() > bn.var()) {
	out = intersect(a, bn.sans());
    } else {
	bdt_key_t new_avec = intersect(an.avec(), bn.avec());
	bdt_key_t new_sans = intersect(an.sans(), bn.sans());
	out = BDT::make(an.var(), new_avec, new_sans);
    }

    _intersect_map[kp] = out;
    return out;
}


BDT BDT::extrude(bdt_var_t var) const
{
    return BDT(extrude_inner(var, _key));
}

//static
bdt_key_t BDT::extrude_inner(bdt_var_t var, bdt_key_t key)
{
    if (key == 0) {
	return make(var, 0, 0);
    }

    BDT_VAR_KEY vk(var, key);
    BDT_EXTRUDE_MAP::iterator f = _extrude_map.find(vk);
    if (f != _extrude_map.end())
	return f->second;

    BDT_MAP::iterator g = _nodes.find(key);
    assert(g != _nodes.end());

    const BDT_NODE& n = g->second;
    bdt_key_t out;
    if (n.var() < var) {
	bdt_key_t new_avec = extrude_inner(var, n.avec());
	bdt_key_t new_sans = extrude_inner(var, n.sans());
	out = make(n.var(), new_avec, new_sans);
    } else if (n.var() > var) {
	out = make(var, key, key);
    } else {
	out = make(var, n.sans(), n.sans());
    }

    _extrude_map[vk] = out;
    return out;
}

BDT BDT::remove(bdt_var_t var) const
{
    return BDT(remove_inner(var, _key));
}


BDT BDT::require(bdt_var_t var) const
{
    if (_key == 0)
	return *this;

    const BDT_NODE& node = _nodes[_key];
    if (node.var() == var) {
	bdt_key_t out = make(node.var(), node.avec(), node.avec());
	return BDT(out);
    } else if (node.var() > var) {
	return BDT();
    }

    BDT_VAR_KEY vk(var, _key);
    BDT_EXTRUDE_MAP::iterator f = _require_map.find(vk);
    if (f != _require_map.end())
	return BDT(f->second);

    BDT avec = BDT(node.avec()).require(var);
    BDT sans = BDT(node.sans()).require(var);

    BDT out;
    if (avec.is_null()) {
	out = sans;
    } else {
	out = BDT(make(node.var(), avec._key, sans._key));
    }
    _require_map[vk] = out._key;
    return out;
}


//static
bdt_key_t BDT::remove_inner(bdt_var_t var, bdt_key_t key)
{
    if (key == 0)
	return key;

    const BDT_NODE& node = _nodes[key];
    if (node.var() == var)
	return node.sans();
    else if (node.var() > var)
	return key;

    BDT_VAR_KEY vk(var, key);
    BDT_EXTRUDE_MAP::iterator f = _remove_map.find(vk);
    if (f != _remove_map.end())
	return f->second;

    bdt_key_t avec = remove_inner(var, node.avec());
    bdt_key_t sans = remove_inner(var, node.sans());
    bdt_key_t out = make(node.var(), avec, sans);
    _remove_map[vk] = out;
    return out;
}


BDT_EXPANSION BDT::expand() const
{
    BDT_EXPANSION out;
    assert(_key != 0);
    const BDT_NODE& node = _nodes[_key];
    out.var = node.var();
    out.avec = BDT(node.avec());
    out.sans = BDT(node.sans());
    return out;
}


bool BDT::contains(const INTSET& is) const
{
    bdt_key_t key = _key;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next()) {
	while (key != 0 && _nodes[key].var() < (bdt_var_t)itr.current()) {
	    key = _nodes[key].sans();
	}

	if (key == 0 || _nodes[key].var() > (bdt_var_t)itr.current())
	    return false;

	key = _nodes[key].avec();
    }
    return true;
}


std::vector<INTSET> BDT::get_cubes() const
{
    std::vector<INTSET> out;
    INTSET head;
    BDT seen;

    get_cubes_inner(out, head, seen, true);

    // SANITY CHECKING!!
    if (out.size() == 1 && out[0].empty()) {
	assert(is_null());
	return out;
    }

    BDT sanity_f, sanity_b;
    for (std::vector<INTSET>::iterator itr = out.begin() ; itr != out.end() ;
	itr++)
    {
	BDT c = BDT::cube(*itr);
	assert( (c & sanity_f) != c );
	sanity_f |= c;
    }
    assert(sanity_f == *this);

    for (std::vector<INTSET>::reverse_iterator ritr = out.rbegin() ;
	ritr != out.rend() ; ritr++)
    {
	BDT c = BDT::cube(*ritr);
	assert( (c & sanity_b) != c );
	sanity_b |= c;
    }

    return out;
}


void BDT::get_cubes_inner(std::vector<INTSET>& out, INTSET head, BDT seen,
    bool stoppable) const
{
    if (_key == 0) {
	if (stoppable) {
	    out.push_back(head);
	}
        return;
    }

    if ( (*this & seen) == *this) {
        return;
    }

    const BDT_NODE& node = _nodes[_key];

    if (node.avec() == node.sans()) {
	head.insert(node.var());
	BDT(node.avec()).get_cubes_inner(out, head, seen.require(node.var()), true);
	head.remove(node.var());
    } else {
	// first avec.
	BDT avec(node.avec());
	head.insert(node.var());
	if (avec.extrude(node.var()).subset_of(seen))
	    ;
	else
	    avec.get_cubes_inner(out, head, seen.require(node.var()), true);
	head.remove(node.var());

        seen |= avec;
	BDT sans(node.sans());
	sans.get_cubes_inner(out, head, seen, false);
    }
    // fin
}


INTSET BDT::get_used_vars() const
{
    INTSET out;
    bdt_key_t key = _key;
    while (key != 0) {
	const BDT_NODE& node = _nodes[key];
	out.insert(node.var());
	key = node.sans();
    }
    return out;
}
