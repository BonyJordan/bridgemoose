import math
from collections import Counter

from . import dds
from . import scoring

class Statistic:
    def __init__(self):
        self.count = 0
        self.sum_weight = 0.0
        self.sum_weight_sq = 0.0
        self.mean = 0.0
        self.sum_squares = 0.0

    def add_data_point(self, x, weight=1.0):
        self.count += 1
        self.sum_weight += weight
        self.sum_weight_sq += weight * weight

        mean_old = self.mean
        delta = x - mean_old

        self.mean += (weight / self.sum_weight) * delta
        self.sum_squares += weight * delta * (x - self.mean)

    def sample_variance(self):
        return self.sum_squares / (self.sum_weight - self.sum_weight_sq / self.sum_weight)

    def std_error(self):
        return math.sqrt(self.sample_variance() * self.sum_weight_sq) / self.sum_weight

    def __str__(self):
        return "%.2f +/- %.2f" % (self.mean, self.std_error())
        
def dd_compare_strategies(deal_generator, strategy1, strategy2,
    score_type="IMPS", vulnerability=None):
    """\
Returns a triple; a Statistic and two collections.Counter objects.
The Statistic returns the score from the NS point of view of strategy1
vs. strategy2.  The Counters are essentially dicts mapping ("<contract-declarer>", score) pairs to counts.

A "strategy" is a function which takes a Deal and returns a ContractDeclarer,
which is a string like "3NT-W"

"Scoring" can be 'IMPS', 'matchpoints', 'TOTAL'.

"vulnerability" can be None, "", "NS", "EW", "BOTH", "NSEW"\

    """
    if vulnerability is None:
        vul = ""
    elif vulnerability is "NOTH":
        vul = "NSEW"
    elif vulnerability in ("", "NS", "EW", "NSEW", "EWNS"):
        vul = vulnerability
    else:
        raise ValueError("Vulnerability should be '', 'NS', 'EW', or 'NSEW'")

    if score_type.upper() in ('IMPS', 'IMP'):
        score_func = scoring.scorediff_imps
    elif score_type.upper() in ('MPS', 'MP', 'MATCHPOINTS'):
        score_func = scoring.scorediff_matchpoints
    elif score_type.upper() in ('TOTAL'):
        score_func = lambda x: x
    else:
        raise ValueError("Scoring should be one of 'IMPS','matchpoints','TOTAL'")

    sign = {"N":1, "S":1, "E":-1, "W":-1}

    stat = Statistic()
    count1 = Counter()
    count2 = Counter()

    for deal in deal_generator:
        cd1 = strategy1(deal)
        cd2 = strategy2(deal)
        con1, dec1 = cd1.split("-")
        con2, dec2 = cd2.split("-")
        if (con1,dec1) == (con2,dec2):
            stat.add_data_point(0.0)

        strain1 = con1[1]
        strain2 = con2[1]

        if (strain1, dec1) == (strain2, dec2):
            tx1 = dds.solve_deal(deal, dec1, strain1)
            tx2 = tx1
        else:
            tx1, tx2 = dds.solve_many_deals([
                (deal, dec1, strain1),
                (deal, dec2, strain2)])

        score1 = sign[dec1] * scoring.result_score(con1,
            tx1, dec1 in vul)
        score2 = sign[dec2] * scoring.result_score(con2,
            tx2, dec2 in vul)

        stat.add_data_point(score_func(score1 - score2))
        count1[(cd1, score1)] += 1
        count2[(cd2, score2)] += 1


    return stat, count1, count2
