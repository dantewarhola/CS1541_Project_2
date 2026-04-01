#pragma once


#include <sst/core/component.h>
// SST datatypes
#include <sst/core/sst_types.h>
// SST output to print information
#include <sst/core/output.h>
// SST interface for memory
#include <sst/core/interfaces/stdMem.h>
// SST statistics
#include <sst/core/statapi/stataccumulator.h>

#include <xsim_core/memory_wrapper.hpp>
#include <xsim_core/opcodes.hpp>
#include <xsim_core/tomasulo.hpp>

#include <queue>


namespace XSim
{
namespace Core
{

class Core: public SST::Component
{
	public:
		// SST registration
		SST_ELI_REGISTER_COMPONENT(
			Core,
			"XSim",
			"Core",
			SST_ELI_ELEMENT_VERSION(1,0,0),
			"Simple MIPS-based simulator",
			COMPONENT_CATEGORY_PROCESSOR
		)

		SST_ELI_DOCUMENT_PARAMS(
			{ "verbose", "(uint) Verbosity for debugging.", "0" },
			{ "clock_frequency", "(string) Sets the clock of the core in Hz", "0"} ,
			{ "program", "(infile) Path to program to be executed by the simulator", "REQUIRED"} ,
			{ "output", "(outfile) Path to output file for statistics", "statistics.json"},
			{ "integer.number", "(uint) Number of integer FUs", "2"},
			{ "integer.resnumber", "(uint) Number of integer reservation stations", "4"},
			{ "integer.latency", "(uint) Integer FU latency in cycles", "1"},
			{ "divider.number", "(uint) Number of divider FUs", "1"},
			{ "divider.resnumber", "(uint) Number of divider reservation stations", "2"},
			{ "divider.latency", "(uint) Divider FU latency in cycles", "20"},
			{ "multiplier.number", "(uint) Number of multiplier FUs", "2"},
			{ "multiplier.resnumber", "(uint) Number of multiplier reservation stations", "4"},
			{ "multiplier.latency", "(uint) Multiplier FU latency in cycles", "10"},
			{ "ls.number", "(uint) Number of load/store units", "1"},
			{ "ls.resnumber", "(uint) Number of L/S reservation stations (FIFO)", "8"},
			{ "ls.latency", "(uint) L/S address computation latency in cycles", "3"}
		)

		SST_ELI_DOCUMENT_STATISTICS(
			{ "instructions", "Number of instructions executed", "", 0 }
		)

		// This is used to connect the memory interface, thus we don't need to implement one!
		SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( {"memory", "Interface to memory (e.g., caches)", "SST::Interfaces::StandardMem"} )

	private:
		using Super=SST::Component;
		template<typename Type> using Statistics=SST::Statistics::Statistic<Type>;
		using Cycle_t=SST::Cycle_t;
		using StandardMem=SST::Interfaces::StandardMem;
		using Output=SST::Output;
		using ComponentId_t=SST::ComponentId_t;
		using Params=SST::Params;
		using Clock=SST::Clock;
		using TimeConverter = ::SST::TimeConverter;

	public:
		/** These functions are defined by SST **/
		Core(ComponentId_t id, Params& params);
		virtual void init(unsigned int phase) override final;
		virtual void setup() override final;
		virtual void finish() override final;
		bool tick(Cycle_t cycle);

	private:
		/** For completeness **/
		Core();
		Core(const Core& params);
		Core operator=(const Core& params);
		virtual ~Core() = default;

	protected:
		void load_program(Params &params);
		void load_configuration(Params &params);

	private:
		int verbose{0};
		std::string clock_frequency{"0Hz"};
		std::string output_path{"statistics.json"};

		Output *output{nullptr};
		MemoryWrapper *memory_wrapper;
		Statistics<uint64_t> *instruction_count;
		TimeConverter* tc{nullptr};

		// Instruction trace
		std::vector<uint16_t> program;

		// Tomasulo config
		TomasuloConfig config;

		// Reservation stations
		std::vector<ReservationStation> integer_rs;
		std::vector<ReservationStation> divider_rs;
		std::vector<ReservationStation> multiplier_rs;
		std::deque<ReservationStation> ls_rs; // FIFO queue

		// Global RS index offsets for unique tagging
		int int_rs_offset{0};
		int div_rs_offset{0};
		int mul_rs_offset{0};
		int ls_rs_offset{0};
		int total_rs_count{0};

		// Functional units
		std::vector<FunctionalUnit> integer_fus;
		std::vector<FunctionalUnit> divider_fus;
		std::vector<FunctionalUnit> multiplier_fus;
		FunctionalUnit ls_fu;

		// Register file + alias table
		std::array<uint16_t, 8> registers;
		std::array<RegisterStatus, 8> reg_status;

		// Event list
		std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue;

		// Pipeline state
		int next_issue_index{0};
		uint64_t current_cycle{0};
		bool terminate{false};
		bool memory_pending{false};

		// Stats
		uint64_t total_stalls{0};
		uint64_t total_reg_reads{0};

		// Instruction names for debug
		std::map<uint32_t, std::string> names;
};

}
}