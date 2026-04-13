# Test 2: Mixed FU types
# Tests: multiplier, divider, and integer FUs all active
801F  # LIZ $R0, 31
8304  # LIZ $R3, 4
8502  # LIZ $R5, 2
2860  # MUL $R1, $R3, $R0
2260  # DIV $R4, $R3, $R0
0060  # ADD $R0, $R3, $R0
7060  # PUT $R3
6800  # HALT