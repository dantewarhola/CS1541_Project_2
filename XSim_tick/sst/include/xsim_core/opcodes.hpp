#pragma once

#define LW		8U
#define SW		9U
#define HALT	13U
#define PUT		14U
#define LIZ		16U


//$ The following are the added opcodes from the instructions


//$ R type instrutions:
#define ADD		0U
#define SUB		1U
#define AND		2U
#define NOR     3U
#define DIV     4U
#define MUL     5U
#define MOD     6U
#define EXP     7U
#define JALR    19U
#define JR      12U


//$ I type instructions:
#define LIS     17U
#define LUI     18U
#define BP      20U
#define BN      21U
#define BX      22U
#define BZ      23U


//$ IX type instructions:
#define J       24U


