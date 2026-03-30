#$ Created this test code to test MUL,DIV,MOD,EXP:

800A   # LIZ r0,10
8103   # LIZ r1,3
2E04   # MUL r6,r0,r1  
70C0   # PUT r6
2704   # DIV r7,r0,r1 
70E0   # PUT r7
3204   # MOD r2,r0,r1 
7040   # PUT r2
3B24   # EXP r3,r1,r1  
7060   # PUT r3
6800   # HALT

