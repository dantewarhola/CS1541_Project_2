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
  "clock_frequency": "3GHz",
  "program": args.program,
  "verbose": 0,
  "output": args.output
})

# changed this to add the configuration parameters to the core instead of the latencies
core.addParams(configuration)
# Configure the memory interface in our CPU to use the standard interface
iface = core.setSubComponent("memory", "memHierarchy.standardInterface")
#iface.addParams({"debug" : 1, "debug_level" : 10})


# Now we add the memory to the simulation
# In this case we're using a simple memory controller (the memory frontend)
# This will map RAM from memory add_range_start (0) to add_range_end (64KiB)
memory = sst.Component("data_memory", "memHierarchy.MemController")
memory.addParams(
{
	'clock':		"1GHz",
	"verbose" : 		2,
	"addr_range_end":	64*1024-1,
})

# Memory access timing model we attach it to the memory backend
#	- this does the specifics of the memory simulation
## SimpleMem has a constant access latency for all accesses
## 64KiB memory with 1000ns access latency
memory_timing = memory.setSubComponent("backend", "memHierarchy.simpleMem")
memory_timing.addParams({
	"access_time" : "1000ns",
	"mem_size" : "64KiB"
})

# Now we need to connect the two components together, the link has a latency
#	and can be assimetric (it's not in this case)
cpu_data_memory_link = sst.Link("cpu_data_memory_link")
cpu_data_memory_link.connect(
	(iface,		"lowlink",	"500ps"),
	(memory,	"highlink",	"500ps")
)

# Enable statistics for all components
sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputConsole")
sst.enableAllStatisticsForAllComponents()


