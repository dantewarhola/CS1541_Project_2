# Test 3: Load/Store heavy
# Tests: FIFO ordering, multiple memory accesses, cache hits/misses
801F  # LIZ $R0, 31
8310  # LIZ $R3, 16
8520  # LIZ $R5, 32
4860  # SW $R0, $R3 (store 31 to addr in R3)
4C60  # SW $R0, $R5 (store 31 to addr in R5)
4360  # LW $R3, $R3 (load from addr in R3)
4560  # LW $R5, $R5 (load from addr in R5)
7060  # PUT $R3
6800  # HALT