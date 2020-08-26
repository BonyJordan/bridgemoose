from .auction import Contract
from .direction import Direction
import bisect

def declarer_vulnerable(declarer, board_vulnerability):
    """\
Takes a declarer and a 'board-wide vulnerability' string, which could be
    "both" or "b" for both,
    "ns" or "n" for north/south,
    "ew" or "e" for east/west,
    "none" or "-" for none

    and returns whether this particular declarer is vulnerable.
    """
    d = Direction(declarer)
    bv = board_vulnerability.lower()
    if bv in ("both", "b"):
        return True
    elif bv in ("none", "-"):
        return False
    elif bv in ("ns", "n"):
        return d.dir_pair == "NS"
    elif bv in ("ew", "e"):
        return d.dir_pair == "EW"
    else:
        raise ValueError("Bad board_vulnerability")

def scorediff_imps(diff):
    """
Convert my_score - other_score into IMPs
    """
    imps_table = [15, 45, 85, 125, 165, 215, 265, 315, 365,
        425, 495, 595, 745, 895, 1095, 1295, 1495, 1745, 1995,
        2245, 2495, 2995, 3495, 3995]
    if diff < 0:
        return -bisect.bisect_left(imps_table, -diff)
    else:
        return bisect.bisect_left(imps_table, diff)

def scorediff_matchpoints(diff):
    if diff < 0:
        return 0.0
    elif diff > 0:
        return 1.0
    else:
        return 0.5

def result_score(contract, tricks, vulnerable):
    con = Contract(contract)

    if tricks < con.tricks_needed:
        shortfall = con.tricks_needed - tricks
        if vulnerable:
            if con.double_state == 0:
                return -100 * shortfall
            else:
                return con.double_state * (100 - 300*shortfall)
        else:
            if con.double_state == 0:
                return -50 * shortfall
            elif shortfall < 4:
                return con.double_state * (100 - 200*shortfall)
            else:
                return con.double_state * (400 - 300*shortfall)

    # end going down, start we made it

    # btl is "below the line"
    if con.strain[0] in "nN":
        btl_points = 10 + 30*con.level
    elif con.strain[0] in "SsHh":
        btl_points = 30*con.level
    else:
        btl_points = 20*con.level

    btl_points *= 2 ** con.double_state
    if con.level == 7:
        bonus = 2000 if vulnerable else 1300
    elif con.level == 6:
        bonus = 1250 if vulnerable else 800
    elif btl_points >= 100:
        bonus = 500 if vulnerable else 300
    else:
        bonus = 50

    # insult bonus
    bonus += 50 * con.double_state

    # overtricks
    overtricks = tricks - con.tricks_needed
    if con.double_state > 0:
        bonus += overtricks * con.double_state * (100 if vulnerable else 50)
    elif con.strain in "CcDd":
        bonus += overtricks * 20
    else:
        bonus += overtricks * 30

    return btl_points + bonus

__all__ = ["scorediff_imps", "scorediff_matchpoints", "result_score"]
