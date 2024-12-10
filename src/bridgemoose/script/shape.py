from collections import Counter
import itertools
import functools

def get_all_shapes():
    for t in itertools.combinations(range(16), 3):
        s = [-1] + list(t) + [16]
        yield tuple(s[i+1]-s[i]-1 for i in range(4))

def exact_match(spec, shape):
    for a, b in zip(spec, shape):
        if a != "x" and int(a) != b:
            return False
    return True

@functools.lru_cache
def get_exact_shapes(spec):
    out = [shape for shape in get_all_shapes() if exact_match(spec, shape)]
    return set(out)

def counter_ge(a, b):
    return all(a[key] >= val for key,val in b.items())

@functools.lru_cache
def get_any_shapes(spec):
    req = Counter()
    for x in spec:
        if x != "x":
            req[int(x)] += 1

    out = [shape for shape in get_all_shapes() if counter_ge(Counter(shape), req)]
    return set(out)

@functools.lru_cache
def get_specified_shapes(tree):
    if tree[0] == 'exact':
        return get_exact_shapes(tree[1])
    elif tree[0] == 'any':
        return get_any_shapes(tree[1])
    elif tree[0] == '+':
        a1 = get_specified_shapes(tree[1])
        a2 = get_specified_shapes(tree[2])
        return a1 | a2
    elif tree[0] == '-':
        a1 = get_specified_shapes(tree[1])
        a2 = get_specified_shapes(tree[2])
        return a1 - a2
    else:
        assert False

__all__ = ["get_specified_shapes"]
