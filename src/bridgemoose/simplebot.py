import random

from . import Card, Deal, Direction, Hand, PartialHand, PlayView
from . import dds, scoring

class SimpleBot:
    """\
A single-dummy computer player using approximately the GiB algorithm,
which we all know to be somewhat imperfect, but actually fairly decent
in practice.  Namely, it generates a bunch of random hands consistent
with what is known so far, solves each double dummy, and chooses the
card play which maximizes the score.
    """

    def __init__(self, trials=10, error_weight=0.1, open_lead_error_weight=0.3,
        scoring_type="imps", debug=False):
        """\
- trials is the number of double dummy hands to play
- error_weight says how much weight to consider if a previous play by an
    opponent is DD incorrect.  Arguably this should decrease over the
    course of a hand.
- open_lead_error_weight same, but for the opening lead
- scoring: can be 'imps', 'matchpoints', or 'total'\
        """

        self.debug = debug
        self.trials = trials
        self.error_weight = error_weight
        self.open_lead_error_weight = open_lead_error_weight

        if scoring_type == "imps":
            self.scorer = scoring.scorediff_imps
        elif scoring_type == "total":
            self.scorer = lambda x: x
        elif scoring_type == "matchpoints":
            self.scorer = scoring.scorediff_matchpoints
        else:
            raise ValueError("scoring must be 'imps', 'matchpoints', or 'total'")

    def smart_play(self, pd, accept):
        if self.debug:
            print("pd.hands_left=", pd.hands_left[pd.next_play.i])
        if not pd.current_trick and len(pd.hands_left[pd.next_play.i]) == 13:
            return self.opening_lead(pd, accept)
        if pd.current_trick:
            # Cards In Led Suit
            cils = pd.hands_left[pd.next_play.i].by_suit[pd.current_trick[0].suit]
            if len(cils) == 1:
                return next(iter(cils))

        known_dirs = [pd.next_play]
        if pd.next_play == pd.dummy:
            known_dirs.append(pd.declarer)
        else:
            known_dirs.append(pd.dummy)
        deals = self.generate_matching_deals(pd, accept, known_dirs)
        weights = self.get_fancy_weights(pd, deals)
        return self.weighted_play_choice(pd, deals, weights)

    def get_fancy_weights(self, pd, deals):
        declarers = [pd.declarer, pd.dummy]
        defenders = [pd.declarer+1, pd.dummy+1]
        if pd.next_play in declarers:
            opps = defenders
        else:
            opps = declarers
        player_history = pd.get_player_history()
        history_str = "".join(map(str, pd.history))

        weights = []
        debug = self.debug
        for deal in deals:
            weight = 1.0
            adp_out = dds.analyze_deal_play(deal, str(pd.declarer),
                pd.strain, history_str)
            for i, tpl in enumerate(adp_out):
                who = player_history[i]
                if not who in opps:
                    continue
                ew = self.open_lead_error_weight if i == 0 else self.error_weight
                goods, bads, mine = tpl
                den = goods + bads * ew
                num = 1.0 if mine else ew
                weight *= num / den
                if debug:
                    print("modify for %s by %f / %f" % (pd.history[i], num, den))
            weights.append(weight)
            debug = self.debug

        return weights


    def opening_lead(self, pd, accept):
        deals = self.generate_matching_deals(pd, accept, [pd.next_play])

        weights = [1.0 for _ in deals]
        return self.weighted_play_choice(pd, deals, weights)

    def weighted_play_choice(self, pd, deals, weights):
        debug = self.debug

        smp_list = [tuple([
            str(PartialHand(deal[d]) - pd.hands_played[d.i])
            for d in Direction.all_dirs()])
                for deal in deals]

        cur_trick = "".join(map(str, pd.current_trick))
        smp_out = dds.solve_many_plays(smp_list, str(pd.next_play),
            pd.strain, cur_trick)

        if pd.next_play in [pd.declarer, pd.dummy]:
            scores = [{card: pd.score_table[tricks + pd.declarer_tricks if tricks != -2 else 0]
                for card, tricks in deal_result} for deal_result in smp_out]
        else:
            scores = [{card: -pd.score_table[13 - pd.defense_tricks - tricks if tricks != -2 else 0]
                for card, tricks in deal_result} for deal_result in smp_out]

        viable_cards = [x[0] for x in smp_out[0]]
        if debug:
            sw = sum(weights)
            for card in viable_cards:
                s = [m[card] for m in scores]
                print("%s: [%s]" % (card, ",".join(map(str, s))))


        while len(viable_cards) > 1:
            champ = random.choice(viable_cards)
            gt_cards = []
            eq_cards = []
            for challenger in viable_cards:
                if challenger == champ:
                    eq_cards.append(challenger)
                    continue

                diffs = [self.scorer(dr[champ] - dr[challenger]) for dr in scores]
                if debug:
                    for i, x in enumerate(diffs):
                        if abs(x) >= 0:
                            print("weight=%g   %s vs %s diff = %d (%d:%d)\n%s" % (weights[i]/sw, champ, challenger, x, scores[i][champ], scores[i][challenger], deals[i].square_string()))
                if debug:
                    print("%s - %s = [%s]" % (champ, challenger, ",".join(map(str,diffs))))
                diff = sum([weight * diff for weight, diff in zip(weights, diffs)])
                if debug:
                    if diff < 0:
                        print("RESULT! Challenger (%s) DEFEATS (%s)" % (challenger, champ))
                    elif diff == 0:
                        print("RESULT! Challenger (%s) ties (%s)" % (challenger, champ))
                    else:
                        print("RESULT! Challenger (%s) loses to (%s)" % (challenger, champ))

                if diff < 0:
                    gt_cards.append(challenger)
                elif diff == 0:
                    gt_cards.append(challenger)

            if gt_cards:
                viable_cards = gt_cards
            else:
                return random.choice(eq_cards)

        return viable_cards[0]
        raise NotImplemented

    def generate_matching_deals(self, pd, accept, known_dirs):
        debug = self.debug

        fixed_cards = dict()
        for d in Direction.all_dirs():
            if d in known_dirs:
                fixed_cards[str(d)] = pd.hands_played[d.i] + pd.hands_left[d.i]
            else:
                fixed_cards[str(d)] = pd.hands_played[d.i]

        if debug:
            print("fixed_cards=", fixed_cards)
            print("showouts=", pd.showouts)

        def showout_accept(deal):
            for so in pd.showouts:
                if not deal[so.dir].count[so.suit] == so.count:
                    return False
            else:
                return accept(deal)

        assert False
#        return list(semi_random_deals(
#            self.trials, showout_accept, fixed_cards))


    def play_out_deal(self, deal, declarer, contract, vulnerable, accept):
        """\
Takes a full deal and plays it out, assuming each player is single dummy,
and returns a tuple: (tricks_taken_by_declarer, cards_played_in_order).

Inputs are:
- deal: a bm.Deal object.
- declarer: the direction of the declarer.  String or bridgemoose.Direction
- contract: the contract as a string, e.g. "3NT", "4S", "5Cx", etc.
- vulnerable: whether declarer is vulnerable or not
- accept: a function which takes as input a deal object and returns whether
    or not the deal is consistent with the bidding.  SimpleBot is too dumb
    to know how to interpret auctions, so you, the human, have to do the
    work for it.\

Outputs are:
- number of tricks won by declaring side
- list of cards played
        """
        debug = self.debug

        lho_view = PlayView(declarer, contract, vulnerable)
        rho_view = PlayView(declarer, contract, vulnerable)
        dec_view = PlayView(declarer, contract, vulnerable)

        dec = declarer
        lho = dec + 1
        dum = lho + 1
        rho = dum + 1

        lho_view.set_hand(lho, deal[lho])
        rho_view.set_hand(rho, deal[rho])
        dec_view.set_hand(dec, deal[dec])

        views = [None]*4
        views[dec.i] = dec_view
        views[lho.i] = lho_view
        views[rho.i] = rho_view
        views[dum.i] = dec_view

        opening_lead = True
        while dec_view.declarer_tricks + dec_view.defense_tricks < 13:
            play = self.smart_play(views[dec_view.next_play.i], accept)
            if debug:
                print("The play is %s" % (play,))
            if opening_lead:
                opening_lead = False
                for view in [lho_view, rho_view, dec_view]:
                    view.set_dummy(deal[dum])
            for view in [lho_view, rho_view, dec_view]:
                view.play_card(play)

        return dec_view.declarer_tricks, dec_view.history

    def play_one_card(self, declarer, contract, vulnerable, accept,
        history, my_hand, my_direction, dummy_hand=None):
        """\
Given a certain bridge situation, find a good card play using the
SimpleBot algorithm.

- declarer: a string or bridgemoose.Declarer saying who is declaring
- contract: the contract as a string, e.g. "3NT", "4S", "5Cx", etc.
- vulnerable: whether declarer is vulnerable or not
- accept: a function which takes as input a deal object and returns whether
    or not the deal is consistent with the bidding.  SimpleBot is too dumb
    to know how to interpret auctions, so you, the human, have to do the
    work for it.
- history: a list of bridgemoose.Card showing which cards have been played
    so far.
- my_hand: a bridgemoose.PartialHand containing my original 13 cards
- my_direction: see 'declarer'
- dummy_hand: a bridgemoose.PartialHand containing dummy's original 13 cards,
    can be None if we are on opening lead.

Returns a single bridgemoose.Card object
        """


        view = PlayView(declarer, contract, vulnerable)
        view.set_hand(my_direction, my_hand)
        if dummy_hand is not None:
            view.set_dummy(dummy_hand)

        for h in history:
            view.play_card(h)

        return self.smart_play(view, accept)
 

def test_hand(bp):
    deal = Deal(
        Hand("952/Q32/QT9/KJ97"),
        Hand("AT63/76/K842/AQ8"),
        Hand("K84/A854/AJ53/53"),
        Hand("QJ7/KJT9/76/T642"))

    def accept(deal):
        return deal.N.hcp >= 11 and deal.N.hcp <= 14 and \
            deal.N.nd >= deal.N.nc and deal.N.nh < 4 and deal.N.ns == 4 and \
            deal.S.nh == 4 and deal.S.nd < 4 and deal.S.ns == 3 and \
            deal.S.hcp >= 5 and deal.S.hcp <= 9

    tricks, plays = bp.play_out_deal(deal, "N", "1S", False, accept)


__all__ = ["SimpleBot"]

if __name__ == "__main__":
    BridgeMasterDeals = {
        "1B15": ("6N", "AQJ/432/32/AT876", "K32/KQJ/AKQ/Q432", lambda d: True),
        "1B16": ("6N", "AQJ/32/32/AT8765", "K32/KQJ/AKQ/Q432", lambda d: True),
        "1B17": ("6N", "AQJ/43/32/AT9876", "K32/AK2/AKQ/Q432", lambda d: True),
        "1B18": ("6N", "AQJ/32/32/KJT987", "K32/AKQ/AKQ/5432", lambda d: True),
        "1B19": ("4H", "765/5432/KQ/5432", "432/AQJT9/A32/AK", lambda d: True),
        "1B20": ("4H", "432/6543/K32/KQ4", "AQ5/QJT987/AQ/32", lambda d: True),
        "1B21": ("4H", "K65/Q32/AK654/54", "432/AKJT9/32/A32", lambda d: True),
        "1B22": ("4S", "432/43/K654/AK32", "KQJT9/2/A32/QJT9",
            lambda d: d.W.nh >= 6 and d.E.nh >= 3),
        "1B23": ("7S", "AK/A/AKQJ/KQ8765", "QJ5432/32/2/A432", lambda d: True),
        "1B24": ("4S", "AQ/8765/AK7654/3", "KJT987/432/32/A2", lambda d: True),
        "1B25": ("6S", "432/AKT98/AQ/432", "AKQJT9/2/432/KQJ", lambda d: True),
        "1B26": ("4H", "A654/A2/AQ65/K43", "32/KQJ9876/432/2", lambda d: True),
        "1B27": ("7N", "AK/432/765/KQJ32", "2/AKQJT9/AK432/A", lambda d: True),
        "1B28": ("5C", "A54/32/AT98/A432", "J32/A/QJ2/KQJT98", lambda d: True),
        "1B29": ("2H", "QJT/AKQ/A7654/54", "432/JT987/32/A32", lambda d: True),
        "1B30": ("4S", "A432/A432/J43/43", "QJT98/KQJT/A2/K2", lambda d: True),
        "2A1":  ("3N", "A/AK/5432/QJT987", "KQJ/J432/AQJT/32", lambda d: True),
        "2A2":  ("6N", "AJ9/KQJ/AQJ/5432", "432/A32/K32/AKQJ", lambda d: True),
    }

    import sys

    bp = BotPlayer(trials=200)
    bm_id = sys.argv[1]
    history_str = sys.argv[2]
    history = list(map(Card, history_str.split(","))) if history_str else []

    contract, dum_str, dec_str, accept = BridgeMasterDeals[bm_id]

    c = bp.play_one_card(Direction.SOUTH, contract, False, accept,
        history, PartialHand(dec_str), Direction.SOUTH,
        PartialHand(dum_str))
    print(c)
