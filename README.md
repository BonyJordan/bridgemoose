# BridgeMoose

BridgeMoose is a Python library for analyzing hands in the card game, Bridge,
making heavy use of Bo Haglund and Soren Hein's double-dummy solver code.

## Installation

Use python's installer.
```bash
python setup.py install
```

## Usage

```python
import bridgemoose as bm
from collections import Counter

count = Counter()
for deal in bm.random_deals(100, north=lambda hand: hand.ns >= 7):
    count[dds.solve_deal(deal, "N", "S")] += 1

print("100 Hands with 7+ spades as declarer take this many tricks:", count)
```

## Bugs

For reasons that are mysterious to me, Python's uses one method to clear up
memory, and the C++ library uses a different method, and the result is that
on some systems you get a Segmentation Fault when exiting a program that
used BridgeMoose.  So far as anyone can tell it is harmless.

## Credits

This code would have been impossible without Bo Haglund / Soren Hein's
outstanding open source [double-dummy solver](http://privat.bahnhof.sef/wb758135)

## License

[MIT](https://choosealicense.com/licenses/mit/)
