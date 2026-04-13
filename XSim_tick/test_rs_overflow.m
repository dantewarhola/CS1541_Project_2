# Test 4: RS overflow
# Tests: structural hazard stalls when RSs fill up
# Uses 6 LIZ instructions but only 4 integer RSs available
801F  # LIZ $R0, 31
8304  # LIZ $R3, 4
8502  # LIZ $R5, 2
8107  # LIZ $R0, 7
830A  # LIZ $R3, 10
8503  # LIZ $R5, 3
7060  # PUT $R3
6800  # HALT