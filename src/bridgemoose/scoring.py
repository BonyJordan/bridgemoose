import bisect

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

import re

def scorediff_matchpoints(diff):
    if diff < 0:
        return 0.0
    elif diff > 0:
        return 1.0
    else:
        return 0.5

def result_score(contract, tricks, vulnerable):
    mo = re.match("([1-7])([CcDdHhSsNn]|NT|nt)([x*]{0,2})$", contract)
    if not mo:
        raise ValueError("Bad contract \"" + contract + "\"")

    level = int(mo.group(1))
    tricks_needed = 6 + level

    if tricks < tricks_needed:
        shortfall = tricks_needed - tricks
        if vulnerable:
            if len(mo.group(3)) == 0:
                return -100 * shortfall
            else:
                return len(mo.group(3)) * (100 - 300*shortfall)
        else:
            if len(mo.group(3)) == 0:
                return -50 * shortfall
            elif shortfall < 4:
                return len(mo.group(3)) * (100 - 200*shortfall)
            else:
                return len(mo.group(3)) * (400 - 300*shortfall)

    # end going down, start we made it

    # btl is "below the line"
    if mo.group(2)[0] in "nN":
        btl_points = 10 + 30*level
    elif mo.group(2)[0] in "SsHh":
        btl_points = 30*level
    else:
        btl_points = 20*level

    btl_points *= 2 ** len(mo.group(3))
    if level == 7:
        bonus = 2000 if vulnerable else 1300
    elif level == 6:
        bonus = 1250 if vulnerable else 800
    elif btl_points >= 100:
        bonus = 500 if vulnerable else 300
    else:
        bonus = 50

    # insult bonus
    bonus += 50 * len(mo.group(3))

    # overtricks
    overtricks = tricks - tricks_needed
    if len(mo.group(3)) > 0:
        bonus += overtricks * len(mo.group(3)) * (100 if vulnerable else 50)
    elif mo.group(2)[0] in "CcDd":
        bonus += overtricks * 20
    else:
        bonus += overtricks * 30

    return btl_points + bonus

__all__ = ["scorediff_imps", "scorediff_matchpoints", "result_score"]
