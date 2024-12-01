from . import shape

class DealChecker:
    def __init__(self, tree):
        self.tree = tree

    def __call__(self, deal):
        return eval_script(self.tree, deal, {})

def eval_script(tree, deal, context):
    while len(tree) == 3:
        assert tree[0] == "script"
        eval_assignment(tree[1], deal, context)
        tree = tree[2]

    return eval_bexpr(tree[1], deal, context)

def eval_assignment(tree, deal, context):
    action, varname, bexpr = tree
    assert action == "set"
    value = eval_bexpr(bexpr, deal, context)
    context[varname] = value

BEXPR_MAP = {
    "get": lambda t, d, c: c[t[1]],
    "&&": lambda t, d, c: eval_bexpr(t[1],d,c) and eval_bexpr(t[2],d,c),
    "and": lambda t, d, c: eval_bexpr(t[1],d,c) and eval_bexpr(t[2],d,c),
    "||": lambda t, d, c: eval_bexpr(t[1],d,c) or eval_bexpr(t[2],d,c),
    "or": lambda t, d, c: eval_bexpr(t[1],d,c) or eval_bexpr(t[2],d,c),
    "not": lambda t, d, c: not eval_bexpr(t[1],d,c),
    "<": lambda t, d, c: eval_nexpr(t[1],d,c) < eval_nexpr(t[2],d,c),
    "<=": lambda t, d, c: eval_nexpr(t[1],d,c) <= eval_nexpr(t[2],d,c),
    ">": lambda t, d, c: eval_nexpr(t[1],d,c) > eval_nexpr(t[2],d,c),
    ">=": lambda t, d, c: eval_nexpr(t[1],d,c) >= eval_nexpr(t[2],d,c),
    "==": lambda t, d, c: eval_nexpr(t[1],d,c) == eval_nexpr(t[2],d,c),
    "!=": lambda t, d, c: eval_nexpr(t[1],d,c) != eval_nexpr(t[2],d,c),
    "shape": lambda t, d, c: eval_shape(t[1],t[2],d),
    "hascard": lambda t, d, c: eval_hascard(t[1],t[2],d),
}

def eval_bexpr(tree, deal, context):
    lm = BEXPR_MAP[tree[0].lower()]
    return lm(tree, deal, context)

NEXPR_MAP = {
    "+": lambda t, d, c: eval_nexpr(t[1],d,c) + eval_nexpr(t[2],d,c),
    "-": lambda t, d, c: eval_nexpr(t[1],d,c) - eval_nexpr(t[2],d,c),
    "*": lambda t, d, c: eval_nexpr(t[1],d,c) * eval_nexpr(t[2],d,c),
    "/": lambda t, d, c: eval_nexpr(t[1],d,c) // eval_nexpr(t[2],d,c),
    "%": lambda t, d, c: eval_nexpr(t[1],d,c) % eval_nexpr(t[2],d,c),
    "c13": lambda t, d, c: eval_counter(t,d,A=6,K=4,Q=2,J=1),
    "top2": lambda t, d, c: eval_counter(t,d,A=1,K=1),
    "top3": lambda t, d, c: eval_counter(t,d,A=1,K=1,Q=1),
    "top4": lambda t, d, c: eval_counter(t,d,A=1,K=1,Q=1,J=1),
    "top5": lambda t, d, c: eval_counter(t,d,A=1,K=1,Q=1,J=1,T=1),
    "ace": lambda t, d, c: eval_counter(t,d,A=1),
    "aces": lambda t, d, c: eval_counter(t,d,A=1),
    "king": lambda t, d, c: eval_counter(t,d,K=1),
    "kings": lambda t, d, c: eval_counter(t,d,K=1),
    "queen": lambda t, d, c: eval_counter(t,d,Q=1),
    "queens": lambda t, d, c: eval_counter(t,d,Q=1),
    "jack": lambda t, d, c: eval_counter(t,d,J=1),
    "jacks": lambda t, d, c: eval_counter(t,d,J=1),
    "ten": lambda t, d, c: eval_counter(t,d,T=1),
    "tens": lambda t, d, c: eval_counter(t,d,T=1),
    "club": lambda t, d, c: eval_suit("C",t[1],d),
    "clubs": lambda t, d, c: eval_suit("C",t[1],d),
    "diamond": lambda t, d, c: eval_suit("D",t[1],d),
    "diamonds": lambda t, d, c: eval_suit("D",t[1],d),
    "heart": lambda t, d, c: eval_suit("H",t[1],d),
    "hearts": lambda t, d, c: eval_suit("H",t[1],d),
    "spade": lambda t, d, c: eval_suit("S",t[1],d),
    "spades": lambda t, d, c: eval_suit("S",t[1],d),
    "control": lambda t, d, c: eval_counter(t,d,A=2,K=1),
    "controls": lambda t, d, c: eval_counter(t,d,A=2,K=1),
    "hcp": lambda t, d, c: eval_counter(t,d,A=4,K=3,Q=2,J=1),
    "loser": lambda t, d, c: eval_loser(t,d),
    "losers": lambda t, d, c: eval_loser(t,d),
}

def eval_nexpr(tree, deal, context):
    if isinstance(tree, int):
        return tree
    lm = NEXPR_MAP[tree[0].lower()]
    return lm(tree, deal, context)

def eval_shape(compass, spec, deal):
    allowed = shape.get_specified_shapes(spec)
    return deal.hand(compass[0].upper()).pattern in allowed

def eval_hascard(compass, card, deal):
    raise NotImplemented

def eval_counter(tree, deal, **kwargs):
    if len(tree) == 3:
        suits = tree[2][0].upper()
    else:
        suits = "CDHS"

    hand = deal.hand(tree[1][0].upper())

    return sum(kwargs.get(r,0) for s in suits for r in hand.by_suit[s])

def eval_loser(tree, deal):
    raise NotImplemented

def eval_suit(suit, compass, deal):
    return deal.hand(compass[0].upper()).count[suit]

from . import dlex
from . import dyacc

class Parser:
    def __init__(self):
        self.lexer, self.tokens = dlex.make_lexer()
        self.parser = dyacc.make_parser(self.tokens)

    def parse(self, script):
        result = self.parser.parse(script, lexer=self.lexer)
        return DealChecker(result)

the_parser = None

def script_to_filter(script):
    """ Take as input a string representing a DEALER script
    as per https://www.bridgebase.com/tools/dealer/Manual/input.html
    (alhough not everything is implemented).

    Returns a callable which takes as input a bm.Deal and says True or False.
    """
    global the_parser
    if the_parser is None:
        the_parser = Parser()

    return the_parser.parse(script)

__all__ = ["script_to_filter"]
