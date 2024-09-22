import re
from .direction import Direction
from functools import total_ordering

@total_ordering
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
        self.strain = mo.group(2)[0].upper()
        self.double_state = len(mo.group(3))

    def __repr__(self):
        return "%d%s%s" % (self.level, self.strain, "xx"[:self.double_state])

    def __lt__(self, other):
        o = Contract(other)
        if self.level < o.level:
            return True
        elif self.level > o.level:
            return False

        ss = "X" if self.strain == "N" else self.strain
        os = "X" if o.strain == "N" else o.strain
        if ss < os:
            return True
        elif ss > os:
            return False

        if self.double_state < o.double_state:
            return True
        else:
            return False

    def is_bid(self):
        return isinstance(self.value, Bid)

    def __eq__(self, other):
        o = Contract(other)
        return self.level == o.level and self.strain == o.strain and self.double_state == o.double_state

    def __hash__(self):
        return hash((self.level, self.strain, self.double_state))


@total_ordering
class DeclaredContract:
    RE = re.compile("([1-7])([CcDdHhSsNn]|NT|nt)([x*]{0,2})-([NEWS])$")

    def __init__(self, *args):
        if len(args) == 4:
            level, strain, double_state, declarer = args
            strain = strain.upper()
            if level < 1 or level > 7:
                raise ValueError("bad level")
            if double_state < 0 or double_state > 2:
                raise ValueError("bad double state")
            if strain not in ("C","D","H","S","N","NT"):
                raise ValueError("bad strain")

            self.level = level
            self.tricks_needed = 6 + level
            self.strain = strain[0]
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
        self.strain = mo.group(2)[0].upper()
        self.double_state = len(mo.group(3))
        self.declarer = Direction(mo.group(4))

    def __repr__(self):
        return "%d%s%s-%s" % (self.level, self.strain, "xx"[:self.double_state], self.declarer)

    def __lt__(self, other):
        o = Contract(other)
        if self.level < o.level:
            return True
        elif self.level > o.level:
            return False

        ss = "X" if self.strain == "N" else self.strain
        os = "X" if o.strain == "N" else o.strain
        if ss < os:
            return True
        elif ss > os:
            return False

        if self.double_state < o.double_state:
            return True
        elif self.double_state > o.double_state:
            return False

        return self.declarer.i < o.declarer.i

    def __eq__(self, other):
        o = DeclaredContract(other)
        return self.level == o.level and self.strain == o.strain and self.double_state == o.double_state and self.declarer == o.declarer

    def __hash__(self):
        return hash((self.level, self.strain, self.double_state, self.declarer))

    def ds(self):
        """ Return a 2 character string with declarer, strain """
        return str(self.declarer) + self.strain

class Bid:
    STRAINS = ["C","D","H","S","N"]

    def __init__(self, *args):
        if len(args) == 1 and type(args[0]) == str:
            self.level = int(args[0][0])
            self.strain = args[0][1:].upper()
        elif len(args) == 1 and type(args[0]) == Bid:
            self.level = args[0].level
            self.strain = args[0].strain
        elif len(args) == 2:
            self.level = int(args[0])
            self.strain = args[1].upper()
        else:
            raise TypeError("Bad Bid")

        if self.level < 1 or self.level > 7:
            raise TypeError("Bad level")
        if self.strain == "NT":
            self.strain = "N"
        if not self.strain in Bid.STRAINS:
            raise TypeError("Bad strain")

    def step(self):
        return self.level * 5 + Bid.STRAINS.index(self.strain) - 5

    def cmp(self, other):
        return self.step() - Bid(other).step()

    def min_bid_strain(self, strain):
        if strain == "NT":
            strain = "N"
        if not strain in "CDHSN":
            raise ValueError("Bad Strain, must be in 'CDHSN'")

        bid = self
        while True:
            bid += 1
            if bid.strain == strain:
                return bid

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

class Call:
    """ Representing the legal calls possible during an auction, which
    are bids, Pass, Double, and Redouble """
    def __init__(self, value):
        if isinstance(value, Call):
            self.kind = value.kind
            self.bid = value.bid
        elif isinstance(value, Bid):
            self.kind = "B"
            self.bid = value
        elif isinstance(value, str):
            self.bid = None
            if value.upper() in ["P", "PASS"]:
                self.kind = "P"
            elif value.upper() in ["X", "D", "DBL", "DOUBLE"]:
                self.kind = "D"
            elif value.upper() in ["XX", "R", "RDBL", "REDBL", "REDOUBLE"]:
                self.kind = "R"
            else:
                self.kind = "B"
                self.bid = Bid(value)
        else:
            raise TypeError(f"Cannot initialize off '{value}'")

    def __eq__(self, other):
        if not isinstance(other, Call):
            other = Call(other)

        return self.kind == other.kind and self.bid == other.bid

    def __hash__(self):
        return hash((self.kind, self.bid))

    def is_bid(self):
        return self.kind == "B"

    def is_pass(self):
        return self.kind == "P"

    def __str__(self):
        return str(self.bid) if self.kind == "B" else self.kind
    __repr__ = __str__


Call.PASS = Call("P")
Call.DOUBLE = Call("D")
Call.REDOUBLE = Call("R")

class Auction:
    def __init__(self, dealer, bids=None):
        """
Create a new Auction object.  Paramater bids can be a comma separated
list of calls.
        """
        self.dealer = dealer
        self.first_strain_calls = dict()
        self.all_calls = []
        self.history = []

        self.next_dir = Direction(dealer)
        self.last_bid = None
        self.last_bid_dir = None
        self.num_doubles = 0
        self.num_passes = 0

        if bids is not None and bids != "":
            for call in bids.split(","):
                self.add_call(call)

    def __iter__(self):
        return iter(self.all_calls)

    def __len__(self):
        return len(self.all_calls)

    def __getitem__(self, idx):
        return self.all_calls[idx]

    def clone(self):
        out = Auction(self.dealer)
        for call in self.all_calls:
            out.add_call(call)
        return out

    def turn(self):
        """ Return which direction is currently to make a call """
        return self.next_dir

    def done(self):
        if self.last_bid is None and self.num_passes == 4:
            return True
        elif self.last_bid is not None and self.num_passes == 3:
            return True
        else:
            return False

    def final_contract(self):
        if not self.done():
            raise TypeError("Auction not finished")

        # ye olde Passe Out
        if self.last_bid is None:
            return None

        key = (self.last_bid_dir.side_index(), self.last_bid.strain)
        dec, bid = self.first_strain_calls[key]
        return DeclaredContract(self.last_bid.level, self.last_bid.strain,
            self.num_doubles, dec)

    def add_call(self, call):
        call = Call(call)

        lc = self.legal_calls()
        if lc is None:
            raise ValueError("Auction is over")
        elif not call in lc:
            print(f"JORDAN: lc={lc}")
            raise ValueError(f"Call '{call}' is not legal here")

        self.all_calls.append(call)
        self.history.append((self.last_bid, self.last_bid_dir, self.num_doubles, self.num_passes))

        cur_dir = self.next_dir
        self.next_dir += 1

        if call.is_pass():
            self.num_passes += 1
            return self

        if call == Call.DOUBLE:
            self.num_passes = 0
            self.num_doubles = 1
            return self

        if call == Call.REDOUBLE:
            self.num_passes = 0
            self.num_doubles = 2
            return self

        # The call was a bid
        assert call.is_bid()
        bid = call.bid
        key = (cur_dir.side_index(), bid.strain)
        if key not in self.first_strain_calls:
            self.first_strain_calls[key] = (cur_dir, bid)

        self.last_bid = bid
        self.last_bid_dir = cur_dir
        self.num_doubles = 0
        self.num_passes = 0

        return self
    add = add_call

    def rem_call(self):
        if len(self.all_calls) == 0:
            raise ValueError("No calls to remove")

        self.next_dir -= 1
        off = self.all_calls.pop()
        self.last_bid, self.last_bid_dir, self.num_doubles, self.num_passes = self.history.pop()

        if off in ["P","X","XX"]:
            return self
        bid = Bid(off)
        key = (self.next_dir.side_index(), bid.strain)
        fdir, fbid = self.first_strain_calls[key]
        if bid == fbid:
            del self.first_strain_calls[key]
        return self

    def legal_calls(self):
        """ Return a list of all legal calls """
        if self.done():
            return None

        out = [Call.PASS]
        if self.last_bid is None:
            out.extend(map(Call,Bid.all_bids()))
        else:
            out.extend(map(Call,self.last_bid.all_above()))
            if self.num_doubles == 0 and self.next_dir.opp_side(self.last_bid_dir):
                out.append(Call.DOUBLE)
            elif self.num_doubles == 1 and self.next_dir.same_side(self.last_bid_dir):
                out.append(Call.REDOUBLE)

        return out

    def min_bid(self):
        """ Return the lowest legal Bid a player can make, or None in the
            case where the auction is already at 7NT """
        if self.last_bid is None:
            return bm.Bid("1C")
        elif self.last_bid.level == 7 and self.last_bid.strain == "N":
            return None
        else:
            return self.last_bid + 1

    def __str__(self):
        return str(self.dealer) + ":" + ",".join(self.all_calls)




def auction_min_new_bid(auction):
    """\
auction is a string of comma separated calls.  Returns a Bid object
representing the lowest legal bid a player may make, or None in the
unlikely case we are at 7nt.
    """
    last_bid = None
    for call in reversed(auction.split(",")):
        if call in ["P", "p", "Pass", "PASS"]:
            continue
        elif call in ["D", "Dbl", "X", "Double", "R", "Redouble", "XX"]:
            continue

        last_bid = Bid(call)
    else:
        return Bid("1C")




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

output is a DeclaredContract object such as "3NTx-W"
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
    "Auction",
    "Bid",
    "Call",
    "Contract",
    "DeclaredContract",
    "auction_next_to_call",
    "auction_to_contract",
]
