#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <string>

namespace XSim
{
namespace Core
{

enum class FUType : uint8_t
{
    INTEGER,
    DIVIDER,
    MULTIPLIER,
    LOAD_STORE
};

enum class Stage : uint8_t
{
    ISSUE,
    READ_OPERAND,
    EXECUTE,
    WRITE_RESULT,
    MEMORY_PENDING
};

// Tag convention: -1 = ready/no producer, >= 0 = RS index producing value
struct ReservationStation
{
    bool busy{false};
    int instr_index{-1};
    uint16_t opcode{0};
    FUType fu_type{FUType::INTEGER};

    uint16_t Vj{0};
    uint16_t Vk{0};
    int Qj{-1};
    int Qk{-1};

    int dest_reg{-1};
    int assigned_fu{-1};

    // Created issue order to track when the instruction was issued
    int issue_order{-1};

    // L/S specific
    uint16_t address{0};
    uint16_t store_data{0};
    int Qstore{-1};
    bool store_data_ready{false};



    void clear()
    {
        busy = false;
        instr_index = -1;
        opcode = 0;
        fu_type = FUType::INTEGER;
        Vj = 0; Vk = 0;
        Qj = -1; Qk = -1;
        dest_reg = -1;
        assigned_fu = -1;
        address = 0;
        store_data = 0;
        Qstore = -1;
        store_data_ready = false;
        // reset the issue order 
        issue_order = -1;
    }
};

struct FunctionalUnit
{
    bool busy{false};
    FUType type{FUType::INTEGER};
    int rs_index{-1};
    uint64_t finish_cycle{0};
    uint64_t instructions_executed{0};

    void clear()
    {
        busy = false;
        rs_index = -1;
        finish_cycle = 0;
    }
};

struct RegisterStatus
{
    int tag{-1}; // RS index that will write this reg, -1 = current
};

struct Event
{
    uint64_t timestamp;
    Stage stage;
    int rs_index;

    // Process later pipeline stages first on same cycle
    bool operator>(const Event& other) const
    {
        if (timestamp != other.timestamp)
            return timestamp > other.timestamp;
        return static_cast<uint8_t>(stage) < static_cast<uint8_t>(other.stage);
    }
};

struct TomasuloConfig
{
    int integer_num{2};
    int integer_rs{4};
    int integer_latency{1};

    int divider_num{1};
    int divider_rs{2};
    int divider_latency{20};

    int multiplier_num{2};
    int multiplier_rs{4};
    int multiplier_latency{10};

    int ls_num{1};
    int ls_rs{8};
    int ls_latency{3};

    int cache_associativity{1};
    std::string cache_size{"4096B"};
    std::string clock{"1GHz"};
};

inline FUType opcode_to_fu_type(uint16_t opcode)
{
    switch (opcode)
    {
        case 8:  // LW
        case 9:  // SW
            return FUType::LOAD_STORE;
        case 4:  // DIV
        case 7:  // EXP
        case 6:  // MOD
            return FUType::DIVIDER;
        case 5:  // MUL
            return FUType::MULTIPLIER;
        default:
            return FUType::INTEGER;
    }
}

// Returns dest reg index, or -1 if none
inline int get_dest_reg(uint16_t instruction, uint16_t opcode)
{
    switch (opcode)
    {
        case 0:  // ADD
        case 1:  // SUB
        case 2:  // AND
        case 3:  // NOR
        case 4:  // DIV
        case 5:  // MUL
        case 6:  // MOD
        case 7:  // EXP
        case 8:  // LW
        case 16: // LIZ
        case 17: // LIS
        case 18: // LUI
        case 19: // JALR
            return (instruction >> 8) & 0x07;
        default:
            return -1;
    }
}

struct SourceRegs
{
    int rs{-1};
    int rt{-1};
};

inline SourceRegs get_source_regs(uint16_t instruction, uint16_t opcode)
{
    SourceRegs src;
    switch (opcode)
    {
        case 0:  // ADD
        case 1:  // SUB
        case 2:  // AND
        case 3:  // NOR
        case 4:  // DIV
        case 5:  // MUL
        case 6:  // MOD
        case 7:  // EXP
            src.rs = (instruction >> 5) & 0x07;
            src.rt = (instruction >> 2) & 0x07;
            break;
        case 8:  // LW
            src.rs = (instruction >> 5) & 0x07;
            break;
        case 9:  // SW: rs = address, rt = data
            src.rs = (instruction >> 5) & 0x07;
            src.rt = (instruction >> 2) & 0x07;
            break;
        case 14: // PUT
        case 12: // JR
        case 19: // JALR
            src.rs = (instruction >> 5) & 0x07;
            break;
        case 20: // BP
        case 21: // BN
        case 22: // BX
        case 23: // BZ
            src.rs = (instruction >> 8) & 0x07;
            break;
        default:
            break;
    }
    return src;
}

}
}