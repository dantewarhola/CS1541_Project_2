#include <sst/core/sst_config.h>
#include <sst/core/interfaces/stdMem.h>

// #include <sst/core/simulation.h>

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
	
	//$ reset values to 0 at the start of the run
	for (int i = 0; i < 8; i++){
        registers[i] = 0;
    }

    pc = 0;
    busy = false;
    waiting_memory = false;
    latency_countdown = 0;
    total_cycles = 0;
	
	//$ gets the output path from the parameters
	output_path = params.find<std::string>("output", "statistics.json");

	// verbose is one of the configuration options for this component
	verbose = params.find<uint32_t>("verbose", verbose);

	// clock_frequency is one of the configuration options for this component
	clock_frequency=params.find<std::string>("clock_frequency",clock_frequency);
	this->registerTimeBase(clock_frequency, true );

	// set instruction latencies
	load_latencies(params);

	// load the program that is to be executed
	load_program(params);

	// Create the SST output with the required verbosity level
	output = new Output("mips_core[@t:@l]: ", verbose, 0, Output::STDOUT);

	// Create a tick function with the frequency specified
	tc = Super::registerClock( clock_frequency, new Clock::Handler2<Core, &Core::tick>(this) );

	output->verbose(CALL_INFO, 1, 0, "Configuring connection to memory...\n");
	// memory_wrapper is used to make write/read requests to the SST simulated memory
	memory_wrapper = loadComponentExtension<MemoryWrapper>(params, output, tc);
	// new MemoryWrapper(*this, params, output, tc);
	output->verbose(CALL_INFO, 1, 0, "Configuration of memory interface completed.\n");

	// SST statistics
	instruction_count = registerStatistic<uint64_t>( "instructions" );

	// tell the simulator not to end without us
	registerAsPrimaryComponent();
	primaryComponentDoNotEndSim();
}

void Core::init(unsigned int phase)
{
	memory_wrapper->init(phase);
}

void Core::setup()
{
	output->output("Setting up.\n");
	std::cout<<"========== STARTED PROGRAM =========="<<std::endl;

}


void Core::finish()
{
	// Nothing to cleanup

	//$ initialized the root variable for my JSON file
	Json::Value root;


	//$ Added the output for the author name to the output file
    root["author"] = "dlw110";

	//$ Added the output for the registers to the output file
    root["registers"]["r0"] = registers[0];
    root["registers"]["r1"] = registers[1];
    root["registers"]["r2"] = registers[2];
    root["registers"]["r3"] = registers[3];
    root["registers"]["r4"] = registers[4];
    root["registers"]["r5"] = registers[5];
    root["registers"]["r6"] = registers[6];
    root["registers"]["r7"] = registers[7];

	//$ Added the output for the instruction counts to the output file
    root["stats"]["add"] = Json::UInt64(instruction_counts[0x00]);
    root["stats"]["sub"] = Json::UInt64(instruction_counts[0x01]);
    root["stats"]["and"] = Json::UInt64(instruction_counts[0x02]);
    root["stats"]["nor"] = Json::UInt64(instruction_counts[0x03]);
    root["stats"]["div"] = Json::UInt64(instruction_counts[0x04]);
    root["stats"]["mul"] = Json::UInt64(instruction_counts[0x05]);
    root["stats"]["mod"] = Json::UInt64(instruction_counts[0x06]);
    root["stats"]["exp"] = Json::UInt64(instruction_counts[0x07]);
    root["stats"]["lw"] = Json::UInt64(instruction_counts[0x08]);
    root["stats"]["sw"] = Json::UInt64(instruction_counts[0x09]);
    root["stats"]["liz"] = Json::UInt64(instruction_counts[0x10]);
    root["stats"]["lis"] = Json::UInt64(instruction_counts[0x11]);
    root["stats"]["lui"] = Json::UInt64(instruction_counts[0x12]);
    root["stats"]["bp"] = Json::UInt64(instruction_counts[0x14]);
    root["stats"]["bn"] = Json::UInt64(instruction_counts[0x15]);
    root["stats"]["bx"] = Json::UInt64(instruction_counts[0x16]);
    root["stats"]["bz"] = Json::UInt64(instruction_counts[0x17]);
    root["stats"]["jr"] = Json::UInt64(instruction_counts[0x0C]);
    root["stats"]["jalr"] = Json::UInt64(instruction_counts[0x13]);
    root["stats"]["j"] = Json::UInt64(instruction_counts[0x18]);
    root["stats"]["halt"] = Json::UInt64(instruction_counts[0x0D]);
    root["stats"]["put"] = Json::UInt64(instruction_counts[0x0E]);


	//$ I am calculating how many total instructions there are in the program
    uint64_t total = 0;
	for (int i = 0; i < 32; i++) {
		total += instruction_counts[i];
	}

    root["stats"]["instructions"] = Json::UInt64(total);
    root["stats"]["cycles"] = Json::UInt64(total_cycles);


	std::ofstream out(output_path);
	Json::StreamWriterBuilder variable;
    variable["indentation"] = "  ";
    out << Json::writeString(variable, root);
    out.close();
	
	std::cout<<"========== FINISHED PROGRAM =========="<<std::endl;
}


/**
 * @brief This function loads the program from the file in parameter: "program"
 */
void Core::load_program(Params &params)
{
	std::string program_path = params.find<std::string>("program","");
	std::ifstream f(program_path);
	if(!f.is_open())
	{
		std::cerr<<"Invalid program file: "<<program_path<<std::endl;
		exit(EXIT_FAILURE);
	}
	for( std::string line; getline( f, line ); )
	{
		std::size_t first_comment=line.find("#");
		std::size_t first_number = line.find_first_of("0123456789ABCDEFabcdef");
		// Ignore comments
		if( first_number<first_comment)
		{
			std::stringstream ss;
			ss << std::hex << line.substr(first_number,4);
			uint16_t instruction;
			ss >> instruction;
			program.push_back(instruction);
		}
	}
	// Print the program for visual inspection
	std::cout<<"Program:"<<std::endl;
	for(uint16_t instruction: program)
	{
		std::cout<<std::hex<<instruction<<std::dec<<std::endl;
	}
}

/**
 * @brief This function loads the latencies from the parameters
 */
void Core::load_latencies(Params &params)
{
	latencies.insert({LIZ, params.find<uint32_t>("liz", 1)});
	names.insert({LIZ, "liz"});
	latencies.insert({LW, params.find<uint32_t>("lw", 1)});
	names.insert({LW, "lw"});
	latencies.insert({SW, params.find<uint32_t>("sw", 1)});
	names.insert({SW, "sw"});
	latencies.insert({PUT, params.find<uint32_t>("put", 1)});
	names.insert({PUT, "put"});
	latencies.insert({HALT, params.find<uint32_t>("halt", 1)});
	names.insert({HALT, "halt"});

	//$ adding names and defaulyt latencies for the new instructions that we added
	latencies.insert({ADD, params.find<uint32_t>("add", 1)});
	names.insert({ADD, "add"});
	latencies.insert({SUB, params.find<uint32_t>("sub", 1)});
	names.insert({SUB, "sub"});
	latencies.insert({AND, params.find<uint32_t>("and", 1)});
	names.insert({AND, "and"});
	latencies.insert({NOR, params.find<uint32_t>("nor", 1)});
	names.insert({NOR, "nor"});
	latencies.insert({DIV, params.find<uint32_t>("div", 1)});
	names.insert({DIV, "div"});
	latencies.insert({MUL, params.find<uint32_t>("mul", 1)});
	names.insert({MUL, "mul"});
	latencies.insert({MOD, params.find<uint32_t>("mod", 1)});
	names.insert({MOD, "mod"});
	latencies.insert({EXP, params.find<uint32_t>("exp", 1)});
	names.insert({EXP, "exp"});
	latencies.insert({JALR, params.find<uint32_t>("jalr", 1)});
	names.insert({JALR, "jalr"});
	latencies.insert({JR, params.find<uint32_t>("jr", 1)});
	names.insert({JR, "jr"});
	latencies.insert({LIS, params.find<uint32_t>("lis", 1)});
	names.insert({LIS, "lis"});
	latencies.insert({LUI, params.find<uint32_t>("lui", 1)});
	names.insert({LUI, "lui"});
	latencies.insert({BP, params.find<uint32_t>("bp", 1)});
	names.insert({BP, "bp"});
	latencies.insert({BN, params.find<uint32_t>("bn", 1)});
	names.insert({BN, "bn"});
	latencies.insert({BX, params.find<uint32_t>("bx", 1)});
	names.insert({BX, "bx"});
	latencies.insert({BZ, params.find<uint32_t>("bz", 1)});
	names.insert({BZ, "bz"});
	latencies.insert({J, params.find<uint32_t>("j", 1)});
	names.insert({J, "j"});
}

bool Core::tick(Cycle_t cycle)
{
	std::cout<<"tick"<<std::endl;
	if(!busy)
	{
		fetch_instruction();
		busy = true;
	}
	// Block waiting for memory!
	if(!waiting_memory)
	{
		if(latency_countdown==0)
		{
			execute_instruction();
			if(instruction_count)
			{
				instruction_count->addData(1);
			}
		}
		else
		{
			latency_countdown--;
		}
	}
	return terminate;
}

void Core::fetch_instruction()
{
	// Better be aligned!!
	uint16_t instruction = program[pc/2];
	uint16_t opcode = instruction >> 11;

	std::cout<<"Fetched "<<names[opcode]<<std::endl;
	try
	{
		latency_countdown = latencies.at(opcode)-1;  //This cycle counts as 1
	}
	catch(std::out_of_range &e)
	{
		std::cerr<<"Unknown instruction: opcode "<<opcode<<std::endl;
		exit(EXIT_FAILURE);
	}
}


//$ Changed the mask from 0x03 to 0x07 to accomodate the change of register bits from 2 to 3
void Core::execute_instruction()
{
	// Better be aligned!!
	uint16_t instruction = program[pc/2];
	uint16_t opcode = instruction >> 11;
	uint16_t rd,rs,rt,imm8;

	//$ Added instruction count and cycle count tracking for output file
	instruction_counts[opcode]++;
	total_cycles += latencies.at(opcode);

	std::cout<<"Running "<<names[opcode]<<std::endl;
	switch (opcode)
	{
		case LW:
			rs = (instruction >> 5) & 0x07;
			rd = (instruction >> 8) & 0x07;
			waiting_memory = true;
			memory_wrapper->read(registers[rs], [this, rd](uint16_t addr, uint16_t data)
			{
				registers[rd]=data;
				pc+=2;
				busy = false;
				waiting_memory = false;
			});
			break;
		case SW:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			waiting_memory = true;
			memory_wrapper->write(registers[rs], registers[rt], [this](uint16_t addr)
			{
				pc+=2;
				busy = false;
				waiting_memory = false;
			});
			break;
		case LIZ:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			registers[rd]=imm8;
			pc+=2;
			busy = false;
			break;
		case PUT:
			rs = (instruction >> 5) & 0x07;
			std::cout<<"Register "<<(int)rs<<" = "<<registers[rs]<<"(unsigned) = "<<(int16_t)registers[rs]<<"(signed)"<<std::endl;
			pc+=2;
			busy = false;
			break;
		case HALT:
			pc+=2;
			primaryComponentOKToEndSim();
			// unregisterExit();
			busy = false;
			break;

	//$ These are the additional R type instructions that we added
		case ADD:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]=registers[rs]+registers[rt];
			pc+=2;
			busy = false;
			break;
		case SUB:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]=registers[rs]-registers[rt];
			pc+=2;
			busy = false;
			break;
		case AND:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]=registers[rs]&registers[rt];
			pc+=2;
			busy = false;
			break;
		case NOR:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]= ~(registers[rs]|registers[rt]);
			pc+=2;
			busy = false;
			break;
		case DIV:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			if (registers[rt] == 0){
				std::cerr<<"Division by zero error"<<std::endl;
				exit(EXIT_FAILURE);
			}
			registers[rd]= (registers[rs]/registers[rt]);
			pc+=2;
			busy = false;
			break;
		case MUL:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]= (registers[rs]*registers[rt]);
			pc+=2;
			busy = false;
			break;
		case MOD:
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd]= (registers[rs]%registers[rt]);
			pc+=2;
			busy = false;
			break;
		case EXP: {
			rs = (instruction >> 5) & 0x07;
			rt = (instruction >> 2) & 0x07;
			rd = (instruction >> 8) & 0x07;

			uint32_t result = 1;
			uint32_t base = registers[rs];
			uint16_t exp  = registers[rt];

			while (exp > 0){
				if (exp & 1){
					result *= base & 0xFFFF;
				}
				base *= base & 0xFFFF;
				exp >>= 1;
			}
			registers[rd] = result;
			pc+=2;
			busy = false;
			break;
		}
		case JALR:
			rs = (instruction >> 5) & 0x07;
			rd = (instruction >> 8) & 0x07;
			registers[rd] = pc+2;
			pc = registers[rs] & 0xFFFE;
			busy = false;
			break;
		case JR:
			rs = (instruction >> 5) & 0x07;
			pc = registers[rs] & 0xFFFE;
			busy = false;
			break;



	//$ These are the additional I type instructions
		case LIS: {
			rd = (instruction >> 8) & 0x07;
			int8_t simm8 = (int8_t)(instruction & 0xFF);
			registers[rd]=(uint16_t)simm8;
			pc+=2;
			busy = false;
			break;
		}
		case LUI:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			registers[rd] = (imm8 << 8) | (registers[rd] & 0x00FF);
			pc+=2;
			busy = false;
			break;
		case BP:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			if (registers[rd] > 0){
				pc = imm8 << 1;
			} else {
				pc+=2;
			}
			busy = false;
			break;
		case BN:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			if (((int16_t)registers[rd]) < 0){
				pc = imm8 << 1;
			} else {
				pc+=2;
			}
			busy = false;
			break;
		case BX:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			if (registers[rd] != 0){
				pc = imm8 << 1;
			} else {
				pc+=2;
			}
			busy = false;
			break;
		case BZ:
			rd = (instruction >> 8) & 0x07;
			imm8 = instruction & 0xFF;
			if (registers[rd] == 0){
				pc = imm8 << 1;
			} else {
				pc+=2;
			}
			busy = false;
			break;

	//$ These are the additional IX type instructions that we added
		case J:{
			uint16_t imm11 = instruction & 0x7FF;
			pc = (pc & 0xF000) | (imm11 << 1);
			busy = false;
			break;
		}

	}
	if(opcode == HALT)
	{
		terminate = true;
	}
}


}
}
