import math
from collections import Counter
from collections import defaultdict
import time

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
        if self.sum_squares == 0:
            return 0
        else:
            return self.sum_squares / (self.sum_weight - self.sum_weight_sq / self.sum_weight)

    def std_error(self):
        if self.sum_weight <= 0:
            return 0.0
        return math.sqrt(self.sample_variance() * self.sum_weight_sq) / self.sum_weight

    def __str__(self):
        return "%.2f +/- %.2f" % (self.mean, self.std_error())
        
def dd_compare_strategies(deal_generator, strategy1, strategy2,
    score_type="IMPS", vulnerability=None, bucketer=None):
    """\
Returns a triple; a Statistic and two collections.Counter objects.
The Statistic returns the score from the NS point of view of strategy1
vs. strategy2.  The Counters are essentially dicts mapping ("<contract-declarer>", score) pairs to counts.

A "strategy" can be either a contract-declarer string ("3NT-W") or a
function which takes a Deal and returns a contract-declarer.

"Scoring" can be 'IMPS', 'matchpoints', 'TOTAL'.

"vulnerability" can be None, "", "NS", "EW", "BOTH", "NSEW"

If bucketer is not None, it is a function which takes a Deal and returns
a dict key, and then dd_compare_strategies returns a dict whose values
are triples as described above.
    """
    if vulnerability is None:
        vul = ""
    elif vulnerability in ("BOTH", "b"):
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

    out = defaultdict(lambda: (Statistic(), Counter(), Counter()))

    deals = []
    queries = []
    for deal in deal_generator:
        deals.append(deal)

        cd1 = strategy1(deal) if callable(strategy1) else strategy1
        cd2 = strategy2(deal) if callable(strategy2) else strategy2
        con1, dec1 = cd1.split("-")
        con2, dec2 = cd2.split("-")

        strain1 = con1[1]
        strain2 = con2[1]

        if (strain1, dec1) == (strain2, dec2):
            queries.append((deal, dec1, strain1))
        else:
            queries.append((deal, dec1, strain1))
            queries.append((deal, dec2, strain2))

    answers = []
    for i in range(0, len(queries), 200):
        answers.extend(dds.solve_many_deals(queries[i:i+200]))

    for deal in deals:
        cd1 = strategy1(deal) if callable(strategy1) else strategy1
        cd2 = strategy2(deal) if callable(strategy2) else strategy2
        con1, dec1 = cd1.split("-")
        con2, dec2 = cd2.split("-")

        strain1 = con1[1]
        strain2 = con2[1]

        if (strain1, dec1) == (strain2, dec2):
            tx1 = answers.pop(0)
            tx2 = tx1
        else:
            tx1 = answers.pop(0)
            tx2 = answers.pop(0)

        score1 = sign[dec1] * scoring.result_score(con1,
            tx1, dec1 in vul)
        score2 = sign[dec2] * scoring.result_score(con2,
            tx2, dec2 in vul)

        b = "" if bucketer is None else bucketer(deal)
        stat, count1, count2 = out[b]

        stat.add_data_point(score_func(score1 - score2))
        count1[(cd1, score1)] += 1
        count2[(cd2, score2)] += 1

    if bucketer is None:
        return out[""]
    else:
        return out

class ReportingTimer:
    def __init__(self, name=None):
        self.name = name
        self.count = 0
        self.last_start = None
        self.last_check = None
        self.total_time = 0

    @staticmethod
    def fmt_elapsed(amount):
        if amount < 1:
            return f"{amount:.3f}s"
        elif amount < 10:
            return f"{amount:.2f}s"
        elif amount < 100:
            return f"{amount:.1f}s"
        else:
            return f"{amount:.0f}s"

    def __enter__(self):
        self.count += 1
        self.last_start = time.time()

    def __exit__(self, exc_type, exc_value, traceback):
        elapsed = time.time() - self.last_start
        self.total_time += elapsed
        pstr = f"{self.name if self.name else 'Timer'}: "
        if self.count == 1:
            pstr += self.fmt_elapsed(elapsed)
        else:
            pstr += f"avg {self.fmt_elapsed(self.total_time/self.count)} over {self.count} calls"
        print(pstr)

