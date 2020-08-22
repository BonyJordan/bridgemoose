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

    @staticmethod
    def all_dirs():
        return list([Direction(i) for i in range(4)])

Direction.EAST = Direction("E")
Direction.WEST = Direction("W")
Direction.NORTH = Direction("N")
Direction.SOUTH = Direction("S")

__all__ = ["Direction"]
