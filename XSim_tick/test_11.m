#$ Tests J

# Load 99 into R0
801F # LIZ R0, 31 
C003 # J to instruction at address 4 
80FF # LIZ R0, 255 
8063 # LIZ R0, 99
7000 # PUT R0
6800 # HALT