# Implementation Notes & README

### **Name: Dante Warhola**

### **Email: dlw110@pitt.edu**

# **Document & Project Overview:**

This project implements a functional instruction set for the X ISA. This simulator accepts a hex encoded instruction, executes its with configurable instruction latencies, and outputs a stats JSON file that contains the final register state, execution counts per instruction, and a total cycle count.

This document provides the reader with my implementation notes for this project. It walks through each phase of the project, and the decisions I made for each phase. It also included all the tests ran for the project and any bugs found and how they were solved.

# Part 1: Implementation Notes:

**Phase 1.1:** 

- First thing that I had to do was add output as a command line argument so that the program can print the stats to a file once we get to that point.
- I added the parser.add_argument for output inside of config.py
- I passed the output argument to the core so it can track stats later

**Phase 1.2:** 

- I need to make sure that all 8 registers are accessible by the code and that RS,RT, and RD are all 3 bit fields long.
- The code right now only sees r0 - r3 because of the & 0x03 in the execute instruction function
- I changed it from 0x03 to 0x07 so that it keeps the last 3 bits after masking
    - 0x03 → 011 (this one keeps the last 2)
    - 0x07 → 111 (this one keeps the last 3)
- I created a simple test code that loaded 31 into R7 and halted.
    - This code ran and gave me the output of: Register 7 = 31(unsigned) = 31(signed)

**Phase 1.3:** 

- I need to add all the opcodes for the other instructions since the only ones available right now are LW,SW,HALT,PUT, and LIZ.
- Missing Instructions:
    - R: add, sub, and, nor, div, mul, mod, exp, jalr, jr
    - I: lis, lui, bp, bn, bx, bz
    - IX: j

**Phase 1.4:** 

- I gotta go add the actual logic for the R type instructions
- ADD, SUB, AND, NOR:
    - I used SW as kind of a guide. I see that we needed to add in what we need for the instruction (rs,rt, or rd) and all of the instructions have pc+=2, then a busy = false, and then the break
        - Trying to test the ADD instruction and running into error where it is not recognizing the instruction and opcode = 0. The hex I'm giving it (0204) is being represented as 204.
        - The test was sending a blank fetch and I realized I need to add all the names and default latencies of the instructions.
- After adding names and latencies everything seems to be working
- DIV:
    - I used the same format as the other 4, but I know for DIV there is a special divide by 0 error that I need to handle
    - I did it using a if statement to see if rd == 0 and if it did, the program printed a error and then exited using EXIT_FAILURE
- MUL, MOD:
    - Very easy to implement just using the appropriate character to designate their operation
- EXP:
    - this one took me a little bit of time figuring out. I knew that I needed all 3 registers and then I needed to have one as a output, one as a base, and one as the exponent
        - I created a result, base, and exp variable
    - I created a loop inside that does the following
        - Loop through the exponent bits one at a time
        - Multiply result only when the bit is a 1
        - Square the base each step
        - Shift exponent right
        - Stop when the exponent becomes 0
- JALR, JR
    - These were pretty simple to figure out, I just looked at the description of the instruction and followed it word for word
    - I pulled in the registers needed, and then did what it wanted me to do
- TESTING
    - test_3
        - Tests ADD,SUB,AND,NOR
        - Implemented correct
    - test_4
        - Tests MUL,DIV,MOD,EXP
        - Implemented correct
    - test_5
        - Tests JALR,JR
        - Implemented correct

**Phase 1.5:** 

- I forgot to do this when adding the opcodes so when I went to test the add instruction it was not working.
    - Added all names and default latencies
- While I’m here I can add the latencies for div,mod,exp, and mul to the JSON file
    - I added the latencies to the JSON file

**Phase 1.6:** 

- Adding I type instructions to the core using the descriptions to help build them
- LIS:
    - Copy and pasting the code from the LIZ, but the difference between them is that LIZ uses zero extension where LIS uses sign extension
    - So the difference in the code is that you just needed to make the imm8 signed
        - Needed to add to the code and have the imm8 to be signed 8 integer and then make the register = signed 8 immediate but unsigned 16 bit
- LUI:
    - Same setup as the rest, pulling in rd and creating imm8
    - Making the LUI you need to take the upper 8 bits from the immediate
- BP,BN,BX,BZ
    - All have the same format but the if statement condition changes for each instruction
    - Tested all 4 instructions and they are verified working

**Phase 1.7:**

- Adding the last instruction to the core.cpp
- J:
    - Same format as the other instructions with the busy = false, and the break
    - Do not need to pull other registers into this instruction
    - The directions for the instruction say the following:
        - PC $=$ PC $[15: 12] \quad . \quad($ imm11 $\ll 1)$
    - This is saying to keep the top 4 bits of where you currently are, and replace the bottom 12 bits with imm11 << 1
    - Used 0x7FF to extract imm11
    - Used 0xF000 to get the top 4 bits

**Phase 2.1:**

- I added the latencies to latencies.json that were explicitly stated in the instructions
- If an instruction is not mentioned in the .json file, it is assumed a latency of 1

**Phase 2.2:** 

Need to generate and configure the output file and the needed json file setup.

- I added tracking for the instruction count and total cycles so I can use them in the output file later.
    - Added instruction_count to keep track of each instruction’s count
    - Added total_cycles to keep track of the number of cycles that are executed
    - Added output_path to store the path where the output file will be sent to
- Added the output file path inside of Core::Core so the program knows where to write the output file when the program finishes
- I added the tracking of the cycle count and instruction count inside of the execute instruction function so we can properly track those stats for the output file.
    - Used instruction_counts[opcode]++ so each instruction gets counted and sorted based on their opcode
    - Used total_cycles += latencies.at(opcode) so the cycle count increases based on the instruction’s latency
- Created the JSON format to output the stats after each test file is run
    - Normal formatting and made it look like the example that professor gave us
    - Tested the stats file and ran into a problem though
        - I ran 2 different tests and the registers are not being set back to 0 after each run
        - So registers are filled with old values
        - Added a for loop that resets the value and also added rests for the other things just in case
    - Fixed the error, and now stats json build works

**Phase 2.3:**

- Testing all instructions to see if stats & outputs are right
    - LUI instruction is not working, its only keeping the upper 8 bits and making the lower 8 0’s
    - $rd←imm8.$rd7..$rd0
    - Its supposed to take the upper 8 bits of the new value and then have the lower 8 bits be the origional values lower 8
    - I fixed it by taking the value in the register and only taking the bottom 8 bits and then combining the two.
- The program runs smooth, and all instructions are working as intended.

# Part 2: Tests & Bugs:

## Tests Completed:

- Test 1 - LIZ, PUT, HALT (passed)
- Test 2 - ADD (passed)
- Test 3 - SUB, AND, NOR (passed)
- Test 4 - MUL,DIV,MOD,EXP (passed)
- Test 5 - JALR, JR (passed)
- Test 6 - LIZ, LIS (passed)
- Test 7 - BP (passed)
- Test 8 - BN (passed)
- Test 9 - BX (passed)
- Test 10 - BZ (passed)
- Test 11 - J (passed)
- Test 12 - LUI, SW, LW (failed) (passed)

## Known Problems / Bugs:

- **Initial LUI Bug (FIXED):**
    - Originally overwrote lower 8 bits with zeros instead of preserving them
    - Fixed by combining upper immediate with existing lower register bits
- **Register Reset Issue (FIXED):**
    - Registers were not resetting between runs, causing incorrect outputs
    - Fixed by adding reset loop for registers and stats so that after every run, all of the registers and stats reset
- **Opcode Recognition Issue (FIXED):**
    - Instructions initially not recognized due to missing names/latencies
    - Fixed by adding all instruction names and default latencies