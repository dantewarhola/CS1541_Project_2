#$ Tests LUI, SW, and LW

8034  # LIZ r0, 0x34       
9012  # LUI r0, 0x12      
8104  # LIZ r1, 0x04     
4820  # SW r0, r1         
4220  # LW r2, r1          
7040  # PUT r2
6800  # HALT