from collections import defaultdict, namedtuple
import operator
import re

from . import Card, Deal, Direction, Hand
from .scoring import result_score

ShowOut = namedtuple("ShowOut", "dir suit count")

class PartialHand:
    def __init__(self, data):
        if isinstance(data, str):
            self._load_from_string(data)
        elif isinstance(data, Hand):
            self._load_from_cards(data.cards)
        else:
            self._load_from_cards(data)

    def _load_from_string(self, data):
        suits = re.split(r"/|\.", data)
        if len(suits) != 4:
            raise ValueError("Expected 4 suits")

        loc = []
        for i, ranks in enumerate(suits):
            suit = Card.SUITS[3-i]
            for rank in ranks:
                if not rank in Card.RANKS:
                    raise ValueError("Bad rank '%s'" % (rank))
                loc.append(Card(suit, rank))

        self._load_from_cards(loc)

    def _load_from_cards(self, data):
        self.cards = set(data)
        self.by_suit = defaultdict(set)
        for card in data:
            self.by_suit[card.suit].add(card)

    def __len__(self):
        return len(self.cards)

    def __str__(self):
        return "/".join([
            "".join(sorted([c.rank for c in self.by_suit[suit]],
                key=lambda r: Card.RANKS.index(r),
                reverse=True))
            for suit in "SHDC"])

    def op_maker(op):
        def fn(self, other):
            if isinstance(other, Card):
                return PartialHand(op(self.cards, {other}))
            elif isinstance(other, PartialHand):
                return PartialHand(op(self.cards, other.cards))
            elif isinstance(other, str):
                if len(other) == 2:
                    return PartialHand(op(self.cards, {Card(other)}))
                else:
                    return PartialHand(op(self.cards, {PartialHand(other)}))
            elif isinstance(other, set):
                return PartialHand(op(self.cards, other))
            else:
                raise TypeError(other)
        return fn

    __sub__ = op_maker(operator.sub)
    __add__ = op_maker(operator.__or__)


class PlayDeal:
    def __init__(self, deal, declarer, contract, vulnerable):
        self.hands_left = [PartialHand(h) for h in [deal.W, deal.N, deal.E, deal.S]]
        self.hands_played = [PartialHand("///") for _ in range(4)]
        self.declarer = Direction(declarer)
        self.next_play = self.declarer + 1
        self.dummy = self.next_play + 1
        self.contract = contract
        self.strain = contract[1]
        self.vulnerable = vulnerable
        self.score_table = [result_score(contract, i, vulnerable) for i in range(14)]

        self.history = []
        self.current_trick = []
        self.declarer_tricks = 0
        self.defense_tricks = 0
        self.showouts = set()

    @staticmethod
    def history_to_player(declarer, history, strain):
        player = declarer + 1
        current_trick = 0
        out = []

        best_card = None
        best_player = None

        for card in history:
            out.append(player)
            current_trick += 1
            if current_trick == 1:
                best_card, best_player = card, player
            elif card.suit == best_card.suit:
                if card > best_card:
                    best_card, best_player = card, player
            elif card.suit == strain:
                best_card, best_player = card, player

            if current_trick == 4:
                player = best_player
                current_trick = 0

        return out

    def get_player_history(self):
        return PlayDeal.history_to_player(self.declarer, self.history,
            self.strain)

    def original_player_suit_count(self, player, suit):
        return len(self.hands_left[player.i].by_suit[suit]) + \
            len(self.hands_played[player.i].by_suit[suit])

    def play_card(self, card):
        if isinstance(card, str):
            card = Card(card)
        if not isinstance(card, Card):
            raise TypeError("Want Card")

        if not card in self.hands_left[self.next_play.i].cards:
            raise ValueError("Card %s not held by %s" % (card, self.next_play))

        if len(self.current_trick) > 0:
            led_suit = self.current_trick[0].suit
            if card.suit != led_suit:
                if self.hands_left[self.next_play.i].by_suit[led_suit]:
                    raise ValueError("Player %s is not out of %s -- %s" %
                        (self.next_play, led_suit, self.hands_left[self.next_play.i]))
                self.showouts.add(ShowOut(self.next_play, led_suit,
                    self.original_player_suit_count(self.next_play, led_suit)))

        self.hands_played[self.next_play.i] += {card}
        self.hands_left[self.next_play.i] -= {card}
        self.history.append(card)
        self.current_trick.append(card)
        self.next_play += 1

        if len(self.current_trick) < 4:
            return

        best_card = self.current_trick[0]
        best_index = 0
        for i in range(1, 4):
            card = self.current_trick[i]
            if card.suit == best_card.suit:
                if card > best_card:
                    best_card, best_index = card, i
            elif card.suit == self.strain:
                best_card, best_index = card, i

        self.next_play += best_index
        if self.next_play in [self.declarer, self.dummy]:
            self.declarer_tricks += 1
        else:
            self.defense_tricks += 1
        self.current_trick = []


__all__ = ["PartialHand", "PlayDeal", "ShowOut"]

if __name__ == "__main__":
    p1 = PartialHand("952/Q32/QT9/KJ97")
    print(p1)
    print(p1 - "S5")
    # >>> deal = bm.Deal(bm.Hand("952/Q32/QT9/KJ97"), bm.Hand("AT63/76/K842/AQ8"), bm.Hand("K84/A854/AJ53/53"), bm.Hand("QJ7/KJT9/76/T642"))
