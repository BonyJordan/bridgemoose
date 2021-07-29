class Direction:
    ALL = "WNES"

    def __init__(self, name):
        if isinstance(name, str):
            self.i = Direction.ALL.index(name)
        elif isinstance(name, int):
            self.i = name
        elif isinstance(name, Direction):
            self.i = name.i
        else:
            raise TypeError()

    def __add__(self, other):
        if isinstance(other, int):
            return Direction( (self.i + other) & 3)
        else:
            raise TypeError()

    def __sub__(self, other):
        if isinstance(other, int):
            return Direction( (self.i - other) & 3)
        else:
            raise TypeError()

    def __str__(self):
        return Direction.ALL[self.i]
    def __repr__(self):
        return "Direction(%s)" % (Direction.ALL[self.i],)

    def __eq__(self, other):
        return isinstance(other, Direction) and self.i == other.i
    def __hash__(self):
        return hash(self.i)

    def same_side(self, other):
        other = Direction(other)
        return (self.i & 1) == (other.i & 1)

    def opp_side(self, other):
        other = Direction(other)
        return (self.i & 1) != (other.i & 1)

    def dir_pair(self):
        return ["EW", "NS", "EW", "NS"][self.i]

    def is_ew(self):
        return self.i & 1 == 0

    def is_ns(self):
        return self.i & 1 == 1

    def side_index(self):
        return self.i & 1

    @staticmethod
    def all_dirs():
        return list([Direction(i) for i in range(4)])

Direction.EAST = Direction("E")
Direction.WEST = Direction("W")
Direction.NORTH = Direction("N")
Direction.SOUTH = Direction("S")

class Vuln:
    EW_BIT = 2
    NS_BIT = 1

    def __init__(self, data):
        if isinstance(data, int):
            if data < 0 or data > 3:
                raise ValueError("Ints from 0 to 3 only")
            else:
                self.data = data
        elif isinstance(data, str):
            m = {"-": 0,
                 "o": 0,
                 "none": 0,
                 "e": Vuln.EW_BIT,
                 "ew": Vuln.EW_BIT,
                 "n": Vuln.NS_BIT,
                 "ns": Vuln.NS_BIT,
                 "b": Vuln.EW_BIT | Vuln.NS_BIT,
                 "both": Vuln.EW_BIT | Vuln.NS_BIT}
            if data in m:
                self.data = m[data]
            else:
                raise ValueError("Strings are from -,e,n,b")
        elif isinstance(data, Vuln):
            self.data = data.data
        else:
            raise TypeError("Unexpected type")

    def ew_vul(self):
        return (self.data & Vuln.EW_BIT) != 0
    def ns_vul(self):
        return (self.data & Vuln.NS_BIT) != 0
        return (self.data & Vuln.EW_BIT) != 0

    def __str__(self):
        return "-neb"[self.data]
    def __repr__(self):
        return "Vuln(%s)" % (self.__str__())

    def __eq__(self, other):
        return isinstance(other, Vuln) and self.data == other.data
    def __hash__(self):
        return hash(self.data)

    @staticmethod
    def all_vulns():
        return list([Vuln(i) for i in range(4)])


__all__ = ["Direction", "Vuln"]
