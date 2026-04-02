#include <sst/core/sst_config.h>
#include <sst/core/interfaces/stdMem.h>

#include <sstream>
#include <cstdlib>
#include <fstream>
#include <cmath>

#include <xsim_core/core.hpp>

#include <json/json.h>

namespace XSim
{
namespace Core
{

Core::Core(ComponentId_t id, Params& params):
	Component(id)
{
	for (int i = 0; i < 8; i++) {
		registers[i] = 0;
		reg_status[i].tag = -1;
	}

	output_path = params.find<std::string>("output", "statistics.json");
	verbose = params.find<uint32_t>("verbose", verbose);
	clock_frequency = params.find<std::string>("clock_frequency", clock_frequency);
	this->registerTimeBase(clock_frequency, true);

	load_configuration(params);
	load_program(params);

	names = {
		{ADD, "add"}, {SUB, "sub"}, {AND, "and"}, {NOR, "nor"},
		{DIV, "div"}, {MUL, "mul"}, {MOD, "mod"}, {EXP, "exp"},
		{LW, "lw"}, {SW, "sw"}, {JR, "jr"}, {HALT, "halt"},
		{PUT, "put"}, {LIZ, "liz"}, {LIS, "lis"}, {LUI, "lui"},
		{JALR, "jalr"}, {BP, "bp"}, {BN, "bn"}, {BX, "bx"},
		{BZ, "bz"}, {J, "j"}
	};

	// Create the SST output with the required verbosity level
	output = new Output("tomsim[@t:@l]: ", verbose, 0, Output::STDOUT);

	// Create a tick function with the frequency specified
	tc = Super::registerClock(clock_frequency, new Clock::Handler2<Core, &Core::tick>(this));

	output->verbose(CALL_INFO, 1, 0, "Configuring connection to memory...\n");
	// memory_wrapper is used to make write/read requests to the SST simulated memory
	memory_wrapper = loadComponentExtension<MemoryWrapper>(params, output, tc);
	output->verbose(CALL_INFO, 1, 0, "Configuration of memory interface completed.\n");

	// SST statistics
	instruction_count = registerStatistic<uint64_t>("instructions");

	// tell the simulator not to end without us
	registerAsPrimaryComponent();
	primaryComponentDoNotEndSim();
}

void Core::load_configuration(Params &params)
{
	config.integer_num     = params.find<int>("integer.number", 2);
	config.integer_rs      = params.find<int>("integer.resnumber", 4);
	config.integer_latency = params.find<int>("integer.latency", 1);

	config.divider_num     = params.find<int>("divider.number", 1);
	config.divider_rs      = params.find<int>("divider.resnumber", 2);
	config.divider_latency = params.find<int>("divider.latency", 20);

	config.multiplier_num     = params.find<int>("multiplier.number", 2);
	config.multiplier_rs      = params.find<int>("multiplier.resnumber", 4);
	config.multiplier_latency = params.find<int>("multiplier.latency", 10);

	config.ls_num     = params.find<int>("ls.number", 1);
	config.ls_rs      = params.find<int>("ls.resnumber", 8);
	config.ls_latency = params.find<int>("ls.latency", 3);

	// Global RS index offsets for unique tagging
	int_rs_offset = 0;
	div_rs_offset = config.integer_rs;
	mul_rs_offset = div_rs_offset + config.divider_rs;
	ls_rs_offset  = mul_rs_offset + config.multiplier_rs;
	total_rs_count = ls_rs_offset + config.ls_rs;

	// Allocate reservation stations
	integer_rs.resize(config.integer_rs);
	for (auto &rs : integer_rs) rs.clear();

	divider_rs.resize(config.divider_rs);
	for (auto &rs : divider_rs) rs.clear();

	multiplier_rs.resize(config.multiplier_rs);
	for (auto &rs : multiplier_rs) rs.clear();

	ls_rs.clear();

	// Allocate functional units
	integer_fus.resize(config.integer_num);
	for (auto &fu : integer_fus) { fu.type = FUType::INTEGER; fu.clear(); }

	divider_fus.resize(config.divider_num);
	for (auto &fu : divider_fus) { fu.type = FUType::DIVIDER; fu.clear(); }

	multiplier_fus.resize(config.multiplier_num);
	for (auto &fu : multiplier_fus) { fu.type = FUType::MULTIPLIER; fu.clear(); }

	ls_fu.type = FUType::LOAD_STORE;
	ls_fu.clear();
}

void Core::init(unsigned int phase)
{
	memory_wrapper->init(phase);
}

void Core::setup()
{
	output->output("Setting up.\n");
	std::cout << "========== TOMASULO SIMULATOR ==========" << std::endl;

	std::cout << "Integer FUs: " << config.integer_num
	          << ", RSs: " << config.integer_rs
	          << ", Latency: " << config.integer_latency << std::endl;
	std::cout << "Divider FUs: " << config.divider_num
	          << ", RSs: " << config.divider_rs
	          << ", Latency: " << config.divider_latency << std::endl;
	std::cout << "Multiplier FUs: " << config.multiplier_num
	          << ", RSs: " << config.multiplier_rs
	          << ", Latency: " << config.multiplier_latency << std::endl;
	std::cout << "L/S FUs: " << config.ls_num
	          << ", RSs (FIFO): " << config.ls_rs
	          << ", Latency: " << config.ls_latency << std::endl;

	std::cout << "RS offsets - Int: [" << int_rs_offset << "," << div_rs_offset << ")"
	          << " Div: [" << div_rs_offset << "," << mul_rs_offset << ")"
	          << " Mul: [" << mul_rs_offset << "," << ls_rs_offset << ")"
	          << " LS: [" << ls_rs_offset << "," << total_rs_count << ")" << std::endl;

	std::cout << "Loaded " << program.size() << " instructions:" << std::endl;
	for (size_t i = 0; i < program.size(); i++) {
		uint16_t opcode = program[i] >> 11;
		std::cout << "  [" << i << "] 0x" << std::hex << program[i]
		          << std::dec << " -> " << names[opcode] << std::endl;
	}

	std::cout << "========================================" << std::endl;
}

void Core::finish()
{
	// TODO: Write output statistics JSON (Phase 10)
	std::cout << "========== FINISHED SIMULATION ==========" << std::endl;
	std::cout << "Total cycles: " << current_cycle << std::endl;
	std::cout << "Total stalls: " << total_stalls << std::endl;
	std::cout << "Total reg reads: " << total_reg_reads << std::endl;
}

void Core::load_program(Params &params)
{
	std::string program_path = params.find<std::string>("program", "");
	std::ifstream f(program_path);
	if (!f.is_open()) {
		std::cerr << "Invalid program file: " << program_path << std::endl;
		exit(EXIT_FAILURE);
	}
	for (std::string line; getline(f, line);) {
		std::size_t first_comment = line.find("#");
		std::size_t first_number = line.find_first_of("0123456789ABCDEFabcdef");
		if (first_number < first_comment) {
			std::stringstream ss;
			ss << std::hex << line.substr(first_number, 4);
			uint16_t instruction;
			ss >> instruction;
			program.push_back(instruction);
		}
	}
}






bool Core::tick(Cycle_t cycle){

	current_cycle = cycle;


	// This if statement checks if there are still instructions left in the trace array to issue
	if(next_issue_index < (int)program.size()) {
		uint16_t instruction = program[next_issue_index];
        uint16_t opcode = instruction >> 11;
        FUType fu_type = opcode_to_fu_type(opcode);


		// Start by assuming no free reservation station was found
		int found_rs_index = -1;


		// Each if block checks to see if there is a free reservation station for the instruction to be issued to
		// and if so, it sets the found_rs_index variable to the index of that reservation station. 


		if (fu_type == FUType::INTEGER){
			for (int i = 0; i < (int)integer_rs.size(); i++){
				if (!integer_rs[i].busy){
					found_rs_index = int_rs_offset + i;
					// Mark the RS busy and fill in the fields
					integer_rs[i].busy = true;
					integer_rs[i].instr_index = next_issue_index;
					integer_rs[i].opcode = opcode;
					integer_rs[i].fu_type = fu_type;
					integer_rs[i].dest_reg = get_dest_reg(instruction, opcode);
					break;
				}
			}
		}
		else if (fu_type == FUType::DIVIDER){
			for (int i = 0; i < (int)divider_rs.size(); i++){
				if (!divider_rs[i].busy){
					found_rs_index = div_rs_offset + i;
					// Mark the RS busy and fill in the fields
					divider_rs[i].busy = true;
					divider_rs[i].instr_index = next_issue_index;
					divider_rs[i].opcode = opcode;
					divider_rs[i].fu_type = fu_type;
					divider_rs[i].dest_reg = get_dest_reg(instruction, opcode);
					break;
				}
			}
		}
		else if (fu_type == FUType::MULTIPLIER){
			for (int i = 0; i < (int)multiplier_rs.size(); i++){
				if (!multiplier_rs[i].busy){
					found_rs_index = mul_rs_offset + i;
					// Mark the RS busy and fill in the fields
					multiplier_rs[i].busy = true;
					multiplier_rs[i].instr_index = next_issue_index;
					multiplier_rs[i].opcode = opcode;
					multiplier_rs[i].fu_type = fu_type;
					multiplier_rs[i].dest_reg = get_dest_reg(instruction, opcode);
					break;
				}
			}
		}
		else if (fu_type == FUType::LOAD_STORE){
			if ((int)ls_rs.size() < config.ls_rs){
				ReservationStation new_rs;
				new_rs.busy = true;
				new_rs.instr_index = next_issue_index;
				new_rs.opcode = opcode;
				new_rs.fu_type = fu_type;
				new_rs.dest_reg = get_dest_reg(instruction, opcode);
				ls_rs.push_back(new_rs);
				found_rs_index = ls_rs_offset + (int)ls_rs.size() - 1;
			}
		}
		// If a free RS was found, schedule a READ_OPERAND event
		if (found_rs_index != -1){
			// Schedule a READ_OPERAND event for the next cycle
			Event next_event;
			next_event.timestamp = current_cycle + 1;
			next_event.stage = Stage::READ_OPERAND;
			next_event.rs_index = found_rs_index;
			event_queue.push(next_event);

			// Move to the next instruction
			next_issue_index++;
		}
		// If no free RS was found, the pipeline stalls
		else{
			total_stalls++;
		}
	}

	// If all instructions have been issued and there are no more events to process then end the simulation
	if (next_issue_index >= (int)program.size() && event_queue.empty())
	{
		primaryComponentOKToEndSim();
		terminate = true;
	}

	return terminate;
}

}
}