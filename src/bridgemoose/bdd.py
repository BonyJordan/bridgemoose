class BDD:
    _nodes_by_id = [None]
    _id_by_key = dict()
    _ite_cache = dict()

    class Node:
        def __init__(self, var, avec, sans):
            assert isinstance(avec, BDD)
            assert isinstance(sans, BDD)
            self.my_id = len(BDD._nodes_by_id)
            self.var = var
            self.avec = avec
            self.sans = sans
            self.count = avec.count() + sans.count()
            self.ncount = avec.ncount() + sans.ncount()

            BDD._nodes_by_id.append(self)
            BDD._id_by_key[(var, avec, sans)] = self.my_id

        def __str__(self):
            return f'({self.my_id})={self.var}?{self.avec.x}:{self.sans.x}'

    @staticmethod
    def get_node(var, avec, sans):
        if not isinstance(var, int):
            raise TypeError(f'Want int not {var}')
        elif var < 0:
            raise ValueError(f'Must be positive: {var}')
        if not isinstance(avec, BDD):
            raise TypeError(avec)
        if not isinstance(sans, BDD):
            raise TypeError(avec)

        if avec == sans:
            return avec

        key1 = (var, avec, sans)
        key2 = (var, ~avec, ~sans)
        if key1 in BDD._id_by_key:
            return BDD(BDD._id_by_key[key1])
        elif key2 in BDD._id_by_key:
            return BDD(-BDD._id_by_key[key2])
        else:
            return BDD(BDD.Node(var, avec, sans).my_id)

    def __init__(self, x):
        if isinstance(x, BDD):
            self.x = x.x
        elif isinstance(x, bool):
            raise TypeError("bools not allowed")
        elif x == 'T' or x == 'F' or isinstance(x, int):
            self.x = x
        else:
            raise TypeError(x)

    def ___eq__(self, other):
        if not isinstance(other, BDD):
            return False
        else:
            return self.x == other.x

    def __hash__(self):
        return hash(self.x)

    def __str__(self):
        if isinstance(self.x, str):
            return self.x
        elif self.x > 0:
            return str(BDD._nodes_by_id[self.x])
        else:
            return "!" + str(BDD._nodes_by_id[self.x])

    def __repr__(self):
        return f'BDD({self.x})'

    @staticmethod
    def ite(i, t, e):
        if not isinstance(i, BDD):
            raise TypeError(i)
        if not isinstance(t, BDD):
            raise TypeError(t)
        if not isinstance(e, BDD):
            raise TypeError(e)

        if i.x == 'T':
            return t
        elif i.x == 'F':
            return e

        if i.x < 0:
            e, t = t, e
            i = ~i

        if i == t:
            t = BDD.TRUE
        elif i == ~t:
            t = BDD.FALSE

        if i == e:
            e = BDD.FALSE
        elif i == ~e:
            e = BDD.TRUE

        if t == e:
            return t
        elif t.x == 'T' and e.x == 'F':
            return i
        elif t.x == 'F' and e.x == 'T':
            return ~i 

        assert not isinstance(i.x, str)

        key1 = (i.x, t.x, e.x)
        key2 = (i.x, (~t).x, (~e).x)

        if key1 in BDD._ite_cache:
            return BDD._ite_cache[key1]
        if key2 in BDD._ite_cache:
            return ~BDD._ite_cache[key2]

        i_node = BDD._nodes_by_id[i.x]
        min_var = i_node.var
        if isinstance(t.x, str):
            t_var = None
        else:
            t_node = BDD._nodes_by_id[abs(t.x)]
            t_var = t_node.var
            min_var = min(min_var, t_var)

        if isinstance(e.x, str):
            e_var = None
        else:
            e_node = BDD._nodes_by_id[abs(e.x)]
            e_var = e_node.var
            min_var = min(min_var, e_var)

        if i_node.var == min_var:
            i_avec = i_node.avec
            i_sans = i_node.sans
        else:
            i_avec = i_sans = i

        if t_var == min_var:
            if t.x > 0:
                t_avec = t_node.avec
                t_sans = t_node.sans
            else:
                t_avec = ~t_node.avec
                t_sans = ~t_node.sans
        else:
            t_avec = t_sans = t

        if e_var == min_var:
            if e.x > 0:
                e_avec = e_node.avec
                e_sans = e_node.sans
            else:
                e_avec = ~e_node.avec
                e_sans = ~e_node.sans
        else:
            e_avec = e_sans = e

        avec = BDD.ite(i_avec, t_avec, e_avec)
        sans = BDD.ite(i_sans, t_sans, e_sans)
        out = BDD.get_node(min_var, avec, sans)

        BDD._ite_cache[key1] = out
        return out

    def __and__(self, other):
        return BDD.ite(self, other, BDD.FALSE)

    def __or__(self, other):
        return BDD.ite(self, BDD.TRUE, other)

    def __xor__(self, other):
        return BDD.ite(self, ~other, other)

    def __sub__(self, other):
        return BDD.ite(other, BDD.FALSE, self)

    def __invert__(self):
        if isinstance(self.x, int):
            return BDD(-self.x)
        elif self.x == 'T':
            return BDD.FALSE
        elif self.x == 'F':
            return BDD.TRUE
        else:
            assert False, (self.x)

    def __str__(self):
        if isinstance(self.x, str):
            return self.x

        node = BDD._nodes_by_id[abs(self.x)]
        return f'{"!"*(self.x < 0)}{node}'

    def count(self):
        if self.x == 'T':
            return 1
        elif self.x == 'F':
            return 0
        elif self.x > 0:
            return BDD._nodes_by_id[self.x].count
        else:
            return BDD._nodes_by_id[-self.x].ncount

    def ncount(self):
        if self.x == 'T':
            return 0
        elif self.x == 'F':
            return 1
        elif self.x > 0:
            return BDD._nodes_by_id[self.x].ncount
        else:
            return BDD._nodes_by_id[-self.x].count

    def size(self):
        seen = set()
        def count(index):
            if isinstance(index, int):
                index = abs(index)
                if index in seen:
                    return 0
                else:
                    seen.add(index)
                    node = BDD._nodes_by_id[index]
                    return 1 + count(node.avec.x) + count(node.sans.x)
            else:
                return 0
        return count(self.x)

    @staticmethod
    def variable(x):
        if not isinstance(x, int):
            return TypeError(f'Must be integer: {x}')
        elif x < 0:
            return ValueError(f'Must be positive: {x}')
        return BDD.get_node(x, BDD.TRUE, BDD.FALSE)

    def bdd_graph_str(self):
        seen = set()
        
        def recurse(bdd):
            if isinstance(bdd.x, str):
                return ""

            xid = abs(bdd.x)
            if xid in seen:
                return ""
            seen.add(xid)
            node = BDD._nodes_by_id[xid]

            return f'{node}\n{recurse(node.avec)}{recurse(node.sans)}'

        if isinstance(self.x, str):
            return self.x + "\n"
        elif self.x > 0:
            return f"POS#{self.x}\n" + recurse(self)
        else:
            return f"NEG#{-self.x}\n" + recurse(self)


BDD.TRUE = BDD('T')
BDD.FALSE = BDD('F')
