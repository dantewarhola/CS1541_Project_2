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

# Part 2: Tests & Bugs:

## Tests Completed:



## Known Problems / Bugs:
