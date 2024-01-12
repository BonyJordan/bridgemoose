#include <assert.h>
#include "bdt2.h"
#include <map>


BDT2_MANAGER::BDT2_MANAGER()
{
    // put in a fake node for number 0
    _nodes.push_back(BDT2_NODE());
}


BDT2_MANAGER::~BDT2_MANAGER()
{
}


void BDT2_MANAGER::get_map_sizes(size_t sizes[MAP_NUM]) const
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


bdt2_t BDT2_MANAGER::atom(bdt2_var_t var)
{
    return make(var, 0, 0);
}


bdt2_t BDT2_MANAGER::cube(const INTSET& is)
{
    bdt2_t out = 0;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
	out = extrude(out, itr.current());
    return out;
}


bdt2_t BDT2_MANAGER::make(bdt2_var_t var, bdt2_t avec, bdt2_t sans)
{
    BDT2_NODE node(var, avec, sans);
    std::map<BDT2_NODE, bdt2_t>::iterator f = _node_rmap.find(node);
    if (f == _node_rmap.end()) {
	bdt2_t key = _nodes.size();
	_nodes.push_back(node);
	_node_rmap[node] = key;
	return key;
    } else {
	return f->second;
    }
}


bdt2_t BDT2_MANAGER::unionize(bdt2_t a, bdt2_t b)
{
    if (a == 0)
	return b;
    if (b == 0)
	return a;
    if (a == b)
	return a;

    BDT2_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT2_OP_MAP::const_iterator f = _union_map.find(kp);
    if (f != _union_map.end()) {
	return f->second;
    }

    const BDT2_NODE& an = _nodes[a];
    const BDT2_NODE& bn = _nodes[b];
    bdt2_t out;

    if (an.var() < bn.var()) {
	bdt2_t new_sans = unionize(an.sans(), b);
	out = make(an.var(), an.avec(), new_sans);
    } else if (an.var() > bn.var()) {
	bdt2_t new_sans = unionize(bn.sans(), a);
	out = make(bn.var(), bn.avec(), new_sans);
    } else {
	bdt2_t new_avec = unionize(an.avec(), bn.avec());
	bdt2_t new_sans = unionize(an.sans(), bn.sans());
	out = make(an.var(), new_avec, new_sans);
    }

    _union_map[kp] = out;
    return out;
}


bdt2_t BDT2_MANAGER::intersect(bdt2_t a, bdt2_t b)
{
    if (a == 0 || b == 0)
	return 0;
    if (a == b)
	return a;

    BDT2_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT2_OP_MAP::const_iterator f = _intersect_map.find(kp);
    if (f != _intersect_map.end()) {
	return f->second;
    }

    const BDT2_NODE& an = _nodes[a];
    const BDT2_NODE& bn = _nodes[b];
    bdt2_t out;

    if (an.var() < bn.var()) {
	out = intersect(an.sans(), b);
    } else if (an.var() > bn.var()) {
	out = intersect(a, bn.sans());
    } else {
	bdt2_t new_avec = intersect(an.avec(), bn.avec());
	bdt2_t new_sans = intersect(an.sans(), bn.sans());
	out = make(an.var(), new_avec, new_sans);
    }

    _intersect_map[kp] = out;
    return out;
}


bdt2_t BDT2_MANAGER::extrude(bdt2_t key, bdt2_var_t var)
{
    if (key == 0) {
	return make(var, 0, 0);
    }

    BDT2_VAR_KEY vk(var, key);
    BDT2_VAR_MAP::iterator f = _extrude_map.find(vk);
    if (f != _extrude_map.end())
	return f->second;

    const BDT2_NODE& n = _nodes[key];
    bdt2_t out;
    if (n.var() < var) {
	bdt2_t new_avec = extrude(n.avec(), var);
	bdt2_t new_sans = extrude(n.sans(), var);
	out = make(n.var(), new_avec, new_sans);
    } else if (n.var() > var) {
	out = make(var, key, key);
    } else {
	out = make(var, n.sans(), n.sans());
    }

    _extrude_map[vk] = out;
    return out;
}


bdt2_t BDT2_MANAGER::require(bdt2_t key, bdt2_var_t var)
{
    const BDT2_NODE& node = _nodes[key];
    if (node.var() == var) {
	bdt2_t out = make(node.var(), node.avec(), node.avec());
	return out;
    } else if (node.var() > var) {
	return 0;
    }

    BDT2_VAR_KEY vk(var, key);
    BDT2_VAR_MAP::iterator f = _require_map.find(vk);
    if (f != _require_map.end())
	return f->second;

    bdt2_t avec_key = require(node.avec(), var);
    bdt2_t sans_key = require(node.sans(), var);

    bdt2_t out;
    if (avec_key == 0) {
	out = sans_key;
    } else {
	out = make(node.var(), avec_key, sans_key);
    }
    _require_map[vk] = out;
    return out;
}


bdt2_t BDT2_MANAGER::remove(bdt2_t key, bdt2_var_t var)
{
    if (key == 0)
	return key;

    const BDT2_NODE& node = _nodes[key];
    if (node.var() == var)
	return node.sans();
    else if (node.var() > var)
	return key;

    BDT2_VAR_KEY vk(var, key);
    BDT2_VAR_MAP::iterator f = _remove_map.find(vk);
    if (f != _remove_map.end())
	return f->second;

    bdt2_t avec = remove(node.avec(), var);
    bdt2_t sans = remove(node.sans(), var);
    bdt2_t out = make(node.var(), avec, sans);
    _remove_map[vk] = out;
    return out;
}


BDT2_NODE BDT2_MANAGER::expand(bdt2_t key) const
{
    jassert(key > 0);
    jassert(key < _nodes.size());
    return _nodes[key];
}


bool BDT2_MANAGER::contains(bdt2_t key, const INTSET& is)
{
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next()) {
	while (key != 0 && _nodes[key].var() < (bdt2_var_t)itr.current()) {
	    key = _nodes[key].sans();
	}

	if (key == 0 || _nodes[key].var() > (bdt2_var_t)itr.current())
	    return false;

	key = _nodes[key].avec();
    }
    return true;
}


std::vector<INTSET> BDT2_MANAGER::get_cubes(bdt2_t key)
{
    std::vector<INTSET> out;
    INTSET head;
    bdt2_t seen = 0;
    get_cubes_inner(key, out, head, seen, true);
    return out;
}


void BDT2_MANAGER::get_cubes_inner(bdt2_t key, std::vector<INTSET>& out,
    INTSET head, bdt2_t seen, bool stoppable)
{
    if (key == 0) {
	if (stoppable) {
	    out.push_back(head);
	}
        return;
    }

    if (subset_of(key, seen))
        return;

    const BDT2_NODE& node = _nodes[key];

    if (node.avec() == node.sans()) {
	head.insert(node.var());
	get_cubes_inner(node.avec(), out, head, require(seen, node.var()), true);
	head.remove(node.var());
    } else {
	// first avec.
	head.insert(node.var());
	if (subset_of(extrude(node.avec(), node.var()), seen))
	    ;
	else
	    get_cubes_inner(node.avec(), out, head, require(seen, node.var()), true);
	head.remove(node.var());

        seen = unionize(seen, node.avec());
	get_cubes_inner(node.sans(), out, head, seen, false);
    }
    // fin
}


INTSET BDT2_MANAGER::get_used_vars(bdt2_t key) const
{
    INTSET out;
    while (key != 0) {
	const BDT2_NODE& node = _nodes[key];
	out.insert(node.var());
	key = node.sans();
    }
    return out;
}
