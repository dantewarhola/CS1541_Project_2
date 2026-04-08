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

## Phase 5: Implement Read Operand

### 5.1 Add issue_order to reservation station:

- Inside of tomasulo.hpp I added int issue_order{-1} to ReservationStation so each instruction can be stamped with the order it was issued
- I also added issue_order = -1 to the clear() method so the field resets when an RS slot is freed

### 5.2 Implement the 3 lookup helpers:

- Added declarations for get_rs_by_global_index, get_fus_for_type, and get_latency_for_type to the protected section of core.hpp
- Implemented get_rs_by_global_index in core.cpp which takes a global RS index and returns a pointer to that RS entry
- Implemented get_fus_for_type in core.cpp which takes a FU type and returns the vector of FUs of that type
- Implemented get_latency_for_type in core.cpp which takes a FU type and returns how many cycles that FU type takes to execute

### 5.3 Implement can_dispatch_oldest:

- Added the declaration for can_dispatch_oldest to the protected section of core.hpp
- Implemented can_dispatch_oldest in core.cpp which scans all RS entries in the same FU pool and checks if any other RS is busy, has all operands ready, and has a lower issue_order than the current instruction
- If an older ready instruction is found it returns false meaning the current instruction cannot dispatch yet
- If no older ready instruction is found it returns true meaning the current instruction is the oldest and can proceed

### 5.4 Implement handle_read_operand:

- Added the declaration for handle_read_operand to the protected section of core.hpp
- Implemented handle_read_operand in core.cpp which is the main handler for the Read Operand stage
- The first thing it does is check if Qj or Qk are not -1, if either is still waiting on a tag the operands are not ready so it reschedules a READ_OPERAND event for the next cycle and returns
- If the operands are ready it then calls can_dispatch_oldest to make sure no older instruction is waiting for the same FU type, if there is it reschedules for the next cycle and returns
- If this instruction is the oldest ready one it searches the correct FU pool for a free unit, if none is found it increments total_stalls, reschedules for the next cycle, and returns
- If a free FU is found it marks it busy, records which FU was assigned on the RS, calculates when it will finish executing, and pushes an EXECUTE event at that cycle

### 5.5 Link READ_OPERAND into tick:

- I added an event queue drain loop inside tick() that runs every cycle before the termination check
- The loop checks if the event at the top of the queue has a timestamp matching the current cycle, if so it pops it off and dispatches it to the correct handler
- Added a check for READ_OPERAND events that calls handle_read_operand with the rs_index from the event
- Left EXECUTE and WRITE_RESULT as empty stubs with TODO comments since those stages are not implemented yet

### 5.6 Set issue_order during issue:

- Added issue_order = next_issue_index to all four FU type blocks in the Issue stage inside tick()
- This was added to the INTEGER, DIVIDER, MULTIPLIER, and LOAD_STORE blocks right after the other RS fields are filled in

### 5.7 register renaming:

- Added register renaming logic to the Issue stage inside tick() right after a free RS is found
- Called get_source_regs() to get the source registers for the instruction
- For each source register checked reg_status to see if the value is ready or if another instruction is still producing it
- If the tag is -1 the value is ready so I copied it into Vj or Vk and incremented total_reg_reads
- If the tag is not -1 a previous instruction is still producing that value so I copied the tag into Qj or Qk instead so the instruction knows to wait
- After handling source registers I updated reg_status for the destination register to point to this RS so future instructions that read this register will know to wait on it

## Phase 6: Implementing the Execution Stage

### 6.1 Implement handle_execute:

- Added the declaration for handle_execute to the protected section of core.hpp
- Implemented handle_execute in core.cpp which takes a global RS index and schedules a WRITE_RESULT event for the next cycle

### 6.2 Add to tick():

- Added handle_execute(current_event.rs_index); to tick() where the TODO is for the execute stage

### 6.3 Test:

- Wrote a two instruction trace with two independent LIZ instructions and ran the simulator
- Confirmed RS 0 dispatched to Execute on cycle 2 and scheduled Write Result on cycle 3
- Confirmed RS 1 dispatched to Execute on cycle 3 and scheduled Write Result on cycle 4
- Confirmed the simulation ended cleanly at cycle 5 with no hanging
- Had to fix a bug in can_dispatch_oldest where it was not skipping RS entries that had already been dispatched to a FU

## Phase 7: Write Result Stage

### 7.1 Implement handle_write_result:
- Added handle_write_result to core.cpp which handles the broadcast, register alias table update, and freeing of the RS and FU
- On broadcast it scans all RS pools (integer, divider, multiplier, L/S) and clears any Qj or Qk that match the completing RS index
- Only clears the register alias table entry if it still points to this RS, since a later instruction may have already overwritten it
- Frees the FU and increments its instruction count, then clears the RS

### 7.2 Update get_rs_by_global_index:
- Added support for L/S global indices so it can look up entries in the ls_rs deque instead of crashing

### 7.3 Wire into tick():
- Replaced the WRITE_RESULT TODO stub with a call to handle_write_result

### 7.4 Test:
- Tested with two independent LIZ instructions plus HALT, got total cycles 6 with 0 stalls which matches hand calculation
- Tested with a RAW dependency chain (two LIZ then ADD that reads both results), got total cycles 8 with 0 stalls
- The ADD correctly waited in Read Operand until both LIZ instructions broadcast their results

## Phase 8: Load/Store Unit (FIFO Queue)

### 8.1 Implement handle_ls_read_operand:
- Added a separate handler for L/S instructions since they use a FIFO queue instead of normal RS dispatch
- Only the head of the queue can dispatch, all other entries wait until they reach the front
- Checks that operands are ready, the L/S FU is free, and no memory operation is currently pending before dispatching
- Dispatches by computing the effective address from Vj and scheduling an Execute event after ls_latency cycles

### 8.2 Update handle_execute for L/S:
- For L/S instructions handle_execute now sends a memory request through memory_wrapper instead of going straight to Write Result
- LW uses memory_wrapper->read() with a callback that stores the loaded value and schedules Write Result
- SW uses memory_wrapper->write() with a callback that schedules Write Result
- Sets memory_pending to true so no other L/S instruction can send a memory request until this one finishes

### 8.3 Update handle_write_result for L/S:
- For L/S instructions the RS is freed by popping the front of the deque instead of calling clear()
- After popping, all L/S global indices shift down by 1 so we fix up tags in reg_status, Qj/Qk in all RS pools, and event queue indices

### 8.4 Update tick() routing:
- READ_OPERAND events now check the FU type and route L/S instructions to handle_ls_read_operand
- Added MEMORY_PENDING event handling
- Termination check now also waits for memory_pending to be false

### 8.5 Test:
- Ran with example_program.m which has SW and LW instructions
- Both memory requests went through correctly (confirmed by memory_wrapper print statements)
- Total cycles was 6031 due to no cache, each memory access taking ~1000 cycles to DRAM

## Phase 9: L1 Cache Integration

### 9.1 Update config.py:
- Read cache associativity and size from the configuration JSON
- Created an SST memHierarchy.Cache component between the CPU and memory
- Set cache_line_size to 16, access_latency_cycles to 1, and L1 to true as specified
- Changed the two links: CPU connects to cache, cache connects to memory
- Also changed the core clock to use the clock value from the configuration JSON instead of hardcoded 3GHz

### 9.2 Test:
- Ran with example_program.m and confirmed the cache is working
- SW was a cold miss (latency 1005 cycles to DRAM), LW was a cache hit (latency 2 cycles)
- Total cycles dropped from 6031 without cache to 1022 with cache
- Cache stats confirmed 1 hit and 1 miss which is correct for a SW followed by a LW to the same address


# Part 2: Tests & Bugs:



## Tests Completed:



## Known Problems / Bugs:
