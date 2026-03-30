#$ Created this test code to test ADD,SUB,AND,NOR:

800A # LIZ r0, 10
8103 # LIZ r1, 3
0204 # ADD r2, r0, r1
7040 # PUT r2
0B04 # SUB r3, r0, r1
7060 # PUT r3
1404 # AND r4, r0, r1
7080 # PUT r4
1D04 # NOR r5, r0, r1
70A0 # PUT r5
6800 # HALT