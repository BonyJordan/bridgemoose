import collections
import operator


class Card(collections.namedtuple("Card", "suit rank")):
    SUITS = "CDHS"
    RANKS = "23456789TJQKA"
    RANK_ORDER = {x:i for i,x in enumerate(RANKS)}
    SUIT_ORDER = {x:i for i,x in enumerate(SUITS)}

    @property
    def suit_index(self):
        return Card.SUIT_ORDER[self.suit]

    @property
    def rank_index(self):
        return Card.RANK_ORDER[self.rank]

    @staticmethod
    def rank_order(r):
        return Card.RANK_ORDER[r]

    SUIT_UNICODE = {'C': u'\u2663', 'D': u'\u2666', 'H': u'\u2665', 'S': u'\u2660'}
    SUIT_HTML = {'C': '&clubs;', 'D':'<FONT color="red">&diams;</FONT>', 'H':'<FONT color="red">&hearts;</FONT>', 'S':'&spades;'}

    def __new__(self, *args):
        if len(args) == 2:
            suit, rank = args
        elif len(args) == 1:
            item = args[0]
            if isinstance(item, Card):
                return item
            elif isinstance(item, str):
                if len(item) != 2:
                    return ValueError("Bad card '%s'" % (item))
                suit, rank = item
            else:
                raise TypeError(item)
        else:
            raise ValueError("Bad arguments to Card")

        if not suit in Card.SUITS:
            return ValueError("Bad suit '%s'" % (suit))
        if not rank in Card.RANKS:
            return ValueError("Bad rank '%s'" % (rank))

        return tuple.__new__(Card, (suit, rank))

    def __str__(self):
        return self.suit + self.rank

    def mk_cmp(op):
        def fn(self, other):
            if self.suit == other.suit:
                return op(Card.RANK_ORDER[self.rank], Card.RANK_ORDER[other.rank])
            else:
                return op(Card.SUIT_ORDER[self.suit], Card.SUIT_ORDER[other.suit])
        return fn

    __ge__ = mk_cmp(operator.ge)
    __gt__ = mk_cmp(operator.gt)
    __le__ = mk_cmp(operator.le)
    __lt__ = mk_cmp(operator.lt)

    __repr__ = __str__

    @staticmethod
    def hi_lo_order_ranks(ranks):
        return "".join(sorted(ranks, key=lambda r:Card.RANK_ORDER[r], reverse=True))

    @staticmethod
    def all():
        return sum([[Card(s,r) for s in Card.SUITS] for r in Card.RANKS], [])

def cmp_rank(r1, r2):
    """
r1 and r2 are single character strings representing a rank.  Return
a positive number if r1>r2, 0 if r1==r2, negative if r1<r2
    """
    return Card.RANK_ORDER[r1] - Card.RANK_ORDER[r2]


def suit_as_good_as(suit, template):
    """\
Takes two suits as string in rank order (e.g. "AQ95") and determines whether
the suit represented by the first argument (suit) is at least as good as
the suit represented by the second argument.  To be at least as good, suit
must be at least as long as template, and each card in each position must
be of equal or better rank.

For example, KT3 is better than K92, but not better than KJ2 or QJ2 or Q432.
If template is a list or tuple, return whether suit is better than any of
them.  (For example ["QTxx", "Kxxx"]).

You might define a suit as having a NT stopper as, for example,
suit_as_good_as(suit, ["A", "Kx", "QTx", "JTxx", "J98x", "xxxxx"])
if that suits your judgement.
    """
    def check_one(suit, single):
        if len(suit) < len(single):
            return False
        for a, b in zip(suit, single):
            if b == 'x':
                return True
            elif cmp_rank(a, b) < 0:
                return False
        return True

    if isinstance(template, str):
        return check_one(suit, template)
    else:
        for single in template:
            if check_one(suit, single):
                return True
        return False

def bit_pack(cards):
    """ Take an iterable of cards and return a 52 bit integer representing
        the bitwise or of (1 << (suit_index*13 + rank_index))
        where clubs has suit_index=0 and the deuce has rank_index=0 """
    out = 0
    for card in cards:
        card = Card(card)
        out |= 1 << (Card.SUIT_ORDER[card.suit]*13 + Card.RANK_ORDER[card.rank])
    return out

def bit_unpack(bits):
    """ Take a 52 bit integer and output a list of cards, where a 1 bit in
        position (suit_index*13 + rank_index) implies the existence of that
        card.  clubs has suit_index=0 and the deuce has rank_index=0 """
    out = []
    for si,suit in enumerate(Card.SUITS):
        for ri,rank in enumerate(Card.RANKS):
            bit = 1 << (si*13 + ri)
            if bit & bits:
                out.append(Card(suit+rank))
    return out




__all__ = ["Card", "cmp_rank", "suit_as_good_as", "bit_pack", "bit_unpack"]
