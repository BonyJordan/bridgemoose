import bridgemoose as bm
import bridgemoose.jade as bj

n = bm.Hand("432/K3/KQ54/K432")
s = bm.Hand("QJT9/A2/A32/AJ98")

w1 = bm.Hand("AK8/JT987/JT98/6")
e1 = bm.Hand("765/Q654/76/QT75")

w2 = bm.Hand("AK87/JT9/JT9/QT7")
e2 = bm.Hand("65/Q87654/876/65")

solver = bj.Solver(n, s, "N", 9, [(e1,w1), (e2,w2)])
print(solver.eval(["HJ"]))
