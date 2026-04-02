# Implementation Notes & README

### **Name: Dante Warhola**

### **Email: dlw110@pitt.edu**

# **Document & Project Overview:**

TBD

# Part 1: Implementation Notes:

TBD

## Phase 1: Add —configuration tag to command line

- The instructions state the command line needs to take 3 files to run: Program, Config, and Output
- The old code takes Latencies, but not Config

### 1.1 Change parser arg:
- Inside of config.py I was able to change the parser argument from latencies to configuration so the program now accepts --configuration

### 1.2 Replace the print & JSON load:
- Inside of config.py I needed to change the print statement and also the JSON load so that instead of loading the latencies file it will now load the configuration file

### 1.3 Add config to core:
- All I did was change core.addParams(latencies) to core.addParams(configuration) so that the configuration parameter gets added to the core instead of the latencies since we aren't using that anymore

### 1.4 Test:
- I tested my implementation and I was able to confirm that Phase 1 works and that --configuration works and is parsed correctly when the program is run


## Phase 2: Data Structures & Core Skeleton
* The spec requires data structures for reservation stations, functional units, register renaming, and an event system
* Created a new file tomasulo.hpp to hold all the structs separate from core.hpp

### 2.1 Define structs:
* Created structs for ReservationStation, FunctionalUnit, RegisterStatus, Event, and TomasuloConfig
* Added helper functions to map opcodes to their FU type and to extract source/destination registers from instructions

### 2.2 Update core.hpp:
* Replaced the old Project 1 in-order pipeline state with the new Tomasulo data structures
* Added vectors for each RS pool, arrays for each FU type, a register alias table, and an event queue

### 2.3 Update core.cpp:
* Added load_configuration() to read the FU counts, RS counts, and latencies from the JSON config
* Allocates all RS pools and FU arrays based on the config values
* setup() prints everything so we can verify sizes match the config

### 2.4 Test:
* Built and ran with configuration.json, confirmed all values printed in setup() match the config file
* Confirmed the instruction trace loads and decodes correctly

## **Phase 3: Instruction Trace Loading**

- Reused load_program() from Project 1 to load the hex trace into the program
- The parser handles inline comments and blank lines
- setup() prints each instruction index, hex value, and decoded opcode name

### 3.1 Test:

- Confirmed in 2.4 that the test file loads and decodes correctly

## Phase 4: Issue Stage

### 4.1 Extract opcode and determine FU type:

- Inside of tick() in core.cpp I added a check to see if there are still instructions left in the trace array to issue
- I extracted the instruction from the program vector using next_issue_index and then extracted the opcode by shifting the instruction right 11 bits
- I then called opcode_to_fu_type() with the opcode to determine which RS pool the instruction belongs to so we know where to look for a free reservation station

### 4.2 Find a free reservation station:

- After determining the FU type I needed to search the correct RS pool for a free reservation station
- I started found_rs_index at -1 to show that no free RS has been found yet
- For INTEGER, DIVIDER, and MULTIPLIER I looped through each RS pool and looked for the first entry where busy is false, once found I recorded the global RS index using the offset and broke out of the loop
- For LOAD_STORE it works differently because it is a FIFO queue so instead of searching I just checked if the current size of the queue is less than the max allowed and if so recorded where the new entry would go at the back of the queue

### 4.3 Handle the free RS case:

- Inside each FU type if block I combined the search and fill into one step so that when a free RS is found it is immediately marked busy and filled in with the instruction information (instr_index, opcode, fu_type, dest_reg)
- For LOAD_STORE I created a new ReservationStation, filled in its fields, and pushed it onto the back of the FIFO queue, then set found_rs_index to the correct global index using ls_rs_offset and the new size minus 1
- After all the FU type blocks I added a check for found_rs_index not being -1, and if so I created a READ_OPERAND event with the timestamp set to the next cycle and pushed it onto the event queue, then advanced next_issue_index to move to the next instruction

### 4.4 Handle ending the simulation:

- After the issue stage I added a check to see if all instructions have been issued and the event queue is empty
- If both are true I called primaryComponentOKToEndSim() to tell SST that the simulation is done and set terminate to true so tick() stops being called

### 4.5 Test:

- I created two test traces to test the issue stage
- phase_4_1.m had 4 instructions which is less than the 4 integer RSs available, confirmed that all 4 instructions issued one per cycle with no stalls and the simulation ended cleanly with Total cycles: 4 and Total stalls: 0
- phase_4_2.m had 6 instructions with 5 LIZ instructions to overflow the 4 integer RSs, confirmed that the first 4 instructions issued fine and the 5th instruction stalled every cycle because the RSs never freed up since execute is not yet implemented, Total stalls confirmed to be incrementing correctly
# Part 2: Tests & Bugs:



## Tests Completed:



## Known Problems / Bugs:
