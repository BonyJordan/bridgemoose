import bridgemoose as bm
import bridgemoose.jade as bj

m = bm.hand_makers()

n = bm.Hand("432/K3/KQ54/K432")
s = bm.Hand("QJT9/A2/A32/AJ98")
open_lead = "HJ"
con = bm.Contract("3NT")
N = 10

w = m.CARD("HJ") & ((m.HEARTS <= 2) | (m.CARD("HT"))) & ~(m.CARD("HQ"))
print(f"North={n}")
print(f"South={s}")

def win_rank_min(wr_list):
    return "".join(min([x[i] for x in wr_list], key=bm.Card.rank_order) for i in range(4))


wr_list = []
we_trips = []
fails = 0
for deal in bm.random_deals(None, south=s, north=n, west=w):
    smp_list = [tuple([
        str(bm.PartialHand(deal[d]) - open_lead)
        for d in bm.Direction.all_dirs()])]
    cur_trick = str(open_lead)
    smp_out = bm.dds.solve_many_plays(smp_list, "N", con.strain, cur_trick, True)
    print(f"For {deal.W}::{deal.E} got {smp_out[0]}")
    if any(pair[1] >= con.tricks_needed for pair in smp_out[0]):
        we_trips.append((deal.W, deal.E, 1.0))
        for trip in smp_out[0]:
            if trip[1] >= con.tricks_needed:
                wr_list.append(trip[2])
        if len(we_trips) == N:
            break
    else:
        fails += 1
        if fails > 10000*(len(we_trips)+1):
            raise ValueError("Too many fails")

def rotate_deals(we_trips, mnwr, used_cards):
    # mnwr = max non win rank
    wmap = {}
    used_by_suit = {x:"" for x in "SHCD"}
    for card in used_cards:
        card = bm.Card(card)
        used_by_suit[card.suit] += card.rank

    for (west, east, weight) in we_trips:
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

        r_west = bm.Hand(w_cards)
        r_east = bm.Hand(e_cards)
        key = (r_west, r_east)
        if key in wmap:
            wmap[key] += weight
        else:
            wmap[key] = weight

    return [(*key, value) for key, value in wmap.items()]


r_we_trips = rotate_deals(we_trips, win_rank_min(wr_list), [open_lead])
print("After rotations")
for west, east, weight in r_we_trips:
    print(f"{west}::{east} -> {weight}")

we_pairs = [(west, east) for west, east, weight in r_we_trips]

solver = bj.Solver(n, s, con.strain, con.tricks_needed, we_pairs)
print(solver.eval([open_lead]))
