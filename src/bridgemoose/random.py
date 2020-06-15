import random
from .deal import Card, Deal, Hand
from .direction import Direction

def one_random_deal(fixed_cards={}, rng=None):
    if rng is None:
        rng = random
        sorting = False
    else:
        sorting = True

    cardset = set(Card.all())
    hands = dict()
    for key, val in fixed_cards.items():
        if isinstance(key, Direction):
            key = str(key)
        if not key in "NSEW":
            raise ValueError("Bad key for fixed_cards")
        if isinstance(val, Hand):
            hands[key] = val
        else:
            hands[key] = Hand(val)

        cardset -= hands[key].cards

    # Must sort when
    cardlist = list(cardset)
    if sorting:
        cardlist.sort()
    rng.shuffle(cardlist)
    n = 0
    for key in Direction.ALL:
        if not key in hands:
            hands[key] = Hand(set(cardlist[n:n+13]))
            n += 13
    assert n == len(cardlist), (n, len(cardlist))
    return Deal(hands['W'],hands['N'],hands['E'],hands['S'])

def random_deals(count, accept=None, fixed_cards={}, rng=None, fail_count=100000):
    """
Class to generate random deals.
count       : stop generation after this many successes
accept      : a function which takes a Deal and returns True or False
fixed_cards : a map direction -> hand string
rng         : an instance of random.Random()
fail_count  : stop generation after this many failures in the accept clause
    """

    misses = 0
    hits = 0
    while (count is None or hits<count) and (fail_count is None or misses<fail_count):
        d = one_random_deal(fixed_cards, rng)
        if accept is None or accept(d):
            hits += 1
            yield d
        else:
            misses += 1

__all__ = ["random_deals"]
