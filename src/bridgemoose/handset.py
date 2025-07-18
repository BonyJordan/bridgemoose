from collections import defaultdict, Counter
import functools
import itertools
import operator
import random
import re
import time
from .direction import Direction
from .card import Card
from .deal import Deal, Hand
from .play import PartialHand
from .jbdd import BDD

debug = False


class HandSetMetric:
    def __init__(self, values):
        self.cache = {}
        self.values = values

    def __eq__(self, other):
        if isinstance(other, HandSetMetric):
            return (self - other) == 0
        elif isinstance(other, int):
            key = ("=", other)
            if key in self.cache:
                return HandSet(self.cache[key])
            else:
                if self.values is None:
                    self._compute_values()
                out = self.values.get(other, BDD.false())
                self.cache[key] = out
                return HandSet(out)
        else:
            raise TypeError(other)

    def __ne__(self, other):
        return ~(self == other)

    def cmp_func_make(op, from_int):
        def cmp_func(self, other):
            if isinstance(other, HandSetMetric):
                return op(self - other, 0)
            elif isinstance(other, int):
                return from_int(self, other)
            else:
                raise TypeError(other)
        return cmp_func

    __le__ = cmp_func_make(operator.le, lambda self, n: self.less_than(n+1))
    __lt__ = cmp_func_make(operator.lt, lambda self, n: self.less_than(n))
    __ge__ = cmp_func_make(operator.ge, lambda self, n: ~self.less_than(n))
    __gt__ = cmp_func_make(operator.gt, lambda self, n: ~self.less_than(n+1))

    def make_arith_func(op):
        def arith_func(self, other):
            if not isinstance(other, HandSetMetric):
                raise TypeError(other)
            
            out = dict()
            for key1, val1 in self.values.items():
                for key2, val2 in other.values.items():
                    key3 = op(key1, key2)
                    both = val1 & val2
                    if key3 in out:
                        out[key3] |= both
                    else:
                        out[key3] = both

            return HandSetMetric(out)
        return arith_func

    __add__ = make_arith_func(operator.add)
    __sub__ = make_arith_func(operator.sub)

    def __mul__(self, other):
        return HandSetMetric({key*other: val for key, val in self.values.items()})
    __rmul__ = __mul__


    def less_than(self, n):
        if self.values is None:
            self._compute_values()

        if n < min(self.values.keys()):
            return HandSet(BDD.false())

        key = ("<",n)
        if key in self.cache:
            return HandSet(self.cache[key])

        a = self.less_than(n-1).bdd
        b = self.values.get(n-1, BDD.false())
        out = a | b
        self.cache[key] = out
        return HandSet(out)

class SimpleHandMetric(HandSetMetric):
    cards = [Card(suit, rank) for rank in "AKQJ" for suit in "SHDC"] +\
        [Card(suit, rank) for suit in "SHDC" for rank in "T98765432"]
    card_index = {c:i for i, c in enumerate(cards)}

    def __init__(self, scores):
        values = {0: BDD.true()}
        for card in reversed(sorted(scores, key=SimpleHandMetric.card_index.get)):
            card_value = scores[card]
            aveces = {val + card_value: hs for val, hs in values.items()}
            sanses = {val: hs for val, hs in values.items()}

            values = {}
            for val in aveces.keys() | sanses.keys():
                a = aveces.get(val, BDD.false())
                s = sanses.get(val, BDD.false())
                n = BDD(SimpleHandMetric.card_index[card]).thenelse(a, s)
                values[val] = n

        super(SimpleHandMetric, self).__init__(values)

class QuickTricksMetric(HandSetMetric):
    def __init__(self):
        suit_val_list = [QuickTricksMetric.suit_values(s).items() for s in "SHDC"]
        values = dict()
        for tupe in itertools.product(*suit_val_list):
            key = sum(x[0] for x in tupe)
            val = functools.reduce(BDD.__and__, [x[1] for x in tupe])
            if key in values:
                values[key] |= val
            else:
                values[key] = val
        super(QuickTricksMetric, self).__init__(values)

    @staticmethod
    def suit_values(suit):
        ranks = [var for var, card in enumerate(SimpleHandMetric.cards) if card.suit == suit]
        have_x = BDD.false()
        for var in reversed(ranks[3:]):
            have_x = BDD(var).thenelse(BDD.true(), have_x)
        have_q = BDD(ranks[2])
        have_k = BDD(ranks[1])
        have_a = BDD(ranks[0])

        values = {}
        # 2.0 quick tricks: AK
        values[4] = have_a & have_k

        # 1.5 quick tricks: AQ no K
        values[3] = have_a & ~have_k & have_q

        # 1.0 quick tricks: A no K, no Q ... or KQ
        values[2] = (have_a & ~have_k & ~have_q) | (~have_a & have_k & have_q)

        # 0.5 quick tricks: no A, K, no Q, x
        values[1] = ~have_a & have_k & ~have_q & have_x

        # 0.0 quick tricks: no A, [no K  or K but stiff]
        values[0] = ~have_a & have_k.thenelse(~have_q & ~have_x, BDD.true())

        for a, b in itertools.combinations(values.values(), 2):
            assert a & b == BDD.false()

        red = functools.reduce(BDD.__or__, values.values())
        assert red == BDD.true(), (red.bdd_graph_str())

        return values

class DealSet:
    def __init__(self, d):
        if d is None:
            self.d = BDD.false()
        elif isinstance(d, BDD):
            self.d = d
        else:
            raise TypeError("BDD or None")

    def sample(self, rng=random):
        index = rng.randrange(self.d.pcount())
        bits = set(DealSet.get_bits(self.d, index))
        hand_lists = [[],[],[],[]]
        for i, card in enumerate(SimpleHandMetric.cards):
            owner = 2*(i*2+1 in bits) + 1*(i*2 in bits)
            hand_lists[owner].append(card)

        return Deal(*[Hand(h) for h in hand_lists])

    def contains(self, deal):
        if not isinstance(deal, Deal):
            raise TypeError("bridgemoose.Deal expected")

        bits = set()

        for i, card in enumerate(SimpleHandMetric.cards):
            for j, hand in enumerate(deal):
                if card in hand.cards:
                    break
            else:
                assert False, (f"missing {card} somehow")

            if j & 2:
                bits.add(i*2+1)
            if j & 1:
                bits.add(i*2)

        return self.d.eval_pset(bits)


    def count(self):
        return self.d.pcount()

    @staticmethod
    def get_bits(bdd, index):
        return bdd.get_pindex(index)

    def __and__(self, other):
        return DealSet(self.d & other.d)
    def __or__(self, other):
        return DealSet(self.d | other.d)
    def __xor___(self, other):
        return DealSet(self.d ^ other.d)
    def __invert__(self):
        return DealSet(~self.d)
    def ite(self, t, e):
        return DealSet(self.bdd.thenelse(t.bdd, e.bdd))

class DealSetConverter:
    four_hands = None

    def __init__(self, player):
        self.player = Direction(player)
        self.cache = dict()

    def __call__(self, hs):
        if not isinstance(hs, HandSet):
            raise TypeError("Require HandSet")

        if DealSetConverter.four_hands is None:
            DealSetConverter._compute_four_hands()

        return DealSet(self._doit(hs.bdd) & DealSetConverter.four_hands)

    @staticmethod
    def _compute_four_hands(N=13):
        def sub(tupe, index):
            return tuple(x - 1*(i==index) for i, x in enumerate(tupe))

        tier = {(0,0,0,0) : BDD.true()}
        for k in range(1,N*4+1):
            next_tier = {}

            v1 = (N*4-k)*2
            v2 = v1 + 1

            for tupe in itertools.combinations(range(k+3), 3):
                target = tuple_to_pattern(tupe, k)
                if max(target) > N:
                    continue

                subs = [tier.get(sub(target, i), BDD.false()) for i in range(4)]
                tx = BDD(v2).thenelse(subs[2], subs[3])
                fx = BDD(v2).thenelse(subs[0], subs[1])
                next_tier[target] = BDD(v1).thenelse(tx, fx)

            tier = next_tier

        assert len(tier) == 1, (tier.keys())
        DealSetConverter.four_hands = tier[(N,N,N,N)]

    def _doit(self, bdd):
        split = bdd.split()
        if split is True or split is False:
            return bdd

        if bdd in self.cache:
            return self.cache[bdd]

        var, avec, sans = split
        avec = self._doit(avec)
        sans = self._doit(sans)

        if self.player.i & 2:
            n2 = BDD(2*var + 1).thenelse(avec, sans)
        else:
            n2 = BDD(2*var + 1).thenelse(sans, avec)

        if self.player.i & 1:
            n1 = BDD(2*var).thenelse(n2, sans)
        else:
            n1 = BDD(2*var).thenelse(sans, n2)

        self.cache[bdd] = n1
        return n1

def tuple_to_pattern(tupe, n):
    s = [-1] + list(tupe) + [n + len(tupe)]
    out = tuple(s[i+1]-s[i]-1 for i in range(len(tupe)+1))
    assert min(out) >= 0, (tupe, n)
    return out

class IncrTuple(tuple):
    def __new__(cls, lst):
        return super(IncrTuple, cls).__new__(cls, tuple(lst))

    def incr(self, index):
        l = list(self)
        l[index] += 1
        return IncrTuple(l)

class ShapeMaker:
    THE_RE = re.compile(r'(?P<SKIP>\s+)|(?P<ANY>any)|(?P<OP>[-+])|(?P<PAT>[0-9x]{4})|(?P<ERROR>.)')
    ALL = [tuple_to_pattern(t, 13) for t in itertools.combinations(range(16),3)]
    BDDS = None

    @staticmethod
    def get_pattern_bdds():
        SID = {'S':0, 'H':1, 'D':2, 'C': 3}
        if ShapeMaker.BDDS is not None:
            return ShapeMaker.BDDS

        so_far = {IncrTuple([0,0,0,0]): BDD.true()}
        for i, card in reversed(list(enumerate(SimpleHandMetric.cards))):
            sid = SID[card.suit]
            var = BDD(i)
            after = {pat: (bdd & ~var) for pat, bdd in so_far.items()}
            for pat, bdd in so_far.items():
                ipat = pat.incr(sid)
                if sum(ipat) > 13:
                    continue

                ibdd = (bdd & var)
                if ipat in after:
                    after[ipat] |= ibdd
                else:
                    after[ipat] = ibdd

            so_far = after

        ShapeMaker.BDDS = {pat: bdd for pat, bdd in so_far.items() if sum(pat) == 13}
        return ShapeMaker.BDDS


    @staticmethod
    def matching_tuples(spec):
        assert len(spec) == 4
        out = []
        for tupe in ShapeMaker.ALL:
            for i in range(4):
                if spec[i] not in ('x', str(tupe[i])):
                    break
            else:
                out.append(tupe)
        return out

    class ExBase:
        def do_end(self):
            raise ValueError("End of line not expected")
        def do_skip(self, mo, so_far):
            return self
        def do_any(self, mo, so_far):
            raise ValueError(f"'any' not expected at f{mo.start() + 1}")
        def do_op(self, mo, so_far):
            raise ValueError(f"operator not expected at f{mo.start() + 1}")
        def do_pat(self, mo, so_far):
            raise ValueError(f"shape pattern not expected at f{mo.start() + 1}")

    class ExAnyOrPat(ExBase):
        def __init__(self, sign):
            self.sign = sign
        def do_any(self, mo, so_far):
            return ShapeMaker.ExPat(self.sign)
        def do_pat(self, mo, so_far):
            if self.sign == '+':
                so_far.update(ShapeMaker.matching_tuples(mo.group()))
            elif self.sign == '-':
                so_far.difference_update(ShapeMaker.matching_tuples(mo.group()))
            else:
                assert False
            return ShapeMaker.ExOp()

    class ExPat(ExBase):
        def __init__(self, sign):
            self.sign = sign
        def do_pat(self, mo, so_far):
            m_tupes = set()
            for pat_spec in set(itertools.permutations(mo.group())):
                m_tupes.update(ShapeMaker.matching_tuples(pat_spec))
            if self.sign == '+':
                so_far |= m_tupes
            elif self.sign == '-':
                so_far -= m_tupes
            else:
                assert False
            return ShapeMaker.ExOp()

    class ExOp(ExBase):
        def do_end(self):
            pass
        def do_op(self, mo, so_far):
            return ShapeMaker.ExAnyOrPat(mo.group())


    @staticmethod
    def get_handset(spec):
        state = ShapeMaker.ExAnyOrPat('+')
        so_far = set()

        for mo in ShapeMaker.THE_RE.finditer(spec):
            if mo.lastgroup == "SKIP":
                continue
            elif mo.lastgroup == "ANY":
                state = state.do_any(mo, so_far)
            elif mo.lastgroup == "OP":
                state = state.do_op(mo, so_far)
            elif mo.lastgroup == "PAT":
                state = state.do_pat(mo, so_far)
            else:
                assert mo.lastgroup == "ERROR"
                raise ValueError(f"Unexpected character `{mo.group()}' at pos {mo.start()+1}")
        state.do_end()

        m = hand_makers()
        out = HandSet(BDD.false())
        for pat in so_far:
            pat_bdd = functools.reduce(HandSet.__and__, [x == y for x, y in zip([m.NUM_SP, m.NUM_HE, m.NUM_DI, m.NUM_CL], pat)])
            out |= pat_bdd

        return out

class OrderedLengthMetric(HandSetMetric):
    def __init__(self, place):
        values = dict()
        for pat, bdd in ShapeMaker.get_pattern_bdds().items():
            x = sorted(pat)[place]
            if x in values:
                values[x] |= bdd
            else:
                values[x] = bdd

        super(OrderedLengthMetric, self).__init__(values)

class lazy_const:
    def __init__(self, factory):
        self.factory = factory
        self.made = False

    def __get__(self, instance, owner):
        if not self.made:
            self.value = self.factory()
            self.made = True
        return self.value

class hand_makers:
    NUM_CL = lazy_const(lambda: SimpleHandMetric({Card("C",r): 1 for r in "AKQJT98765432"}))
    NUM_DI = lazy_const(lambda: SimpleHandMetric({Card("D",r): 1 for r in "AKQJT98765432"}))
    NUM_HE = lazy_const(lambda: SimpleHandMetric({Card("H",r): 1 for r in "AKQJT98765432"}))
    NUM_SP = lazy_const(lambda: SimpleHandMetric({Card("S",r): 1 for r in "AKQJT98765432"}))
    HCP = lazy_const(lambda: SimpleHandMetric({Card(s,r): v for s in "SHDC" for r, v in [('A',4), ('K',3), ('Q',2), ('J',1)]}))
    RP = lazy_const(lambda: SimpleHandMetric({Card(s,r): v for s in "SHDC" for r, v in [('A',3), ('K',2), ('Q',1)]}))
    """ Royal Points; 3 for an ace, 2 for a king, 1 for a queen """
    CLUBS = NUM_CL
    DIAMONDS = NUM_DI
    HEARTS = NUM_HE
    SPADES = NUM_SP
    ACES = lazy_const(lambda: SimpleHandMetric({Card(s,"A"): 1 for s in "SHDC"}))
    KINGS = lazy_const(lambda: SimpleHandMetric({Card(s,"K"): 1 for s in "SHDC"}))
    QUEENS = lazy_const(lambda: SimpleHandMetric({Card(s,"Q"): 1 for s in "SHDC"}))
    JACKS = lazy_const(lambda: SimpleHandMetric({Card(s,"J"): 1 for s in "SHDC"}))
    TENS = lazy_const(lambda: SimpleHandMetric({Card(s,"T"): 1 for s in "SHDC"}))
    TOP2 = lazy_const(lambda: SimpleHandMetric({Card(s,r): 1 for s in "SHDC" for r in "AK"}))
    TOP3 = lazy_const(lambda: SimpleHandMetric({Card(s,r): 1 for s in "SHDC" for r in "AKQ"}))
    TOP4 = lazy_const(lambda: SimpleHandMetric({Card(s,r): 1 for s in "SHDC" for r in "AKQJ"}))
    TOP5 = lazy_const(lambda: SimpleHandMetric({Card(s,r): 1 for s in "SHDC" for r in "AKQJT"}))
    CONTROLS = lazy_const(lambda: SimpleHandMetric({Card(s,r): v for s in "SHDC" for r, v in [('A',2), ('K',1)]}))
    """ 2 for ace, 1 for king """

    @staticmethod
    def CARD(card):
        index = SimpleHandMetric.card_index[Card(card)]
        return HandSet(BDD(index))
    """ example CARD("SK") returns whether hand holds the spade king """

    @staticmethod
    def IN_SUIT(suit):
        """ IN_SUIT("S") is equvialent to SPADES """
        return {
            "S":hand_makers.NUM_SP,
            "H":hand_makers.NUM_HE,
            "D":hand_makers.NUM_DI,
            "C":hand_makers.NUM_CL,
        }[suit]

    @staticmethod
    def CONTAINS(thing):
        """ Either a Hand or a PartialHand or a string representation """
        ph = PartialHand(thing)
        if ph.cards:
            return functools.reduce(operator.__and__, map(hand_makers.CARD, ph.cards))
        else:
            return HandSet(BDD.true())

    QUICKx2 = lazy_const(lambda: QuickTricksMetric())
    """ 4 for AK, 3 for AQ, 2 for A or KQ, 1 for Kx """

    @staticmethod
    def SHAPE(spec):
        """ takes a string describing shape
"4432" means 4 spades, 4 hearts, 3 diamonds, 2 clubs
"any 4432" means any two suits with 4, any one suit with 3, last suit with 2.
"44xx" means 4 spades, 4 hearts, any number of diamonds and clubs
"4432 + 4333" means either of those shapes
"44xx - 4450" means any shape with 4 spades and 4 hearts and any number of diamonds other than 5.
        """
        return ShapeMaker.get_handset(spec)

    @staticmethod
    def AT_LEAST(suit, spec):
        """ AT_LEAST("S", ["Ax","Qxx"]) for example means:
a spade suit with at least 2 cards, one of which is the ace
OR
a spade suit with at least 3 cards, one of which is the queen, king, or ace.
        """
        def one(suit, spec):
            if not isinstance(spec, str):
                raise TypeError("Expecting a string here")
            key = {k:i for i, k in enumerate("AKQJT98765432x")}
            spec = sorted(spec, key=lambda x: key[x])

            cards = [(i, card) for i, card in enumerate(SimpleHandMetric.cards) if card.suit == suit]
            assert len(cards) == 13, (len(cards), cards)

            states = [BDD.false()]*len(spec) + [BDD.true()]
            for var, card in reversed(cards):
                new_states = list(states)
                for i, req in enumerate(spec):
                    if key[req] >= key[card[1]]:
                        new_states[i] |= \
                            BDD(var).thenelse(new_states[i+1], new_states[i])
                states = new_states
            return HandSet(states[0])

        if isinstance(spec, str):
            return one(suit, spec)
        else:
            return functools.reduce(HandSet.__or__, [one(suit, x) for x in spec])
    LONGEST = lazy_const(lambda: OrderedLengthMetric(3))
    SECOND_LONGEST = lazy_const(lambda: OrderedLengthMetric(2))
    SHORTEST = lazy_const(lambda: OrderedLengthMetric(0))
    ANY = lazy_const(lambda: HandSet(BDD.true()))

    NORTH = DealSetConverter("N")
    SOUTH = DealSetConverter("S")
    EAST = DealSetConverter("E")
    WEST = DealSetConverter("W")


class HandSet:
    NUM_CARDS = SimpleHandMetric({c: 1 for c in SimpleHandMetric.cards})
    HAND = NUM_CARDS.values[13]

    def __init__(self, bdd):
        self.bdd = bdd & HandSet.HAND

    def sample(self, rng=random):
        index = rng.randrange(self.bdd.pcount())
        return Hand([SimpleHandMetric.cards[i] for i in HandSet.get_index(self.bdd, index)])

    def count(self):
        return self.bdd.pcount()

    @staticmethod
    def get_index(bdd, index):
        return bdd.get_pindex(index)

    def __and__(self, other):
        return HandSet(self.bdd & other.bdd)
    def __or__(self, other):
        return HandSet(self.bdd | other.bdd)
    def __xor___(self, other):
        return HandSet(self.bdd ^ other.bdd)
    def __invert__(self):
        return HandSet(~self.bdd)
    def ite(self, t, e):
        return HandSet(self.bdd.thenelse(t.bdd, e.bdd))

    def contains(self, hand):
        if isinstance(hand, str):
            hand = Hand(hand)
        elif not isinstance(hand, Hand):
            raise TypeError("Only Hand type handled")

        cur = self.bdd
        for i, card in enumerate(SimpleHandMetric.cards):
            if not cur:
                return False
            var, pos, neg = cur.split()
            assert var == i
            if card in hand.cards:
                cur = pos
            else:
                cur = neg
        return bool(cur)


if __name__ == "__main__":
    HandMakers = hand_makers()

    s5 = (HandMakers.NUM_SP >= 5)
    h12 = (HandMakers.HCP >= 12)
    a = s5 & h12 & HandMakers.HAND

    hand1 = Hand("AKQJT/5432/Q2/52")
    hand3 = Hand("AKQJ/T5432/Q2/52")
    hand4 = Hand("AKQJT/5432/42/52")

    assert a.contains(hand1)
    assert not a.contains(hand3)
    assert not a.contains(hand4)
    print(a.sample())

    b = HandMakers.SHAPE("4432 + 4333 + 5332")
    c = HandMakers.SHAPE("4432 + any 4333 + 5332")
    d = HandMakers.SHAPE("4432 + any 4333 + 5332 - 3xxx")

    hand5 = Hand("A763/K492/J72/Q3")
    assert d.contains(hand5)
    assert (HandMakers.QUICKx2 == 3).contains(hand5)

    assert HandMakers.AT_LEAST("S", "Kx").contains(hand5)
    assert not HandMakers.AT_LEAST("C", "Kx").contains(hand5)
    assert HandMakers.AT_LEAST("D", "Jxx").contains(hand5)
    assert HandMakers.AT_LEAST("D", ["Kx","Jxx"]).contains(hand5)
    

    if False:
        ds = HandMakers.NORTH(a)
        for _ in range(5):
            print("------")
            print(ds.sample().square_string())

__all__ = ["hand_makers", "DealSet"]
