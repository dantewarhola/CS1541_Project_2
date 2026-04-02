#$ Tests phase 3

801F # LIZ $R0, 31
8220 # LIZ $R1, 16
0060 # ADD $R2, $R0, $R1
4860 # SW  $R2, $R3
4360 # LW  $R3, $R3
6800 # HALT