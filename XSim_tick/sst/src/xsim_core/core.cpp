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
	if(next_issue_index < (int)program.size()){
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
					integer_rs[i].issue_order = next_issue_index;
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
					divider_rs[i].issue_order = next_issue_index;
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
					multiplier_rs[i].issue_order = next_issue_index;
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
				new_rs.issue_order = next_issue_index;
				ls_rs.push_back(new_rs);
				found_rs_index = ls_rs_offset + (int)ls_rs.size() - 1;
			}
		}

		//If a free RS was found, schedule a READ_OPERAND event
		if (found_rs_index != -1){

			//Get the RS entry and source registers for this instruction
			ReservationStation* current_rs = get_rs_by_global_index(found_rs_index);
			SourceRegs source_regs = get_source_regs(instruction, opcode);


			//Check if source register Qj is ready or waiting on a tag
			if (source_regs.rs != -1){
				if (reg_status.at(source_regs.rs).tag == -1){
					current_rs->Vj = registers.at(source_regs.rs); 
					total_reg_reads++;                              
				}
				else{
					current_rs->Qj = reg_status.at(source_regs.rs).tag; 
				}
			}


			//Check if source register Qk is ready or waiting on a tag
			if (source_regs.rt != -1){
				if (reg_status.at(source_regs.rt).tag == -1){
					current_rs->Vk = registers.at(source_regs.rt); 
					total_reg_reads++;                              
				}
				else{
					current_rs->Qk = reg_status.at(source_regs.rt).tag; 
				}
			}

			//If this instruction writes a destination register update reg_status to point to this RS
			if (current_rs->dest_reg != -1){
				reg_status.at(current_rs->dest_reg).tag = found_rs_index;
			}

			//Schedule a READ_OPERAND event for the next cycle
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

	//Process all events that are scheduled to happen on this cycle
	while (!event_queue.empty() && event_queue.top().timestamp == current_cycle){

		//Pop the event off the queue
		Event current_event = event_queue.top();
		event_queue.pop();

		//Dispatch the event to the correct handler based on its stage
		if (current_event.stage == Stage::READ_OPERAND){
			handle_read_operand(current_event.rs_index);
		}
		else if (current_event.stage == Stage::EXECUTE){
			handle_execute(current_event.rs_index);
		}
		else if (current_event.stage == Stage::WRITE_RESULT){
			// TODO: implement in next phase
		}
	}

	// If all instructions have been issued and there are no more events to process then end the simulation
	if (next_issue_index >= (int)program.size() && event_queue.empty()){
		primaryComponentOKToEndSim();
		terminate = true;
	}

	return terminate;
}


// Helper function that takes a global RS index and returns a pointer to the actual RS entry in the right pool
ReservationStation* Core::get_rs_by_global_index(int global_rs_index){
    if (global_rs_index >= int_rs_offset && global_rs_index < div_rs_offset){
        int local_index = global_rs_index - int_rs_offset;
        return &integer_rs.at(local_index);
    }
    else if (global_rs_index >= div_rs_offset && global_rs_index < mul_rs_offset){
        int local_index = global_rs_index - div_rs_offset;
        return &divider_rs.at(local_index);
    }
    else if (global_rs_index >= mul_rs_offset && global_rs_index < ls_rs_offset){
        int local_index = global_rs_index - mul_rs_offset;
        return &multiplier_rs.at(local_index);
    }
	else{
		std::cerr << "Error: Invalid global RS index " << global_rs_index << std::endl;
		exit(EXIT_FAILURE);
	}
}


// Helper function that takes a FU type and returns the vector of FUs of that type
std::vector<FunctionalUnit>& Core::get_fus_for_type(FUType functional_unit_type){
    if (functional_unit_type == FUType::DIVIDER){
        return divider_fus;
    }
    else if (functional_unit_type == FUType::MULTIPLIER){
        return multiplier_fus;
    }
    else{
        return integer_fus;
    }
}


//Helper function that takes a FU type looks how many cycles that FU type takes to execute 
int Core::get_latency_for_type(FUType functional_unit_type){
    if (functional_unit_type == FUType::DIVIDER){
        return config.divider_latency;
    }
    else if (functional_unit_type == FUType::MULTIPLIER){
        return config.multiplier_latency;
    }
    else if (functional_unit_type == FUType::LOAD_STORE){
        return config.ls_latency;
    }
    else{
        return config.integer_latency;
    }
}



//Helper function that checks if this instruction is the oldest ready instruction waiting for this FU type
bool Core::can_dispatch_oldest(int global_rs_index, FUType functional_unit_type){

    //Get the RS entry for the instruction we are checking
    ReservationStation* current_rs = get_rs_by_global_index(global_rs_index);

    //Pick the right pool and offset based on the FU type
    std::vector<ReservationStation>* pool_to_search;
    int pool_offset;

    if (functional_unit_type == FUType::INTEGER){
        pool_to_search = &integer_rs;
        pool_offset = int_rs_offset;
    }
    else if (functional_unit_type == FUType::DIVIDER){
        pool_to_search = &divider_rs;
        pool_offset = div_rs_offset;
    }
    else if (functional_unit_type == FUType::MULTIPLIER){
        pool_to_search = &multiplier_rs;
        pool_offset = mul_rs_offset;
    }
    else{
        //Load store is FIFO so it is always oldest first
        return true;
    }

    //Scan every entry in the pool looking for a ready older instruction
    for (int i = 0; i < (int)pool_to_search->size(); i++){
        ReservationStation& other_rs = pool_to_search->at(i);
        int other_global_index = pool_offset + i;

        //Skip this RS if it is the one we are currently checking
        if (other_global_index == global_rs_index){
            continue;
        }

        //Skip this RS if it is not busy
        if (!other_rs.busy){
            continue;
        }

        //Skip this RS if it still has operands waiting
        if (other_rs.Qj != -1 || other_rs.Qk != -1){
            continue;
        }

		//Skip this RS if it has already been dispatched to a FU
        if (other_rs.assigned_fu != -1){
            continue;
        }

        //If we get here the other RS is busy, ready, and older so we cannot dispatch
        if (other_rs.issue_order < current_rs->issue_order){
            return false;
        }
    }

    return true;
}





//Processes the Read Operand stage for an instruction
void Core::handle_read_operand(int global_rs_index){

    //Get the RS entry for this instruction
    ReservationStation* current_rs = get_rs_by_global_index(global_rs_index);

    //If the RS is not busy something went wrong
    if (!current_rs->busy){
        std::cerr << "Error: handle_read_operand called on non-busy RS " << global_rs_index << std::endl;
        exit(EXIT_FAILURE);
    }

    //Check if source operands are still waiting on a tag
    if (current_rs->Qj != -1 || current_rs->Qk != -1){
        Event wait_event;
        wait_event.timestamp = current_cycle + 1;
        wait_event.stage = Stage::READ_OPERAND;
        wait_event.rs_index = global_rs_index;
        event_queue.push(wait_event);
        return;
    }

    //Operands are ready, check if this is the oldest ready instruction for this FU type
    if (!can_dispatch_oldest(global_rs_index, current_rs->fu_type)){
        Event wait_event;
        wait_event.timestamp = current_cycle + 1;
        wait_event.stage = Stage::READ_OPERAND;
        wait_event.rs_index = global_rs_index;
        event_queue.push(wait_event);
        return;
    }

    //This is the oldest ready instruction, look for a free FU
    std::vector<FunctionalUnit>& fu_pool = get_fus_for_type(current_rs->fu_type);
    int free_fu_index = -1;

    for (int i = 0; i < (int)fu_pool.size(); i++){
        if (!fu_pool.at(i).busy){
            free_fu_index = i;
            break;
        }
    }

    //If no free FU was found wait another cycle and count the stall
    if (free_fu_index == -1){
        total_stalls++;
        Event wait_event;
        wait_event.timestamp = current_cycle + 1;
        wait_event.stage = Stage::READ_OPERAND;
        wait_event.rs_index = global_rs_index;
        event_queue.push(wait_event);
        return;
    }

    //A free FU was found, claim it and schedule an Execute event
    fu_pool.at(free_fu_index).busy = true;
    fu_pool.at(free_fu_index).rs_index = global_rs_index;
    current_rs->assigned_fu = free_fu_index;
    int execution_latency = get_latency_for_type(current_rs->fu_type);
    fu_pool.at(free_fu_index).finish_cycle = current_cycle + execution_latency;
    Event execute_event;
    execute_event.timestamp = current_cycle + execution_latency;
    execute_event.stage = Stage::EXECUTE;
    execute_event.rs_index = global_rs_index;
    event_queue.push(execute_event);
}


//Processes the Execute stage for an instruction
void Core::handle_execute(int global_rs_index){

    //Schedule a WRITE_RESULT event for the next cycle
    Event write_result_event;
    write_result_event.timestamp = current_cycle + 1;
    write_result_event.stage = Stage::WRITE_RESULT;
    write_result_event.rs_index = global_rs_index;
    event_queue.push(write_result_event);
}



}
}