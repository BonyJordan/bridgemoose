from collections import defaultdict, namedtuple
import operator
import re

from . import Card, Contract, Deal, Direction, Hand
from .scoring import result_score

ShowOut = namedtuple("ShowOut", "dir suit count")

class PartialHand:
    def __init__(self, data):
        if isinstance(data, str):
            self._load_from_string(data)
        elif isinstance(data, (Hand,PartialHand)):
            self._load_from_cards(data.cards)
        else:
            self._load_from_cards(data)

    def _load_from_string(self, data):
        suits = re.split(r"/|\.", data)
        if len(suits) != 4:
            raise ValueError("Expected 4 suits, got '%s'" % (data))

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

class PlayView:
    def __init__(self, declarer, contract, vulnerable):
        self.hands_left = [None] * 4
        self.hands_played = [PartialHand("///") for _ in range(4)]
        self.declarer = Direction(declarer)
        self.next_play = self.declarer + 1
        self.dummy = self.next_play + 1
        self.contract = Contract(contract)
        self.strain = self.contract.strain
        self.vulnerable = vulnerable
        self.score_table = [result_score(contract, i, vulnerable) for i in range(14)]

        self.history = []
        self.current_trick = []
        self.declarer_tricks = 0
        self.defense_tricks = 0
        self.showouts = set()

    def clone(self):
        copy = PlayView(self.declarer, self.contract, self.vulnerable)
        return self.clone_into(copy)

    def clone_into(self, other):
        other.hands_left = [None if x is None else PartialHand(x) for x in self.hands_left]
        other.hands_played = [PartialHand(x) for x in self.hands_played]
        other.next_play = self.next_play
        other.history = list(self.history)
        other.current_trick = list(self.current_trick)
        other.declarer_tricks = self.declarer_tricks
        other.defense_tricks = self.defense_tricks
        other.showouts = set(self.showouts)
        return other
    

    @staticmethod
    def history_to_player(declarer, history, strain):
        """\
Construct a list which identifies which player played each card
from a history (an iterable of played cards)
        """
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

    def set_hand(self, direction, hand):
        direction = Direction(direction)
        if not self.hands_left[direction.i] is None:
            raise ValueError("Hand %s already set", direction)
        if self.hands_played[direction.i].cards:
            raise ValueError("Cards already played")
        if isinstance(hand, Hand):
            self.hands_left[direction.i] = PartialHand(hand)
        elif isinstance(hand, PartialHand):
            self.hands_left[direction.i] = hand
        else:
            raise TypeError(hand, "Want Hand or PartialHand")

    def set_dummy(self, hand):
        self.set_hand(self.dummy, hand)

    def get_player_history(self):
        return PlayView.history_to_player(self.declarer, self.history,
            self.strain)

    def legal_plays(self):
        my_hand = self.hands_left[self.next_play.i]
        return self.hand_legal_plays(my_hand)

    def hand_legal_plays(self, hand):
        if isinstance(hand, Hand):
            hand = PartialHand(hand) - self.hands_played[self.next_play.i]
        if len(self.current_trick) > 0:
            led_suit = self.current_trick[0].suit
            same_suit = hand.by_suit[led_suit]
            if same_suit:
                return list(same_suit)

        return list(hand.cards)


    def finish_trick(self):
        assert len(self.current_trick) == 4

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


    def play_card(self, card, finish_trick=True):
        if isinstance(card, str):
            card = Card(card)
        if not isinstance(card, Card):
            raise TypeError("Want Card")

        next_hand = self.hands_left[self.next_play.i]
        if next_hand is not None:
            if not card in next_hand.cards:
                raise ValueError("Card %s not held by %s" % (card, self.next_play))
            self.hands_left[self.next_play.i] -= {card}

        if len(self.current_trick) > 0:
            led_suit = self.current_trick[0].suit
            if card.suit != led_suit:
                if next_hand is not None and next_hand.by_suit[led_suit]:
                    raise ValueError("Player %s is not out of %s -- %s" %
                        (self.next_play, led_suit, next_hand))

                self.showouts.add(ShowOut(self.next_play, led_suit,
                    len(self.hands_played[self.next_play.i].by_suit[led_suit])))

        self.hands_played[self.next_play.i] += {card}
        self.history.append(card)
        self.current_trick.append(card)
        self.next_play += 1

        if len(self.current_trick) == 4 and finish_trick:
            self.finish_trick()

class ViewKeeper:
    """ A class that maintains views for all four (three) players """
    def __init__(self, declarer, contract, vulnerable, deal):
        declarer = Direction(declarer)
        lho = declarer + 1
        dummy = declarer + 2
        rho = declarer + 3

        self.deal = deal
        self.views = [PlayView(declarer, contract, vulnerable) for _ in range(3)]
        self.dec_view, self.lho_view, self.rho_view = self.views

        self.view_by_dir = [None]*4
        self.view_by_dir[declarer.i] = self.dec_view
        self.view_by_dir[lho.i] = self.lho_view
        self.view_by_dir[dummy.i] = self.dec_view
        self.view_by_dir[rho.i] = self.rho_view

        self.dec_view.set_hand(declarer, deal[declarer])
        self.lho_view.set_hand(lho, deal[lho])
        self.rho_view.set_hand(rho, deal[rho])

    def play_card(self, card, finish_trick=True):
        for view in self.views:
            view.play_card(card, finish_trick)
            if len(view.history) == 1:
                view.set_dummy(deal[view.dummy])

    def finish_trick(self):
        for view in self.views:
            view.finish_trick()

    def cur_direction(self):
        return self.dec_view.next_play

    def cur_view(self):
        return self.views[self.dec_view.next_play.i]

    def cur_legal_plays(self):
        return self.cur_view().legal_plays()

    def get_history(self):
        return self.dec_view.history

    def get_declarer_tricks(self):
        return self.dec_view.declarer_tricks

    def get_defense_tricks(self):
        return self.dec_view.defense_tricks


__all__ = ["PartialHand", "PlayView", "ShowOut", "ViewKeeper"]

if __name__ == "__main__":
    p1 = PartialHand("952/Q32/QT9/KJ97")
    print(p1)
    print(p1 - "S5")
    # >>> deal = bm.Deal(bm.Hand("952/Q32/QT9/KJ97"), bm.Hand("AT63/76/K842/AQ8"), bm.Hand("K84/A854/AJ53/53"), bm.Hand("QJ7/KJT9/76/T642"))
