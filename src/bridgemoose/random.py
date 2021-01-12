import random
from .deal import Card, Deal, Hand
from .direction import Direction
from .play import PartialHand

def parse_card_set(s):
    out = set()

    by_suit = s.split("/")
    if len(by_suit) != 4:
        raise ValueError("Bad hand specifier: '%s' needs 3 '/'s" % (s))

    for suit, ranks in zip("SHDC", by_suit):
        for rank in ranks:
            out.add(Card(suit, rank))

    if len(out) > 13:
        raise ValueError("More than 13 cards: '%s'" % (s))
    return out

class RestrictedDealer:
    def __init__(self, west=None, north=None, east=None, south=None, deal_accept=None, rng=None):
        if rng is None:
            self.rng = random
            self.sorting = False
        else:
            self.sorting = True

        self.cardset = set(Card.all())
        self.acceptors = dict()
        self.known_cards = {d: set() for d in Direction.ALL}
        self.deal_accept = deal_accept

        for d, spec in zip("WNES", [west, north, east, south]):
            if spec is None:
                continue
            if isinstance(spec, str):
                hand = Hand(spec)
                self._process_card_set(d, hand.cards)
            elif isinstance(spec, (Hand, PartialHand)):
                self._process_card_set(d, spec.cards)
            elif callable(spec):
                self.acceptors[d] = spec
            else:
                self._process_card_set(d, set(spec))

    def one_try(self):
        # Must sort when
        cardlist = list(self.cardset)
        if self.sorting:
            self.cardlist.sort()
        self.rng.shuffle(cardlist)
        n = 0

        hands = {}
        for key in Direction.ALL:
            known = self.known_cards[key]
            k = 13 - len(known)
            unknown = set(cardlist[n:n+k])
            hands[key] = Hand(known | unknown)
            n += k

        assert n == len(cardlist), (n, len(cardlist))

        for d, acc in self.acceptors.items():
            if not acc(hands[d]):
                return None

        deal = Deal(hands['W'],hands['N'],hands['E'],hands['S'])
        if self.deal_accept is not None:
            if not self.deal_accept(deal):
                return None
        return deal

    def _process_card_set(self, d, cset):
        if len(cset) != 13:
            raise ValueError("Require exactly 13 cards for %s" % (d,))
        for x in cset:
            if not isinstance(x, Card):
                raise TypeError("Not a Card: %s" % (x))
            if not x in self.cardset:
                raise ValueError("Card used twice: %s" % (x))
            self.cardset.remove(x)
        self.known_cards[d] = cset

def random_deals(count, west=None, north=None, east=None, south=None,
    deal_accept=None, rng=None, fail_count=100000):
    """\
Class to generate random deals.
count       : stop generation after this many successes [None = infinite]
rng         : an instance of random.Random()
fail_count  : stop generation after this many failures in the accept clause
    [None = infinite]
west, north, east, south: can each be one of:
    None - no restrictions on the hand
    str - parsed as a Hand
    Hand, PartialHand - use the 13 cards from the object
    list, set - assumes 13 Card objects, sets exact value
    callable - a function which takes a Hand and returns True/False\
deal_accept  : can be None or a callable which takes a Deal and returns True/False\
"""

    misses = 0
    hits = 0
    dealer = RestrictedDealer(west, north, east, south, deal_accept, rng)
    while count is None or hits < count:
        d = dealer.one_try()
        if d is None:
            misses += 1
            if hits == 0 and fail_count is not None and misses >= fail_count:
                raise ValueError("No hits found - is your clause possible?")
        else:
            hits += 1
            yield d

__all__ = ["random_deals"]
