import bridgemoose as bm
import bridgemoose.jade as bj

m = bm.hand_makers()

n = bm.Hand("432/K3/KQ54/K432")
s = bm.Hand("QJT9/A2/A32/AJ98")
open_lead = "HJ"
con = bm.Contract("3NT")
N = 5

w = m.CARD(open_lead)

deals = []
ews = []
fails = 0
for deal in bm.random_deals(None, south=s, north=n, west=w):
    smp_list = [tuple([
        str(bm.PartialHand(deal[d]) - open_lead)
        for d in bm.Direction.all_dirs()])]
    cur_trick = str(open_lead)
    smp_out = bm.dds.solve_many_plays(smp_list, "N", con.strain, cur_trick)
    print(f"For {deal.W}/{deal.E} got {smp_out[0]}")
    if any(pair[1] >= con.tricks_needed for pair in smp_out[0]):
        print("yay")
        deals.append(deal)
        ews.append((deal.E, deal.W))
        if len(deals) == N:
            break
    else:
        fails += 1
        if fails > 10000*(len(deals)+1):
            raise ValueError("Too many fails")

solver = bj.Solver(n, s, con.strain, con.tricks_needed, ews)
print(solver.eval(["HJ"]))
