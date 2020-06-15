class Direction:
    ALL = "WNES"

    def __init__(self, name):
        if isinstance(name, str):
            self.i = Direction.ALL.index(name)
        elif isinstance(name, int):
            self.i = name
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

    def __eq__(self, other):
        return isinstance(other, Direction) and self.i == other.i
    def __hash__(self):
        return hash(self.i)


__all__ = ["Direction"]
