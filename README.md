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



# Part 2: Tests & Bugs:

## Tests Completed:



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