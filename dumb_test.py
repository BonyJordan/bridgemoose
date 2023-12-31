import bridgemoose as bm
import bridgemoose.jade as bj
from collections import defaultdict

m = bm.hand_makers()

n = bm.Hand("432/K3/KQ54/K432")
s = bm.Hand("QJT9/A2/A32/AJ98")
open_lead = "HJ"
con = bm.Contract("3NT")
w = m.CARD("HJ") & ((m.HEARTS <= 2) | (m.CARD("HT"))) & ~(m.CARD("HQ"))
N = 20

print(f"North={n}")
print(f"South={s}")
used_cards = [open_lead]

def win_rank_min(wr_list):
    return "".join(min([x[i] for x in wr_list], key=bm.Card.rank_order) for i in range(4))

def win_rank_score(wr):
    return sum(bm.Card.rank_order(r) for r in wr)


def rotate_one_deal(west, east, mnwr, used_cards):
    used_by_suit = defaultdict(set)
    for card in used_cards:
        card = bm.Card(card)
        used_by_suit[card.suit].add(card.rank)

    w_cards = []
    e_cards = []
    for suit, key_rank in zip("SHDC", mnwr):
        w_ranks = west.by_suit[suit]
        e_ranks = east.by_suit[suit]

        w_keep = []
        e_keep = []
        sortable = []

        for rank_list, keep_list in [(w_ranks,w_keep),(e_ranks,e_keep)]:
            for rank in rank_list:
                if rank in used_by_suit[suit] or bm.cmp_rank(rank, key_rank) > 0:
                    keep_list.append(rank)
                else:
                    sortable.append(rank)

        rotations = sorted(sortable, key=bm.Card.rank_order)
        w_add = len(w_ranks) - len(w_keep)

        w_cards.extend([bm.Card(suit, rank) for rank in w_keep])
        w_cards.extend([bm.Card(suit, rank) for rank in rotations[:w_add]])
        e_cards.extend([bm.Card(suit, rank) for rank in e_keep])
        e_cards.extend([bm.Card(suit, rank) for rank in rotations[w_add:]])

    return bm.Hand(w_cards), bm.Hand(e_cards)

weighted_wes = defaultdict(float)
all_best_wr = []
fails = 0
for deal in bm.random_deals(None, south=s, north=n, west=w):
    smp_list = [tuple([
        str(bm.PartialHand(deal[d]) - open_lead)
        for d in bm.Direction.all_dirs()])]
    cur_trick = str(open_lead)
    smp_out = bm.dds.solve_many_plays(smp_list, "N", con.strain, cur_trick, True)
    #print(f"For {deal.W}::{deal.E} got {smp_out[0]}")
    if any(pair[1] >= con.tricks_needed for pair in smp_out[0]):
        best_wr = max([trip[2] for trip in smp_out[0]], key=win_rank_score)
        all_best_wr.append(best_wr)
        #west, east = rotate_one_deal(deal.W, deal.E, best_wr, used_cards)
        west, east = deal.W, deal.E
        weighted_wes[(west,east)] += 1.0

        if len(weighted_wes) == N:
            break
    else:
        fails += 1
        if fails > 10000*(len(weighted_wes)+1):
            raise ValueError("Too many fails")

# Now rotate
mnwr = win_rank_min(all_best_wr)
print(f"The uber min win-ranks = {mnwr}")
rotated_wes = defaultdict(float)
for (west, east), weight in weighted_wes.items():
    r_west, r_east = rotate_one_deal(west, east, mnwr, used_cards)
    rotated_wes[(r_west,r_east)] += weight

print(f"After rotation, there are {len(rotated_wes)} left")
for (west, east), weight in rotated_wes.items():
    print(f"{west}::{east} {weight:.1f}")

we_pairs = list(rotated_wes.keys())

solver = bj.Solver(n, s, con.strain, con.tricks_needed, we_pairs)
print(solver.eval([open_lead]))
print(solver.stats())
