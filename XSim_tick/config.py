#########################################
#		Inputs to the simulation		#
#########################################
import argparse
parser = argparse.ArgumentParser(description='Configuration options for this simulation.')

parser.add_argument('--program',
                    dest="program",
                    required=True,
                    action='store',
                    help='The program to run in the simulator')

# I changed the name of the latencies file to configuration so the CL can parase the new file
parser.add_argument('--configuration',
                    dest="configuration",
                    required=True,
                    action='store',
                    help='The json file with the simulation configuration')

#$ added the parse for the output file 
parser.add_argument('--output',
                    dest="output",
                    required=True,
                    action='store',
                    help='The output file for final registers and stats')

# Parse arguments
args = parser.parse_args()

# Changed this print to show the new configuration file
## Print info ##
print("Running simulator using program "+args.program)
print("Configuration in  "+args.configuration)


# Changed this JSON load to now load the config file instead of the latencies file
#####################################
#	Load JSON file with configuration	#
#####################################
import json
with open(args.configuration, 'r') as inp_file:
  configuration=json.load(inp_file)
print("Configuration:")
print(json.dumps(configuration, indent=2))


# Now the simulation
import sst

# Add our core to the simulation!
#$ Had to pass the output to the core 
core = sst.Component("XSim","XSim.Core")
core.addParams({
  "clock_frequency": configuration.get("clock", "1GHz"),
  "program": args.program,
  "verbose": 0,
  "output": args.output
})

# changed this to add the configuration parameters to the core instead of the latencies
core.addParams(configuration)
# Configure the memory interface in our CPU to use the standard interface
iface = core.setSubComponent("memory", "memHierarchy.standardInterface")


#$ Read cache settings from the configuration JSON
cache_assoc = configuration.get("cache", {}).get("associativity", 1)
cache_size = configuration.get("cache", {}).get("size", "4096B")
clock_freq = configuration.get("clock", "1GHz")

#$ L1 data cache between CPU and memory
cache = sst.Component("l1_cache", "memHierarchy.Cache")
cache.addParams({
	"cache_frequency":	clock_freq,
	"cache_size":		cache_size,
	"associativity":	cache_assoc,
	"cache_line_size":	16,
	"access_latency_cycles": 1,
	"L1":			True,
	"replacement_policy": "lru"
})


# Now we add the memory to the simulation
memory = sst.Component("data_memory", "memHierarchy.MemController")
memory.addParams(
{
	'clock':		"1GHz",
	"verbose" : 		2,
	"addr_range_end":	64*1024-1,
})

# Memory access timing model
memory_timing = memory.setSubComponent("backend", "memHierarchy.simpleMem")
memory_timing.addParams({
	"access_time" : "1000ns",
	"mem_size" : "64KiB"
})

#$ Link CPU to L1 cache
cpu_cache_link = sst.Link("cpu_cache_link")
cpu_cache_link.connect(
	(iface,		"lowlink",	"500ps"),
	(cache,		"highlink",	"500ps")
)

#$ Link L1 cache to memory
cache_memory_link = sst.Link("cache_memory_link")
cache_memory_link.connect(
	(cache,		"lowlink",	"500ps"),
	(memory,	"highlink",	"500ps")
)

# Enable statistics for all components
sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputConsole")
sst.enableAllStatisticsForAllComponents()