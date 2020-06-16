import collections
import operator


class Card(collections.namedtuple("Card", "suit rank")):
    SUITS = "CDHS"
    RANKS = "23456789TJQKA"
    RANK_ORDER = {x:i for i,x in enumerate(RANKS)}

    @staticmethod
    def rank_order(r):
        return RANK_ORDER[r]

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

        return tuple.__new__(Card, (args[0], args[1]))

    def __str__(self):
        return self.suit + self.rank

    def mk_cmp(op):
        def fn(self, other):
            return self.suit == other.suit and fn(Card.RANK_ORDER[self.rank], Card.RANK_ORDER[other.rank])
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


__all__ = ["Card", "cmp_rank"]
