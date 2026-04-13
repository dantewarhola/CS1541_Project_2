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

	output = new Output("tomsim[@t:@l]: ", verbose, 0, Output::STDOUT);

	tc = Super::registerClock(clock_frequency, new Clock::Handler2<Core, &Core::tick>(this));

	output->verbose(CALL_INFO, 1, 0, "Configuring connection to memory...\n");
	memory_wrapper = loadComponentExtension<MemoryWrapper>(params, output, tc);
	output->verbose(CALL_INFO, 1, 0, "Configuration of memory interface completed.\n");

	instruction_count = registerStatistic<uint64_t>("instructions");

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

	// set up the global RS index offsets so every RS has a unique tag
	int_rs_offset = 0;
	div_rs_offset = config.integer_rs;
	mul_rs_offset = div_rs_offset + config.divider_rs;
	ls_rs_offset  = mul_rs_offset + config.multiplier_rs;
	total_rs_count = ls_rs_offset + config.ls_rs;

	integer_rs.resize(config.integer_rs);
	for (auto &rs : integer_rs) rs.clear();

	divider_rs.resize(config.divider_rs);
	for (auto &rs : divider_rs) rs.clear();

	multiplier_rs.resize(config.multiplier_rs);
	for (auto &rs : multiplier_rs) rs.clear();

	ls_rs.clear();

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
	Json::Value root;

	root["cycles"] = Json::UInt64(current_cycle);

	// per-FU instruction counts
	Json::Value int_array(Json::arrayValue);
	for (int i = 0; i < (int)integer_fus.size(); i++){
		Json::Value entry;
		entry["id"] = i;
		entry["instructions"] = Json::UInt64(integer_fus[i].instructions_executed);
		int_array.append(entry);
	}
	root["integer"] = int_array;

	Json::Value mul_array(Json::arrayValue);
	for (int i = 0; i < (int)multiplier_fus.size(); i++){
		Json::Value entry;
		entry["id"] = i;
		entry["instructions"] = Json::UInt64(multiplier_fus[i].instructions_executed);
		mul_array.append(entry);
	}
	root["multiplier"] = mul_array;

	Json::Value div_array(Json::arrayValue);
	for (int i = 0; i < (int)divider_fus.size(); i++){
		Json::Value entry;
		entry["id"] = i;
		entry["instructions"] = Json::UInt64(divider_fus[i].instructions_executed);
		div_array.append(entry);
	}
	root["divider"] = div_array;

	Json::Value ls_array(Json::arrayValue);
	Json::Value ls_entry;
	ls_entry["id"] = 0;
	ls_entry["instructions"] = Json::UInt64(ls_fu.instructions_executed);
	ls_array.append(ls_entry);
	root["ls"] = ls_array;

	root["reg reads"] = Json::UInt64(total_reg_reads);
	root["stalls"] = Json::UInt64(total_stalls);

	// write to output file
	std::ofstream out(output_path);
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "  ";
	out << Json::writeString(writer, root);
	out.close();

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

	// === ISSUE STAGE ===
	// try to issue the next instruction if there are any left
	if(next_issue_index < (int)program.size()){
		uint16_t instruction = program[next_issue_index];
        uint16_t opcode = instruction >> 11;
        FUType fu_type = opcode_to_fu_type(opcode);

		int found_rs_index = -1;

		// find a free RS in the right pool and fill it in
		if (fu_type == FUType::INTEGER){
			for (int i = 0; i < (int)integer_rs.size(); i++){
				if (!integer_rs[i].busy){
					found_rs_index = int_rs_offset + i;
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

		if (found_rs_index != -1){
			// register renaming: check source operands
			ReservationStation* current_rs = get_rs_by_global_index(found_rs_index);
			SourceRegs source_regs = get_source_regs(instruction, opcode);

			if (source_regs.rs != -1){
				if (reg_status.at(source_regs.rs).tag == -1){
					current_rs->Vj = registers.at(source_regs.rs);
					total_reg_reads++;
				}
				else{
					current_rs->Qj = reg_status.at(source_regs.rs).tag;
				}
			}

			if (source_regs.rt != -1){
				if (reg_status.at(source_regs.rt).tag == -1){
					current_rs->Vk = registers.at(source_regs.rt);
					total_reg_reads++;
				}
				else{
					current_rs->Qk = reg_status.at(source_regs.rt).tag;
				}
			}

			// update the register alias table for the destination
			if (current_rs->dest_reg != -1){
				reg_status.at(current_rs->dest_reg).tag = found_rs_index;
			}

			// schedule read operand for next cycle
			Event next_event;
			next_event.timestamp = current_cycle + 1;
			next_event.stage = Stage::READ_OPERAND;
			next_event.rs_index = found_rs_index;
			event_queue.push(next_event);

			next_issue_index++;
		}
		else{
			total_stalls++;
		}
	}

	// === EVENT PROCESSING ===
	// drain all events scheduled for this cycle
	while (!event_queue.empty() && event_queue.top().timestamp == current_cycle){
		Event current_event = event_queue.top();
		event_queue.pop();

		if (current_event.stage == Stage::READ_OPERAND){
			ReservationStation* rs = get_rs_by_global_index(current_event.rs_index);
			if (rs->fu_type == FUType::LOAD_STORE){
				handle_ls_read_operand(current_event.rs_index);
			}
			else{
				handle_read_operand(current_event.rs_index);
			}
		}
		else if (current_event.stage == Stage::EXECUTE){
			handle_execute(current_event.rs_index);
		}
		else if (current_event.stage == Stage::WRITE_RESULT){
			handle_write_result(current_event.rs_index);
		}
		else if (current_event.stage == Stage::MEMORY_PENDING){
			handle_memory_response(current_event.rs_index);
		}
	}

	// end simulation when everything is done
	if (next_issue_index >= (int)program.size() && event_queue.empty() && !memory_pending){
		primaryComponentOKToEndSim();
		terminate = true;
	}

	return terminate;
}


ReservationStation* Core::get_rs_by_global_index(int global_rs_index){
    if (global_rs_index >= int_rs_offset && global_rs_index < div_rs_offset){
        return &integer_rs.at(global_rs_index - int_rs_offset);
    }
    else if (global_rs_index >= div_rs_offset && global_rs_index < mul_rs_offset){
        return &divider_rs.at(global_rs_index - div_rs_offset);
    }
    else if (global_rs_index >= mul_rs_offset && global_rs_index < ls_rs_offset){
        return &multiplier_rs.at(global_rs_index - mul_rs_offset);
    }
    else if (global_rs_index >= ls_rs_offset && global_rs_index < total_rs_count){
        int local_index = global_rs_index - ls_rs_offset;
        if (local_index < (int)ls_rs.size()){
            return &ls_rs.at(local_index);
        }
        std::cerr << "Error: L/S RS index " << local_index << " out of range (size=" << ls_rs.size() << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
    else{
        std::cerr << "Error: Invalid global RS index " << global_rs_index << std::endl;
        exit(EXIT_FAILURE);
    }
}


std::vector<FunctionalUnit>& Core::get_fus_for_type(FUType fu_type){
    if (fu_type == FUType::DIVIDER) return divider_fus;
    else if (fu_type == FUType::MULTIPLIER) return multiplier_fus;
    else return integer_fus;
}


int Core::get_latency_for_type(FUType fu_type){
    if (fu_type == FUType::DIVIDER) return config.divider_latency;
    else if (fu_type == FUType::MULTIPLIER) return config.multiplier_latency;
    else if (fu_type == FUType::LOAD_STORE) return config.ls_latency;
    else return config.integer_latency;
}


bool Core::can_dispatch_oldest(int global_rs_index, FUType fu_type){
    ReservationStation* current_rs = get_rs_by_global_index(global_rs_index);

    // L/S is FIFO so dispatch order is already enforced
    if (fu_type == FUType::LOAD_STORE) return true;

    std::vector<ReservationStation>* pool;
    int pool_offset;

    if (fu_type == FUType::INTEGER){
        pool = &integer_rs;
        pool_offset = int_rs_offset;
    }
    else if (fu_type == FUType::DIVIDER){
        pool = &divider_rs;
        pool_offset = div_rs_offset;
    }
    else{
        pool = &multiplier_rs;
        pool_offset = mul_rs_offset;
    }

    // check if any older instruction in the same pool is also ready and undispatched
    for (int i = 0; i < (int)pool->size(); i++){
        ReservationStation& other = pool->at(i);
        int other_idx = pool_offset + i;

        if (other_idx == global_rs_index) continue;
        if (!other.busy) continue;
        if (other.Qj != -1 || other.Qk != -1) continue;
        if (other.assigned_fu != -1) continue;

        if (other.issue_order < current_rs->issue_order) return false;
    }

    return true;
}


void Core::handle_read_operand(int global_rs_index){
    ReservationStation* rs = get_rs_by_global_index(global_rs_index);

    if (!rs->busy){
        std::cerr << "Error: handle_read_operand called on non-busy RS " << global_rs_index << std::endl;
        exit(EXIT_FAILURE);
    }

    // wait if operands arent ready yet
    if (rs->Qj != -1 || rs->Qk != -1){
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    // wait if an older instruction is also ready for the same FU type
    if (!can_dispatch_oldest(global_rs_index, rs->fu_type)){
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    // find a free FU
    std::vector<FunctionalUnit>& fus = get_fus_for_type(rs->fu_type);
    int free_fu = -1;
    for (int i = 0; i < (int)fus.size(); i++){
        if (!fus[i].busy){
            free_fu = i;
            break;
        }
    }

    // no free FU, stall
    if (free_fu == -1){
        total_stalls++;
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    // dispatch to the FU
    fus[free_fu].busy = true;
    fus[free_fu].rs_index = global_rs_index;
    rs->assigned_fu = free_fu;

    int latency = get_latency_for_type(rs->fu_type);
    fus[free_fu].finish_cycle = current_cycle + latency;

    Event e;
    e.timestamp = current_cycle + latency;
    e.stage = Stage::EXECUTE;
    e.rs_index = global_rs_index;
    event_queue.push(e);
}


void Core::handle_ls_read_operand(int global_rs_index){
    if (ls_rs.empty()){
        std::cerr << "Error: handle_ls_read_operand called but L/S queue is empty" << std::endl;
        exit(EXIT_FAILURE);
    }

    // only the head of the FIFO can dispatch
    int head_idx = ls_rs_offset;
    if (global_rs_index != head_idx){
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    ReservationStation& head = ls_rs.front();

    // wait for operands
    if (head.Qj != -1 || head.Qk != -1){
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    // wait if the L/S FU is busy or a memory op is still pending
    if (ls_fu.busy || memory_pending){
        Event e;
        e.timestamp = current_cycle + 1;
        e.stage = Stage::READ_OPERAND;
        e.rs_index = global_rs_index;
        event_queue.push(e);
        return;
    }

    // dispatch: compute effective address
    ls_fu.busy = true;
    ls_fu.rs_index = global_rs_index;
    head.assigned_fu = 0;
    head.address = head.Vj;

    int latency = get_latency_for_type(FUType::LOAD_STORE);
    ls_fu.finish_cycle = current_cycle + latency;

    Event e;
    e.timestamp = current_cycle + latency;
    e.stage = Stage::EXECUTE;
    e.rs_index = global_rs_index;
    event_queue.push(e);
}


void Core::handle_execute(int global_rs_index){
    ReservationStation* rs = get_rs_by_global_index(global_rs_index);

    // L/S: send the memory request after address computation
    if (rs->fu_type == FUType::LOAD_STORE){
        memory_pending = true;
        memory_pending_rs = global_rs_index;

        if (rs->opcode == LW){
            memory_wrapper->read(rs->address, [this, global_rs_index](uint16_t addr, uint16_t data){
                ReservationStation* load_rs = get_rs_by_global_index(global_rs_index);
                load_rs->Vk = data;

                Event e;
                e.timestamp = current_cycle + 1;
                e.stage = Stage::WRITE_RESULT;
                e.rs_index = global_rs_index;
                event_queue.push(e);

                memory_pending = false;
                memory_pending_rs = -1;
            });
        }
        else if (rs->opcode == SW){
            memory_wrapper->write(rs->address, rs->Vk, [this, global_rs_index](uint16_t addr){
                Event e;
                e.timestamp = current_cycle + 1;
                e.stage = Stage::WRITE_RESULT;
                e.rs_index = global_rs_index;
                event_queue.push(e);

                memory_pending = false;
                memory_pending_rs = -1;
            });
        }
        return;
    }

    // non-L/S: just schedule write result
    Event e;
    e.timestamp = current_cycle + 1;
    e.stage = Stage::WRITE_RESULT;
    e.rs_index = global_rs_index;
    event_queue.push(e);
}


void Core::handle_memory_response(int global_rs_index){
    // memory responses handled via callbacks in handle_execute
}


void Core::handle_write_result(int global_rs_index){
    ReservationStation* rs = get_rs_by_global_index(global_rs_index);

    if (!rs->busy){
        std::cerr << "Error: handle_write_result called on non-busy RS " << global_rs_index << std::endl;
        exit(EXIT_FAILURE);
    }

    // broadcast: clear tags in all RS pools waiting on this result
    for (auto &r : integer_rs){
        if (r.busy){
            if (r.Qj == global_rs_index) r.Qj = -1;
            if (r.Qk == global_rs_index) r.Qk = -1;
        }
    }
    for (auto &r : divider_rs){
        if (r.busy){
            if (r.Qj == global_rs_index) r.Qj = -1;
            if (r.Qk == global_rs_index) r.Qk = -1;
        }
    }
    for (auto &r : multiplier_rs){
        if (r.busy){
            if (r.Qj == global_rs_index) r.Qj = -1;
            if (r.Qk == global_rs_index) r.Qk = -1;
        }
    }
    for (auto &r : ls_rs){
        if (r.busy){
            if (r.Qj == global_rs_index) r.Qj = -1;
            if (r.Qk == global_rs_index) r.Qk = -1;
        }
    }

    // update register alias table
    if (rs->dest_reg != -1){
        if (reg_status.at(rs->dest_reg).tag == global_rs_index){
            reg_status.at(rs->dest_reg).tag = -1;
        }
    }

    // free the FU
    if (rs->fu_type != FUType::LOAD_STORE){
        std::vector<FunctionalUnit>& fus = get_fus_for_type(rs->fu_type);
        if (rs->assigned_fu != -1 && rs->assigned_fu < (int)fus.size()){
            fus[rs->assigned_fu].busy = false;
            fus[rs->assigned_fu].rs_index = -1;
            fus[rs->assigned_fu].instructions_executed++;
        }
    }
    else{
        ls_fu.busy = false;
        ls_fu.rs_index = -1;
        ls_fu.instructions_executed++;
    }

    // free the RS
    if (rs->fu_type == FUType::LOAD_STORE){
        if (!ls_rs.empty()){
            ls_rs.pop_front();

            // popping the front shifts all L/S indices down by 1
            // fix up tags in reg_status, Qj/Qk in all pools, and events in the queue
            for (int i = 0; i < 8; i++){
                if (reg_status[i].tag > ls_rs_offset && reg_status[i].tag < ls_rs_offset + (int)ls_rs.size() + 1){
                    reg_status[i].tag--;
                }
            }
            for (auto &r : integer_rs){
                if (r.busy){
                    if (r.Qj > ls_rs_offset && r.Qj < ls_rs_offset + (int)ls_rs.size() + 1) r.Qj--;
                    if (r.Qk > ls_rs_offset && r.Qk < ls_rs_offset + (int)ls_rs.size() + 1) r.Qk--;
                }
            }
            for (auto &r : divider_rs){
                if (r.busy){
                    if (r.Qj > ls_rs_offset && r.Qj < ls_rs_offset + (int)ls_rs.size() + 1) r.Qj--;
                    if (r.Qk > ls_rs_offset && r.Qk < ls_rs_offset + (int)ls_rs.size() + 1) r.Qk--;
                }
            }
            for (auto &r : multiplier_rs){
                if (r.busy){
                    if (r.Qj > ls_rs_offset && r.Qj < ls_rs_offset + (int)ls_rs.size() + 1) r.Qj--;
                    if (r.Qk > ls_rs_offset && r.Qk < ls_rs_offset + (int)ls_rs.size() + 1) r.Qk--;
                }
            }
            for (auto &r : ls_rs){
                if (r.busy){
                    if (r.Qj > ls_rs_offset && r.Qj < ls_rs_offset + (int)ls_rs.size() + 1) r.Qj--;
                    if (r.Qk > ls_rs_offset && r.Qk < ls_rs_offset + (int)ls_rs.size() + 1) r.Qk--;
                }
            }

            // rebuild the event queue to fix L/S RS indices
            std::priority_queue<Event, std::vector<Event>, std::greater<Event>> new_queue;
            while (!event_queue.empty()){
                Event ev = event_queue.top();
                event_queue.pop();
                if (ev.rs_index > ls_rs_offset && ev.rs_index < ls_rs_offset + (int)ls_rs.size() + 1){
                    ev.rs_index--;
                }
                new_queue.push(ev);
            }
            event_queue = new_queue;
        }
    }
    else{
        rs->clear();
    }
}



}
}