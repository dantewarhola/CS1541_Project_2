# Test 1: Integer-heavy trace with RAW dependencies
# Tests: dependency forwarding, multiple integer FUs, RS pressure
801F  # LIZ $R0, 31
8304  # LIZ $R3, 4
8502  # LIZ $R5, 2
0060  # ADD $R0, $R3, $R0
0AA0  # SUB $R1, $R5, $R0
1460  # AND $R2, $R0, $R0
7060  # PUT $R3
6800  # HALT