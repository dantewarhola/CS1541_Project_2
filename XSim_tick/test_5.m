#$ Created this test code to test JALR,JR:

810A   # LIZ r1,0x0A  (jump target byte address)

9F20   # JALR r7,r1   (call subroutine)

8037   # LIZ r0,55
7000   # PUT r0
6800   # HALT

# --- subroutine at address 0x30 ---
8063   # LIZ r0,99
7000   # PUT r0
60E0   # JR r7
6800   # HALT