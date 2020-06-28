from collections import namedtuple
import math

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
        
ContractDeclarer = namedtuple("ContractDeclarer", "contract declarer")

def dd_compare_strategies(deal_generator, strategy1, strategy2, scoring="IMPS",
    vulnerability=None):
    """\
Return a Statistic() measuring how much better (or worse if mean is negative)
strategy1 is compared to strategy2.  The score is always computed from
the N-S point of view.

A "strategy" is a function which takes a Deal and returns a ContractDeclarer

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

    if scoring.upper() in ('IMPS', 'IMP'):
        score_func = scoring.scorediff_imps
    elif scoring.upper() in ('MPS', 'MP', 'MATCHPOINTS'):
        score_func = scoring.scorediff_matchpoints
    elif scoring.upper() in ('TOTAL'):
        score_func = lambda x: x
    else:
        raise ValueError("Scoring should be one of 'IMPS','matchpoints','TOTAL'")

    sign = {"N":1, "S":1, "E":-1, "W":-1}

    stat = Statistic()
    for deal in deal_generator:
        cd1 = strategy1(deal)
        cd2 = strategy2(deal)
        if cd1 == cd2:
            stat.add_data_point(0.0)

        strain1 = cd1.contract[1]
        strain2 = cd2.contract[2]

        if (strain1, cd1.declarer) == (strain2, cd2.declarer):
            tx1 = dds.solve_deal(deal, cd1.declarer, strain1)
            tx2 = tx1
        else:
            tx1, tx2 = dds.solve_many_deals([
                (deal, cd1.declarer, strain1),
                (deal, cd2.declarer, strain2)])

        score1 = sign[cd1.declarer] * scoring.result_score(cd1.contract,
            tx1, cd1.declarer in vul)
        score2 = sign[cd2.declarer] * scoring.result_score(cd2.contract,
            tx2, cd2.declarer in vul)

        stat.add_data_point(score_func(score1 - score2))

    return stat
