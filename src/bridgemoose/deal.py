import collections
import itertools
from .direction import *
from .card import Card

class Hand:
    """
    An object of this class represents the 13 cards held by a single player,
with the following useful analytics defined:

* nc, nd, nh, ns:  number of clubs, diamonds, hearts, spades 
* C, D, H, S:      list of card ranks by suit
* hcp:             high card points
* rp:              royal points (A=3, K=2, Q=1)
* control_points:  2 for an ace, 1 for a king
* lsp:             long suit points (+1 for each card past 4)
* ssp:             short suit points (1 doubleton, 2 singleton, 3 void)
* count:           map from suit to length.  count["C"] is the same as nc.
* rank_count:      map from rank (e.g. '2' or 'J') to an integer 0,1,2,3,4
* by_suit:         map from suit to a string of ranks.
* shape:           a tuple of 4 ints from high to low (e.g. (5,3,3,2)
* pattern:         a tuple of 4 ints in S-H-D-C order
* cards:           a set of Card objects
    """

    HCP_MAP = {r:0 for r in Card.RANKS}
    HCP_MAP['J'] = 1
    HCP_MAP['Q'] = 2
    HCP_MAP['K'] = 3
    HCP_MAP['A'] = 4

    RP_MAP = {r:0 for r in Card.RANKS}
    RP_MAP['Q'] = 1
    RP_MAP['K'] = 2
    RP_MAP['A'] = 3

    def __init__(self, cards):
        if type(cards) is str:
            self.init_for_string(cards)
        elif isinstance(cards, (set, frozenset, list)):
            self.init_for_iterable(cards)
        elif type(cards) is Hand:
            self.init_for_iterable(cards.cards)
        else:
            raise TypeError("bad type: %s for" % (type(cards)), cards)

    def init_for_string(self, thestr):
        thecards = set()
        bs = thestr.split('/')
        assert len(bs) == 4
        for i, s in enumerate("SHDC"):
            if bs[i] == '-':
                continue
            for ch in bs[i]:
                assert ch in Card.RANKS
                thecards.add(Card(s, ch))
        assert len(thecards) == 13
        self.init_for_iterable(thecards)

    def init_for_iterable(self, cards):
        raw_by_suit = {suit:"" for suit in Card.SUITS}
        self.cards = frozenset(Card(c) for c in cards)
        self.hcp = 0
        self.rp = 0
        self.control_points = 0
        self.rank_count = {r:0 for r in Card.RANKS}

        for c in self.cards:
            raw_by_suit[c.suit] += c.rank
            self.hcp += Hand.HCP_MAP[c.rank]
            self.rp += Hand.RP_MAP[c.rank]
            self.rank_count[c.rank] += 1
            if c.rank == "A":
                self.control_points += 2
            elif c.rank == "K":
                self.control_points += 1

        self.by_suit = {k:Card.hi_lo_order_ranks(v) for k, v in raw_by_suit.items()}
        self.count = {k:len(v) for k, v in raw_by_suit.items()}

        self.shape = tuple(sorted(self.count.values(), reverse=True))
        self.pattern = tuple([self.count[x] for x in "SHDC"])

        self.nc = self.count['C']
        self.nd = self.count['D']
        self.nh = self.count['H']
        self.ns = self.count['S']
        self.C = self.by_suit['C']
        self.D = self.by_suit['D']
        self.H = self.by_suit['H']
        self.S = self.by_suit['S']

        self.qt = 0
        self.lsp = 0
        self.ssp = 0


        for s in Card.SUITS:
            b = self.by_suit[s]
            if len(b) < 3:
                self.ssp += 3 - len(b)
            elif len(b) > 4:
                self.lsp += len(b) - 4

            if len(b) == 1:
                if b[0] == 'A':
                    self.qt += 1
            elif len(b) >= 2:
                if b[0] == 'A':
                    if b[1] == 'K':
                        self.qt += 2
                    elif b[1] == 'Q':
                        self.qt += 1.5
                    else:
                        self.qt += 1
                elif b[0] == 'K':
                    if b[1] == 'Q':
                        self.qt += 1
                    else:
                        self.qt += 0.5

    def str_for_suit(self, suit, void_symbol="-"):
        b = self.by_suit[suit]
        if len(b) == 0:
            return void_symbol
        else:
            return b

    def __eq__(self, other):
        return self.cards == other.cards

    def __hash__(self):
        return hash(self.cards)

    def __str__(self):
        return "/".join([self.str_for_suit(s) for s in "SHDC"])

    def __repr__(self):
        return f"bridgemoose.Hand('{str(self)}')"

    def lin_string(self):
        return "".join([s+self.str_for_suit(s,"") for s in "SHDC"])

    def square_string(self, suit_symbols="CDHS"):
        return  "\n".join(["%s %s" % (symbol, self.str_for_suit(suit)) for 
            suit, symbol in reversed(list(zip(Card.SUITS, suit_symbols)))])


def two_hands_square_string(left, right, suit_symbols="CDHS"):
    """ Takes as input two bridge hands returns a string looking like:
S KT93          S AQJ75
H J2            H T3
D J874          D T2
C Q53           C KT64
    """
    out = ""
    rss = list(reversed(suit_symbols))
    for suit, symbol in zip("SHDC",rss):
        out += "%s %-13s %s %-13s\n" % (symbol, left.str_for_suit(suit), symbol, right.str_for_suit(suit))
    return out

def fourth_hand(h1, h2, h3):
    h1 = Hand(h1)
    h2 = Hand(h2)
    h3 = Hand(h3)
    ac = set(Card.all())
    ac -= h1.cards
    ac -= h2.cards
    ac -= h3.cards
    return Hand(ac)


class Deal:
    """ Class representing four bridge hands """
    def __init__(self, W, N, E, S):
        nones = sum(x is None for x in (W,N,E,S))
        if nones == 1:
            if W is None:
                W = fourth_hand(N,E,S)
            elif N is None:
                N = fourth_hand(W,E,S)
            elif E is None:
                E = fourth_hand(W,N,S)
            else:
                S = fourth_hand(W,N,E) 
        elif nones != 0:
            raise ValueError("Too many Nones")

        self.W = W
        self.N = N
        self.E = E
        self.S = S

    def hand(self, side):
        if isinstance(side, Direction):
            side = str(side)
        if not side in "WNES":
            raise KeyError(side)
        return getattr(self, side)

    def square_string(self, played_cards=set()):
        def my_str(hand, suit):
            ranks = hand.str_for_suit(suit)
            out = ""
            later = ""
            for rank in ranks:
                if Card(suit, rank) in played_cards:
                    later += rank
                else:
                    out += rank
            if later:
                out += "[%s]" % (later,)
            return out

        out = ""
        for suit in "SHDC":
            out += "%8s%s %s\n" % ("", suit, my_str(self.N, suit))
        for suit in "SHDC":
            out += "%s %-13s %s %-13s\n" % (suit, my_str(self.W, suit),
                suit, my_str(self.E, suit))
        for suit in "SHDC":
            out += "%8s%s %s\n" % ("", suit, my_str(self.S, suit))
        return out

    def fancy_square_string(self, played_cards=None):
        if played_cards is None:
            played_cards = set()
        else:
            played_cards = set(played_cards)

        out = ""
        SYMS = {
            "S": "\033[34m\u2660\033[0m",
            "H": "\033[31m\u2665\033[0m",
            "D": "\033[31;1m\u2666\033[0m",
            "C": "\033[32m\u2663\033[0m",
        }

        def prep_suit(hand, suit):
            out = "%s " % (SYMS[suit],)
            bold_mode = False
            ranks = hand.str_for_suit(suit)
            for rank in ranks:
                card = Card(suit, rank)
                if card in played_cards and bold_mode:
                    out += "\033[0m"
                    bold_mode = False
                elif not card in played_cards and not bold_mode:
                    out += "\033[1m"
                    bold_mode = True
                out += rank
            if bold_mode:
                out += "\033[0m"
            return out, len(ranks)

        fancy_out = ""
        for suit in "SHDC":
            fancy_out += "%8s %s\n" % ("", prep_suit(self.N, suit)[0])

        for suit in "SHDC":
            pw, nw = prep_suit(self.W, suit)
            pe, _  = prep_suit(self.E, suit)
            fancy_out += "%s %*s %s\n" % (pw, 13-nw, "", pe)

        for suit in "SHDC":
            fancy_out += "%8s %s\n" % ("", prep_suit(self.S, suit)[0])

        return fancy_out

    def lin_string(self, /, board_num=None, set_num=0, dealer="S", vuln="-"):
        # sv tag -> "o", "n", "e", "b"
        # md tag -> 2 = West.  Start w/ South "SQ9543HK82DQT4CJ5," example.
        if board_num is not None:
            dealer, vuln = board_number_to_dealer_vuln(board_num)
        else:
            dealer = Direction(dealer)
            vuln = Vuln(vuln)
            board_num = dealer_vuln_to_board_number(dealer, vuln)

        return "st||pn|,,,|md|%d%s,%s,%s,%s|sv|%s|rh||ah|Board %d|" % (
            (dealer.i+2)%4, self.S.lin_string(),
            self.W.lin_string(), self.N.lin_string(),
            self.E.lin_string(), "oneb"[vuln.data], board_num + 16*set_num)

    def __getitem__(self, index):
        if isinstance(index, Direction):
            index = index.i
        elif isinstance(index, str):
            index = Direction.ALL.index(index)
        elif not isinstance(index, int):
            raise TypeError("Unhandled index:", index)

        return [self.W, self.N, self.E, self.S][index]

    def __iter__(self):
        yield from [self.W, self.N, self.E, self.S]

    def __eq__(self, other):
        if not isinstance(other, Deal):
            return False
        return all(self[d] == other[d] for d in Direction.ALL)

    def __hash__(self):
        return hash((self.W, self.N, self.E, self.S))


__all__ = ["Deal", "Hand",
    "two_hands_square_string", "fourth_hand"]
