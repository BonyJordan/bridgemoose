import re
from .direction import Direction

class Contract:
    RE = re.compile("([1-7])([CcDdHhSsNn]|NT|nt)([xX*]{0,2})$")

    def __init__(self, spec):
        if isinstance(spec, (Contract, DeclaredContract)):
            self.level = spec.level
            self.tricks_needed = spec.tricks_needed
            self.strain = spec.strain
            self.double_state = spec.double_state
            return
        elif isinstance(spec, Bid):
            self.level = spec.level
            self.strain = spec.strain
            self.tricks_needed = 6 + self.level
            self.double_state = 0
            return
        elif not isinstance(spec, str):
            raise TypeError("str, Bid, Contract, DeclaredContract allowed")

        mo = Contract.RE.match(spec)
        if not mo:
            raise ValueError("Bad contract \"" + spec + "\"")

        self.level = int(mo.group(1))
        self.tricks_needed = 6 + self.level
        self.strain = mo.group(2)
        self.double_state = len(mo.group(3))

    def __repr__(self):
        return "%d%s%s" % (self.level, self.strain, "xx"[:self.double_state])

    def __eq__(self, other):
        o = Contract(other)
        return self.level == o.level and self.strain == o.strain and self.double_state == o.double_state

    def __hash__(self):
        return hash((self.level, self.strain, self.double_state))

class DeclaredContract:
    RE = re.compile("([1-7])([CcDdHhSsNn]|NT|nt)([x*]{0,2})-([NEWS])$")

    def __init__(self, *args):
        if len(args) == 4:
            level, strain, double_state, declarer = args
            if level < 1 or level > 7:
                raise ValueError("bad level")
            if double_state < 0 or double_state > 2:
                raise ValueError("bad double state")
            if strain not in ("C","D","H","S","N","NT"):
                raise ValueError("bad strain")

            self.level = level
            self.tricks_needed = 6 + level
            self.strain = strain
            self.double_state = double_state
            self.declarer = Direction(declarer)
            return
        elif len(args) != 1:
            raise ValueError("I m confused")

        spec = args[0]

        if isinstance(spec, DeclaredContract):
            self.level = spec.level
            self.tricks_needed = spec.tricks_needed
            self.strain = spec.strain
            self.double_state = spec.double_state
            self.declarer = spec.declarer
            return

        mo = DeclaredContract.RE.match(spec)
        if not mo:
            raise ValueError("Bad contract \"" + spec + "\"")

        self.level = int(mo.group(1))
        self.tricks_needed = 6 + self.level
        self.strain = mo.group(2)
        self.double_state = len(mo.group(3))
        self.declarer = Direction(mo.group(4))

    def __repr__(self):
        return "%d%s%s-%s" % (self.level, self.strain, "xx"[:self.double_state], self.declarer)

    def __eq__(self, other):
        o = DeclaredContract(other)
        return self.level == o.level and self.strain == o.strain and self.double_state == o.double_state and self.declarer == o.declarer

    def __hash__(self):
        return hash((self.level, self.strain, self.double_state, self.declarer))

class Bid:
    STRAINS = ["C","D","H","S","NT"]

    def __init__(self, *args):
        if len(args) == 1 and type(args[0]) == str:
            self.level = int(args[0][0])
            self.strain = args[0][1:].upper()
        elif len(args) == 2:
            self.level = int(args[0])
            self.strain = args[1].upper()
        else:
            raise TypeError()

        if self.level < 1 or self.level > 7:
            raise TypeError("Bad level")
        if self.strain == "N":
            self.strain = "NT"
        if not self.strain in Bid.STRAINS:
            raise TypeError("Bad strain")

    def step(self):
        return self.level * 5 + Bid.STRAINS.index(self.strain) - 5

    def cmp(self, other):
        if not isinstance(other, Bid):
            raise TypeError()
        return self.step() - other.step()

    @staticmethod
    def all_bids():
        cur = Bid("1C")
        top = Bid("7N")
        while True:
            yield cur
            if cur == top:
                break
            cur = cur + 1

    def all_above(self):
        """ A generator iterating over all bids above this one """
        cur = self
        top = Bid("7N")
        while cur < top:
            cur = cur + 1
            yield cur

    def __lt__(self, other):
        return self.cmp(other) < 0
    def __le__(self, other):
        return self.cmp(other) <= 0
    def __gt__(self, other):
        return self.cmp(other) > 0
    def __ge__(self, other):
        return self.cmp(other) >= 0
    def __eq__(self, other):
        return self.cmp(other) == 0
    def __ne__(self, other):
        return self.cmp(other) != 0
    def __hash__(self):
        return hash((self.level, self.strain))

    def __add__(self, other):
        if not isinstance(other, int):
            raise TypeError()
        n = self.step() + other
        if n < 0 or n >= 35:
            raise ValueError("Out of Bounds")
        return Bid(n//5 + 1, Bid.STRAINS[n%5])

    def __sub__(self, other):
        if isinstance(other, int):
            n = self.step() - other
            if n < 0 or n >= 35:
                raise ValueError("Out of Bounds")
            return Bid(n//5 + 1, Bid.STRAINS[n%5])
        elif isinstance(other, Bid):
            return self.step() - other.step()
        else:
            raise TypeError()

    def __str__(self):
        return "%d%s" % (self.level, self.strain)

def auction_next_to_call(dealer, auction):
    """\
dealer is a string W, N, E, S or a bridgemoose.Direction,
auction is a string of comma separated calls.

output is a bridgemoose.Direction or None if the auction is over.
    """
    turn = Direction(dealer)
    num_passes = 0
    any_bids = False
    for call in auction.split(","):
        if num_passes == 4:
            return ValueError("Action after 4 passes")
        if num_passes == 3 and any_bids:
            return ValueError("Action after bidding then 3 passes")

        if call in ["P", "p", "Pass", "PASS"]:
            num_passes += 1
            turn += 1
            continue
        elif call in ["D", "Dbl", "X", "Double", "R", "Redouble", "XX"]:
            num_passes = 0
            turn += 1
            continue

        num_passes = 0
        any_bids = True
        turn += 1
        continue

    if num_passes == 4 or num_passes == 3 and any_bids:
        return None
    else:
        return turn

def auction_to_contract(dealer, auction):
    """\
dealer is a string W, N, E, S or a bridgemoose.Direction,
auction is a string of comma separated calls.

output is a final contract <level><strain><double>"-"<dir>.  Example "3NTx-W"
    """
    turn = Direction(dealer)
    last_bid = None
    last_bid_dir = None
    first_bid_strain = dict()
    num_passes = 0
    double_state = 0

    for call in auction.split(","):
        if num_passes == 4:
            return ValueError("Action after 4 passes")
        if num_passes == 3 and last_bid is not None:
            return ValueError("Action after bidding then 3 passes")

        if call in ["P", "p", "Pass", "PASS"]:
            num_passes += 1
            turn += 1
            continue
        if call in ["D", "Dbl", "X", "Double"]:
            if last_bid is None:
                return ValueError("Cannot Double before a bid")
            if last_bid_dir.same_side(turn):
                return ValueError("Cannot Double my own side", last_bid_dir, turn)
            if double_state != 0:
                return ValueError("Cannot Double after double")

            num_passes = 0
            double_state = 1
            turn += 1
            continue
        if call in ["R", "Redouble", "XX"]:
            if double_state != 1:
                return ValueError("Cannot Redouble until Doubled!")
            if last_bid_dir.opp_side(turn):
                return ValueError("Cannot Redouble ourselves!")

            num_passes = 0
            double_state = 2
            turn += 1
            continue

        bid = Bid(call)
        if last_bid is not None:
            if bid <= last_bid:
                return ValueError("Next bid not after previous bid!")

        key = (turn.dir_pair(), bid.strain)
        if not key in first_bid_strain:
            first_bid_strain[key] = turn

        num_passes = 0
        double_state = 0
        last_bid = bid
        last_bid_dir = turn
        turn += 1
        continue

    key = (last_bid_dir.dir_pair(), last_bid.strain)
    return DeclaredContract(last_bid.level, last_bid.strain, double_state,
        first_bid_strain[key])

__all__ = [
    "Bid",
    "Contract",
    "DeclaredContract",
    "auction_next_to_call",
    "auction_to_contract",
]
