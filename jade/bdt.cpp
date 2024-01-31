#include <assert.h>
#include "bdt.h"
#include <map>


BDT_MANAGER::BDT_MANAGER()
{
    // put in a fake node for number 0
    _nodes.push_back(BDT_NODE());
}


BDT_MANAGER::~BDT_MANAGER()
{
}


void BDT_MANAGER::get_map_sizes(size_t sizes[MAP_NUM]) const
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


bdt_t BDT_MANAGER::atom(bdt_var_t var)
{
    return make(var, null(), null());
}


bdt_t BDT_MANAGER::cube(const INTSET& is)
{
    bdt_t out;
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next())
	out = extrude(out, itr.current());
    return out;
}


bdt_t BDT_MANAGER::make(bdt_var_t var, bdt_t avec, bdt_t sans)
{
    BDT_NODE node(var, avec, sans);
    std::map<BDT_NODE, bdt_t>::iterator f = _node_rmap.find(node);
    if (f == _node_rmap.end()) {
	bdt_t key = bdt_t::from(_nodes.size());
	_nodes.push_back(node);
	_node_rmap[node] = key;
	return key;
    } else {
	return f->second;
    }
}


bdt_t BDT_MANAGER::unionize(bdt_t a, bdt_t b)
{
    if (a.is_null())
	return b;
    if (b.is_null())
	return a;
    if (a == b)
	return a;

    BDT_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT_OP_MAP::const_iterator f = _union_map.find(kp);
    if (f != _union_map.end()) {
	return f->second;
    }

    BDT_NODE an = _nodes[a.get()];
    BDT_NODE bn = _nodes[b.get()];
    bdt_t out;

    if (an.var() < bn.var()) {
	bdt_t new_sans = unionize(an.sans(), b);
	out = make(an.var(), an.avec(), new_sans);
    } else if (an.var() > bn.var()) {
	bdt_t new_sans = unionize(bn.sans(), a);
	out = make(bn.var(), bn.avec(), new_sans);
    } else {
	bdt_t new_avec = unionize(an.avec(), bn.avec());
	bdt_t new_sans = unionize(an.sans(), bn.sans());
	out = make(an.var(), new_avec, new_sans);
    }

    _union_map[kp] = out;
    return out;
}


bdt_t BDT_MANAGER::intersect(bdt_t a, bdt_t b)
{
    if (a.is_null() || b.is_null())
	return null();
    if (a == b)
	return a;

    BDT_KEY_PAIR kp(std::min(a, b), std::max(a, b));
    BDT_OP_MAP::const_iterator f = _intersect_map.find(kp);
    if (f != _intersect_map.end()) {
	return f->second;
    }

    BDT_NODE an = _nodes[a.get()];
    BDT_NODE bn = _nodes[b.get()];
    bdt_t out;

    if (an.var() < bn.var()) {
	out = intersect(an.sans(), b);
    } else if (an.var() > bn.var()) {
	out = intersect(a, bn.sans());
    } else {
	bdt_t new_avec = intersect(an.avec(), bn.avec());
	bdt_t new_sans = intersect(an.sans(), bn.sans());
	out = make(an.var(), new_avec, new_sans);
    }

    _intersect_map[kp] = out;
    return out;
}


bdt_t BDT_MANAGER::extrude(bdt_t key, bdt_var_t var)
{
    if (key.is_null()) {
	return make(var, null(), null());
    }

    BDT_VAR_KEY vk(var, key);
    BDT_VAR_MAP::iterator f = _extrude_map.find(vk);
    if (f != _extrude_map.end()) {
	return f->second;
    }

    jassert(key.in_range(_nodes.size()));
    BDT_NODE n = _nodes[key.get()];
    bdt_t out;
    if (n.var() < var) {
	bdt_t new_avec = extrude(n.avec(), var);
	bdt_t new_sans = extrude(n.sans(), var);
	out = make(n.var(), new_avec, new_sans);
    } else if (n.var() > var) {
	out = make(var, key, key);
    } else {
	out = make(var, n.sans(), n.sans());
    }

    _extrude_map[vk] = out;
    return out;
}


bdt_t BDT_MANAGER::require(bdt_t key, bdt_var_t var)
{
    BDT_NODE node = _nodes[key.get()];
    if (node.var() == var) {
	bdt_t out = make(node.var(), node.avec(), node.avec());
	return out;
    } else if (node.var() > var) {
	return null();
    }

    BDT_VAR_KEY vk(var, key);
    BDT_VAR_MAP::iterator f = _require_map.find(vk);
    if (f != _require_map.end())
	return f->second;

    bdt_t avec_key = require(node.avec(), var);
    bdt_t sans_key = require(node.sans(), var);

    bdt_t out;
    if (avec_key.is_null()) {
	out = sans_key;
    } else {
	out = make(node.var(), avec_key, sans_key);
    }
    _require_map[vk] = out;
    return out;
}


bdt_t BDT_MANAGER::remove(bdt_t key, bdt_var_t var)
{
    if (key.is_null())
	return key;

    BDT_NODE node = _nodes[key.get()];
    if (node.var() == var)
	return node.sans();
    else if (node.var() > var)
	return key;

    BDT_VAR_KEY vk(var, key);
    BDT_VAR_MAP::iterator f = _remove_map.find(vk);
    if (f != _remove_map.end())
	return f->second;

    bdt_t avec = remove(node.avec(), var);
    bdt_t sans = remove(node.sans(), var);
    bdt_t out = make(node.var(), avec, sans);
    _remove_map[vk] = out;
    return out;
}


BDT_NODE BDT_MANAGER::expand(bdt_t key) const
{
    jassert(key.in_range(_nodes.size()));
    return _nodes[key.get()];
}


bool BDT_MANAGER::contains(bdt_t key, const INTSET& is)
{
    for (INTSET_ITR itr(is) ; itr.more() ; itr.next()) {
	while (!key.is_null() && _nodes[key.get()].var() < (bdt_var_t)itr.current()) {
	    key = _nodes[key.get()].sans();
	}

	if (key.is_null() || _nodes[key.get()].var() > (bdt_var_t)itr.current())
	    return false;

	key = _nodes[key.get()].avec();
    }
    return true;
}


std::vector<INTSET> BDT_MANAGER::get_cubes(bdt_t key)
{
    std::vector<INTSET> out;
    INTSET head;
    bdt_t seen = null();
    get_cubes_inner(key, out, head, seen, true);
    return out;
}


void BDT_MANAGER::get_cubes_inner(bdt_t key, std::vector<INTSET>& out,
    INTSET head, bdt_t seen, bool stoppable)
{
    if (key.is_null()) {
	if (stoppable) {
	    out.push_back(head);
	}
        return;
    }

    if (subset_of(key, seen))
        return;

    BDT_NODE node = _nodes[key.get()];

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


INTSET BDT_MANAGER::get_used_vars(bdt_t key) const
{
    INTSET out;
    while (!key.is_null()) {
	BDT_NODE node = _nodes[key.get()];
	out.insert(node.var());
	key = node.sans();
    }
    return out;
}


///////////

static const uint32_t FILE_HEADER = 0x315722;

std::string BDT_MANAGER::write_to_file(const char* filename)
{
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
	return oserr_str(filename);
    }

    if (fwrite(&FILE_HEADER, sizeof FILE_HEADER, 1, fp) != 1) {
	fclose(fp);
	return oserr_str(filename);
    }

    uint32_t sz = _nodes.size();
    if (fwrite(&sz, sizeof sz, 1, fp) != 1) {
	fclose(fp);
	return oserr_str(filename);
    }

    for (unsigned i=1 ; i<_nodes.size() ; i++) {
	if (fwrite(&_nodes[i], sizeof _nodes[i], 1, fp) != 1) {
	    fclose(fp);
	    return oserr_str(filename);
	}
    }
    fclose(fp);
    return "";
}

template <class T>
std::string read_thing(T& thing, FILE* fp, const char* filename) {
    if (fread(&thing, sizeof thing, 1, fp) != 1) {
	if (feof(fp)) {
	    fclose(fp);
	    std::string out = filename;
	    out += ": Unexpected end of file";
	    return out;
	} else {
	    fclose(fp);
	    return oserr_str(filename);
	}
    }
    return "";
}


std::string BDT_MANAGER::read_from_file(const char* filename)
{
    if (_nodes.size() != 1) {
	return "cannot read into non empty BDT_MANAGER";
    }

    FILE* fp = fopen(filename, "r");
    if (!fp) {
	return oserr_str(filename);
    }

    uint32_t sz;
    std::string err;
    if ((err = read_thing(sz, fp, filename)) != "") {
	return err;
    }
    if (sz != FILE_HEADER) {
	fclose(fp);
	return std::string(filename) + ": missing header";
    }
    if ((err = read_thing(sz, fp, filename)) != "")
	return err;
    _nodes.resize(sz);

    for (unsigned i=1 ; i<sz ; i++) {
	if ((err = read_thing(_nodes[i], fp, filename)) != "")
	    return err;
	_node_rmap[_nodes[i]] = bdt_t::from(i);
    }
    fclose(fp);
    return "";
}
