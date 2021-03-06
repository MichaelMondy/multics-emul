/*
   Copyright (c) 2007-2014 Michael Mondy

   This software is made available under the terms of the
   ICU License -- ICU 1.8.1 and later.     
   See the LICENSE file at the top-level directory of this distribution and
   at http://example.org/project/LICENSE.
*/

#ifndef _HW6180_H
#define _HW6180_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// === SIMH

#include "sim_defs.h"

/* These are from SIMH, but not listed in sim_defs.h */
extern t_addr (*sim_vm_parse_addr)(DEVICE *, char *, char **);
extern void (*sim_vm_fprint_addr)(FILE *, DEVICE *, t_addr);
extern uint32 sim_brk_types, sim_brk_dflt, sim_brk_summ; /* breakpoint info */

/* Additions to SIMH -- may conflict with future versions of SIMH */
#define REG_USER1 040000000
#define REG_USER2 020000000

// ============================================================================
// === A couple of generic typedefs

typedef unsigned int uint;  // efficient unsigned int, at least 32 bits
typedef unsigned flag_t;    // efficient unsigned flag

// ============================================================================
// === Misc enum typdefs

// The CPU supports 3 addressing modes
typedef enum { ABSOLUTE_mode, APPEND_mode, BAR_mode } addr_modes_t;

// The CPU supports a privileged/non-privileged flag (dependent upon
// addressing mode, ring of operation, and settings of the active
// segment when in the appending (segmented) address mode.)
typedef enum { NORMAL_mode, PRIV_mode } instr_modes_t;

// The control unit of the CPU is always in one of several states.  We
// don't currently use all of the states used in the physical CPU.  
// The FAULT_EXEC cycle did not exist in the physical hardware.
typedef enum {
    ABORT_cycle, FAULT_cycle, EXEC_cycle, FAULT_EXEC_cycle, INTERRUPT_cycle,
    FETCH_cycle,
    DIS_cycle  // A pseudo cycle for handling the DIS instruction
// CA FETCH OPSTORE, DIVIDE_EXEC
} cycles_t;

// 32 fault codes exist.  Fault handling and interrupt handling are
// similar.
enum faults {
    // Note that these numbers are decimal, not octal.
    // Faults not listed are not generated by the emulator.
    shutdown_fault = 0, store_fault = 1, mme1_fault = 2, f1_fault = 3,
    timer_fault = 4, cmd_fault = 5, drl_fault = 6,
    connect_fault = 8,
    illproc_fault = 10,
    op_not_complete_fault = 11,
    startup_fault = 12,
    overflow_fault = 13, div_fault = 14,
    dir_flt0_fault = 16,
    acc_viol_fault = 20,
    mme2_fault = 21, mme3_fault = 22, mme4_fault = 23, fault_tag_2_fault = 24,
    trouble_fault = 31
    //
    // oob_fault=32 // out-of-band, simulator only
};

// Fault conditions as stored in the "FR" Fault Register
// C99 and C++ would allow 64bit enums, but bits past 32 are related to (unimplemented) parity faults.
typedef enum {
    // Values are bit masks
    fr_ill_op = 1,         // illegal opcode
    fr_ill_mod = 1 << 1,   // illegal address modifier
    // fr_ill_slv = 1 << 2,   // illegal BAR mode procedure
    fr_ill_proc = 1 << 3  // illegal procedure other than the above three
    // fr_ill_dig = 1 << 6 // illegal decimal digit
} fault_cond_t;

// Simulator stop codes (as returned by sim_instr to SIMH)
//     Zero and values above 63 reserved by SIMH
enum sim_stops {
    STOP_MEMCLEAR = 1, // executing empty memory; zero reserved by SIMH
    STOP_BUG,          // impossible conditions, coding error, etc
    STOP_WARN,         // something odd or interesting; further exec might possible
    STOP_IBKPT,        // breakpoint, possibly auto-detected by emulator
    STOP_DIS,          // executed a "delay until interrupt set"
    STOP_SIMH          // A simh routine returned non zero
};

// Devices connected to a SCU
enum active_dev { ADEV_NONE, ADEV_CPU, ADEV_IOM };

// Devices connected to an IOM (I/O multiplexer)
enum dev_type { DEVT_NONE, DEVT_TAPE, DEVT_CON, DEVT_DISK };

// Logging levels.  Messages at level "debug" and level "info" may be re-routed
// to a file via the debug log command (messages at all levels may be
// re-routed via the console log command.   Messages at level debug
// may be suppressed by turning off debug.
enum log_level { DEBUG_MSG, INFO_MSG, NOTIFY_MSG, WARN_MSG, ERR_MSG };


// ============================================================================
// === Misc constants and macros

// Clocks
#define CLK_TR_HZ (512*1)  // should be 512 kHz, but we'll use 512 Hz for now
#define TR_CLK 1 /* SIMH allows clock ids 0..7 */

// Memory
#define IOM_MBX_LOW 01200
#define IOM_MBX_LEN 02200
#define DN355_MBX_LOW 03400
#define DN355_MBX_LEN 03000

#define ARRAY_SIZE(a) ( sizeof(a) / sizeof((a)[0]) )

enum { seg_bits = 15};  // number of bits in a segment number
enum { n_segments = 1 << seg_bits};  // why does c89 treat enums as more constant than consts?

// Constants and a function for ordinary masking of low (rightmost) bits.
static const t_uint64 MASK36 = ~(~((t_uint64)0)<<36);  // lower 36 bits all on
static const t_uint64 MASK18 = ~(~((t_uint64)0)<<18);  // lower 18 bits all on
#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) )    // lower (x) bits all ones

// ============================================================================
// === Struct typdefs

/*
    For efficiency, we mostly use full ints instead of bit fields for
    the flags and other fields of most of the typedefs here.  When necessary,
    such as when an instruction saves a control register to memory, the more
    efficient variables are copied into appropriate bit fields.

    Also note that we only represent selected fields of some of the
    registers.  The simulator doesn't need some of the various scratches
    and the Multics OS doesn't need them either.

    Comments about bit positions and field lengths reflect the
    layouts of the physical hardware, not the simulator.

    Descriptions of registers may be found in AN87 and AL39.
*/

/* MF fields of EIS multi-word instructions -- 7 bits */
typedef struct {
    flag_t ar;
    flag_t rl;
    flag_t id;
    uint reg;  // 4 bits
} eis_mf_t;

/* Format of a 36 bit instruction word */
typedef struct {
    uint addr;    // 18 bits at  0..17; 18 bit offset or seg/offset pair
    uint opcode;  // 10 bits at 18..27
    uint inhibit; //  1 bit  at 28
    union {
        struct {
            uint pr_bit;  // 1 bit at 29; use offset[0..2] as pointer reg
            uint tag;     // 6 bits at 30..35 */
        } single;
        eis_mf_t mf1;     // from bits 29..35 of EIS instructions
    } mods;
    flag_t is_eis_multiword;  // set true for relevent opcodes
} instr_t;

/* Indicator register (14 bits [only positions 18..32 have meaning]) */
typedef struct {
    uint zero;              // bit 18
    uint neg;               // bit 19
    uint carry;             // bit 20; see AL39, 3-6
    uint overflow;          // bit 21
    uint exp_overflow;      // bit 22   (used only by ldi/stct1)
    uint exp_underflow;     // bit 23   (used only by ldi/stct1)
    uint overflow_mask;     // bit 24
    uint tally_runout;      // bit 25
    uint parity_error;      // bit 26   (used only by ldi/stct1)
    uint parity_mask;       // bit 27
    uint not_bar_mode;      // bit 28
    uint truncation;        // bit 29
    uint mid_instr_intr_fault;  // bit 30
    uint abs_mode;          // bit 31
    uint hex_mode;          // bit 32
} IR_t;


// "MR" Mode Register, L68
typedef struct {
    // See member "word" for the raw bits, other member values are derivations
    flag_t mr_enable;   // bit 35 "n"
    flag_t strobe;      // bit 30 "l"
    flag_t fault_reset; // bit 31 "m"
    t_uint64 word;
} mode_reg_t;


// Base Address Register (BAR) -- 18 bits
typedef struct {
    uint base;      // 9 bits, upper 9 bits of an 18bit base
    uint bound;     // 9 bits, upper 9 bits of an 18bit bounds
} BAR_reg_t;

// Combination: Pointer Registers and Address Registers
// Note that the eight registers are also known by the
// names: ap, ab, bp, bb, lp, lb, sp, sb
typedef struct {
    int wordno; // offset from segment base; 18 bits
    struct {
        int snr;    // segment #, 15 bits
        uint rnr;   // effective ring number
        uint bitno; // index into wordno
    } PR;   // located in APU in physical hardware
    struct {
        uint charno;    // index into wordno (2 bits)
        uint bitno; // index into charno
    } AR;   // located in Decimal Unit in physical hardware
} AR_PR_t;

// PPR Procedure Pointer Register (pseudo register)
//      Use: Holds info relative to the location in main memory of the
//      procedure segment in execution and the location of the current
//      instruction within that segment
typedef struct {
    uint PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint P;         /* Privileged bit; 1 bit @ 0[18] */
    int IC;         /* Instruction counter, 18 bits */
} PPR_t;

// TPR Temporary Pointer Register (pseudo register)
//      Use: current virt addr used by the processor in performing addr
//      prep for operands, indirect words, and instructions.   At the
//      completion of addr prep, the contents of the TPR is presented
//      to the appending unit associative memories for translation
//      into the 24-bit absolute main memory address.
typedef struct {
    uint TRR;   // Current effective ring number, 3 bits
    uint TSR;   // Current effective segment number, 15 bits
    uint TBR;   // Current bit offset as calculated from ITS and ITP, 6 bits
    uint CA;    // Current computed addr relative to the segment in TPR.TSR, 18 bits
    // FIXME: CA value should probably be placed in ctl_unit_data_t
    uint is_value;  // is offset a value or an address? (du or dl modifiers)
    t_uint64 value; // 36bit value from opcode constant via du/dl
} TPR_t;


// More emulator state variables for the cpu
// These probably belong elsewhere, perhaps control unit data or the
// cu-history regs...
typedef struct {
    cycles_t cycle;
    uint IC_abs;    // translation of odd IC to an absolute address; see ADDRESS of cu history
    flag_t irodd_invalid;   // cached odd instr invalid due to memory write by even instr
    uint read_addr; // last absolute read; might be same as CA for our purposes...; see APU RMA
    // flag_t instr_fetch;  // true during an instruction fetch
    /* The following are all from the control unit history register: */
        flag_t trgo;    // most recent instruction caused a transfer?
        flag_t ic_odd;  // executing odd pair?
        flag_t poa;     // prepare operand address
        uint opcode;    // currently executing opcode
    struct {
        flag_t  fhld;   // An access violation or directed fault is waiting.   AL39 mentions that the APU has this flag, but not where scpr stores it
    } apu_state;
} cpu_state_t;


/* APU history register (72 bits) */
typedef struct {
    int esn;
    enum { esn_ppr = 0, esn_pr = 1, esn_ptr = 2 } bsy;
} apu_hist_t;

/* Fault register (72 bits) */
#if 0
/* NOTE USED, see fault_reg_t above */
typedef struct {
    // Multics never examines this (just the CPU) -- multicians.org glossary
    uint ill_op:1;      /* 1 bit at 0 */
    uint ill_mod:1;     /* 1 bit at 1 */
    uint ill_slv:1;     /* 1 bit at 2 */
    uint ill_proc:1;    /* 1 bit at 3 */
    /* ... */
} fault_reg_t;
#endif

// Emulator-only interrupt and fault info
typedef struct {
    flag_t xed;             // executed xed for a fault handler
    flag_t any;                 // true if any of the below are true
    flag_t int_pending;
    int low_group;          // Lowest group-number fault preset
    uint32 group7;          // bitmask for multiple group 7 faults
    int fault[7];           // only one fault in groups 1..6 can be pending
    flag_t interrupts[32];
} events_t;





/* Control unit data (288 bits) */
typedef struct {
    /*
            NB: Some of the data normally stored here is represented
        elsewhere -- e.g.,the PPR is a variable outside of this
        struct.   Other data is live and only stored here.
    */
    /*      This is a collection of flags and registers from the
        appending unit and the control unit.  The scu and rcu
        instructions store and load these values to an 8 word
        memory block.
            The CU data may only be valid for use with the scu and
        rcu instructions.
            Comments indicate format as stored in 8 words by the scu
        instruction.
    */

    /* NOTE: PPR (procedure pointer register) is a combination of registers:
        From the Appending Unit
            PRR bits [0..2] of word 0
            PSR bits [3..17] of word 0
            P   bit 18 of word 0
        From the Control Unit
            IC  bits [0..17] of word 4
    */

#if 0

    /* First we list some registers we either don't use or that we have represented elsewhere */

    /* word 0 */
    // PPR portions copied from Appending Unit
    uint PPR_PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PPR_PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint PPR_P;         /* Privileged bit; 1 bit @ 0[18] */
    // uint64 word0bits; /* Word 0, bits 18..32 (all for the APU) */
    uint FCT;           // Fault counter; 3 bits at word 0 [33..35]

    /* word 1 */
    //uint64 word1bits; /* Word1, bits [0..19] and [35] */

    uint IA;        /* 4 bits @ 1[20..23] */
    uint IACHN;     /* 3 bits @ 1[24..26] */
    uint CNCHN;     /* 3 bits @ 1[27..29] */
    uint FIADDR     /* 5 bits @ 1[30..34] */

    /* word 2 */
    uint TPR_TRR;   // 3 bits @ 2[0..2];  temporary ring register
    uint TPR_TSR;   // 15 bits @ 2[3..17]; temporary segment register
    // unused: 10 bits at 2[18..27]
    // uint cpu_no; // 2 bits at 2[28..29]; from maint panel switches
    
    /* word 3 */

    /* word 4 */
    // IC belongs to CU
    int IC;         // 18 bits at 4[0..17]; instruction counter aka ilc
    // copy of IR bits 14 bits at 4[18..31]
    // unused: 4 bits at 4[32..36];

    /* word 5 */
    uint CA;        // 18 bits at 5[0..17]; computed address value (offset) used in the last address preparation cycle
    // cu bits for repeats, execute double, restarts, etc
#endif

    /* Above was documentation on all physical control unit data.
     * Below are the members we actually implement here.  Missing
     * members are either not (yet) emulated or are handled outside
     * of this control unit data struct.
     */

    /* word 0, continued */
    flag_t SD_ON;       // SDWAM enabled
    flag_t PT_ON;       // PTWAM enabled

    /* word 1, continued  */
    struct {
        unsigned oosb:1;    // out of segment bounds
        unsigned ocall:1;   // outward call
        // unsigned boc:1;      // bad outward call
        // unsigned ocb:1;      // out of call brackets
    } word1flags;
    flag_t instr_fetch;     // our usage of this may match PI-AP

    /* word 2, continued */
    uint delta;     // 6 bits at 2[30..35]; addr increment for repeats

    /* word 5, continued */
    flag_t rpts;        // just executed a repeat instr;  bit 12 in word one of the CU history register
    flag_t repeat_first;        // "RF" flag -- first cycle of a repeat instruction; We also use with xed
    flag_t rpt;     // execute an rpt instruction
    flag_t rd;      // execute an rpd instruction
    uint CT_HOLD;   // 6 bits at 5[30..35]; contents of the "remember modifier" register
    flag_t xde;     // execute even instr from xed pair
    flag_t xdo;     // execute even instr from xed pair

    /* word 6 */
    instr_t IR;     /* Working instr register; addr & tag are modified */
    //uint tag;       // td portion of instr tag (we only update this for rpt instructions which is the only time we need it)

    /* word 7 */
    // instr_t IRODD;   // Instr holding register; odd word of last pair fetched
    t_uint64 IRODD; /* Instr holding register; odd word of last pair fetched */
    
} ctl_unit_data_t;

// PTW -- 36 bits -- AN87, page 1-17 or AL39
typedef struct {
    uint addr;      // 18 bits; mod 64 abs main memory addr of page aka upper 18 bits; bits 0..17
    // uint did;    // 4 bits; bit 18..21
    // flag_t d;
    // flag_t p;
    flag_t u;       // used; bit 26
    // flag_t o;
    // flag_t y;
    flag_t m;       // modified; bit 29
    // flag_t q;
    // flag_t w;
    // flag_t s;
    flag_t f;       // directed fault (0=>page not in memory, so fault); bit 33
    uint fc;        // which directed fault; bits 34..35
} PTW_t;


// PTWAM (Page Table Word Associative Memory) registers.  These are 51 bits
// as stored by sptr & sptp.  Some bits of PTW are ignored by those
// instructions.
typedef struct {
    PTW_t ptw;
    struct {
        uint ptr;       // 15 bits; effective segment #
        uint pageno;    // 12 bits; 12 high order bits of CA used to fetch this PTW from mem
        flag_t is_full; // PTW is valid
        uint use;       // counter, 4 bits
        uint enabled;   // internal flag, not part of the register
    } assoc;
} PTWAM_t;

// SDW (72 bits)
typedef struct {
    // even word:
    uint addr;      // 24bit main memory addr -- page table or page segment
    uint r1;        // 3 bits
    uint r2;        // 3 bits
    uint r3;        // 3 bits
    flag_t f;       // In memory if one, fault if zero;  In SDW bit 33, stored by ssdp but not ssdr
    uint fc;        // directed fault; Bits 34..35 of even word; in SDW, but not SDWAM?
    // odd word:
    uint bound;     // 14 bits; 14 high order bits of furtherest Y-block16
    flag_t r;       // read perm
    flag_t e;       // exec perm
    flag_t w;       // write perm
    flag_t priv;    // priv
    flag_t u;       // unpaged; bit 19 odd word of SDW
    flag_t g;       // gate control
    flag_t c;       // cache control
    uint cl;        // 14 bits; (inbound) call limiter; aka eb
} SDW_t;


// SDWAM (Segment Descriptor Word Associative Memory) registers; 88 bits each
typedef struct {
    SDW_t sdw;
    struct {
        // flag_t modified;
        uint ptr;           // 15 bits; effective segment #
        flag_t is_full; // flag; this SDW is valid
        uint use;       // counter, 4 bits
        // flag_t enabled;  // internal flag, not part of the register
    } assoc;
} SDWAM_t;

// Descriptor Segment Base Register (51 bits)
typedef struct {
    uint32 addr;    // Addr of DS or addr of page tbl; 24 bits at 0..23
    uint32 bound;   // Upper bits of 16bit addr; 14 bits at 37..50
    flag_t u;       // Is paged?  1 bit at 55
    uint32 stack;   // Used by call6; 12 bits at 60..71
} DSBR_t;

// ============================================================================

// Beginnings of movement of all cpu info into a single struct.   This
// will be needed for supporting multiple CPUs.   At the moment, it mostly
// holds semi-exposed registers used during saved/restored memory debugging.
// On the other hand, it might be better to handle multiple CPUs as
// separate processes anyway.
typedef struct {
    PTWAM_t PTWAM[16];  // Page Table Word Associative Memory, 51 bits
    SDWAM_t SDWAM[16];  // Segment Descriptor Word Associative Memory, 88 bits
    DSBR_t DSBR;            // Descriptor Segment Base Register (51 bits)
} cpu_t;

// Physical Switches & Characteristics
typedef struct {
    // Switches on the Processor's maintenance and configuration panels
    int FLT_BASE;   // normally 7 MSB of 12bit fault base addr
    int cpu_num;    // zero for CPU 'A', one for 'B' etc.
    flag_t dps8_model;  // false if L68; true if DPS8
} switches_t;

// Physical ports on the CPU
typedef struct {
    // The ports[] array should indicate which SCU each of the CPU's 8
    // ports are connected to.
    int ports[8];   // SCU connectivity; designated a..h
    int scu_port;   // What port num are we connected to (same for all SCUs)
} cpu_ports_t;

// System Controller
typedef struct {
    // Note that SCUs had no switches to designate SCU 'A' or 'B', etc.
    // Instead, SCU "A" is the one with base address switches set for 01400,
    // SCU "B" is the SCU with base address switches set to 02000, etc.
    // uint mem_base;   // zero on boot scu
    // mode reg: mostly not stored here; returned by scu_get_mode_register()
    int mode;   // program/manual; if 1, sscr instruction can set some fields
#if 0
    // The info below exists on the physical hardware but is mostly implemented
    // as unchangable values in the emulator.  Instead scu.c returns hard
    // coded values.   One exception is the program/manual switch which
    // is listed above.
    struct {
        unsigned mask_a_assign:9;
        unsigned a_online:1;    // bank a online?
        unsigned a1_online:1;
        unsigned b_online:1;
        unsigned b1_online:1;
        unsigned port_no:4;
        unsigned mode:1;    // program or manual
        unsigned nea_enabled:1;
        unsigned nea:7;
        unsigned interlace:1;
        unsigned lwr:1;     // controls whether A or B is low order memory
        unsigned port_mask_0_3:4;
        unsigned cyclic_prio:7;
        unsigned port_mask_4_7:4;
    } config_switches;
#endif

    // CPU/IOM connectivity; designated 0..7
    struct {
        flag_t is_enabled;
        enum active_dev type;   // type of connected device
        int idnum;              // id # of connected dev, 0..7
        int dev_port;           // which port on the connected device?
    } ports[8];
    

    /* The interrupt registers.
        Four exist; only two "A" and "B" used.
        Two parts -- execute interrupt mask register and a 9-bit mask assignment
        Currently missing: interrupt present
    */
    struct {
        flag_t avail;       // Not physical.  Does mask really exist?
        // Part 1 -- the execute interrupt mask register
        unsigned exec_intr_mask;    // 32 bits, one for each intr or "cell"
        // Part 2 -- the interrupt mask assignment register -- 9 bits total
        struct {
            unsigned raw;       // 9 bits; raw mask; decoded below
            flag_t unassigned;  // is it assigned to a port?
            // We only list one port -- Multics only allowed one port at a time
            // even though the SCU hardware would have allowed all 8 ports
            uint port;          // port to which mask is assigned (0..7)
        } mask_assign;  // eima_data[4];
    } interrupts[4];
} scu_t;

// I/O Multiplexer
enum { max_channels = 64 };     // enums are more constant than consts...
typedef struct {
    uint8 iom_num;
    uint16 base;    // IOM BASE switches - 12 bits
    int ports[8];   // CPU/IOM connectivity; designated a..h; negative to disable
    int scu_port;   // which port on the SCU(s) are we connected to?
    struct {
        enum dev_type type;
        DEVICE* dev;    // attached device; points into sim_devices[]
        // The channel "boards" do *not* point into the UNIT array of the
        // IOM entry within sim_devices[].  These channel "boards" are used
        // only for simulation of async operation (that is as arguments for
        // sim_activate()).  Since they carry no state information, they
        // are dynamically allocated by the IOM as needed.
        UNIT* board;    // represents the channel; See comment just above
    } channels[max_channels];
} iom_t;

// Used to communicate between the IOM and devices
typedef struct {
    int chan;
    void* statep;   // For use by device specific code
    int dev_cmd;    // 6 bits
    int dev_code;   // 6 bits
    int chan_data;  // 6 bits; often some sort of count
    flag_t have_status; // set to true by the device when operation is complete
    int major;
    int substatus;
    flag_t is_read;
    int time;       // request by device for queuing via sim_activate()
} chan_devinfo;

// System-wide info and options not tied to a specific CPU, IOM, or SCU
typedef struct {
    int clock_speed;
        // Instructions rccl and rscr allow access to a hardware clock.
        // If zero, the hardware clock returns the real time of day.
        // If non-zero, the clock starts at an arbitrary date and ticks at
        // a rate approximately equal to the given number of instructions
        // per second.
    // Delay times are in cycles; negative for immediate
    struct {
        int connect;    // Delay between CIOC instr & connect channel operation
        int chan_activate;  // Time for a list service to send a DCW
    } iom_times;
    struct {
        int read;
        int xfer;
    } mt_times;
    flag_t warn_uninit; // Warn when reading uninitialized memory
    flag_t startup_interrupt;
        // The CPU is supposed to start with a startup fault.  This will cause
        // a series of trouble faults until the IOM finally writes a DIS from
        // the tape label onto the troube fault vector location.  In order to
        // reduce debugging clutter, the emulator allows starting the CPU off
        // with an interrupt that we know has a DIS instruction trap.  This
        // interrupt is hinted at in AN70.  This will cause the CPU to start off
        // waiting for the next interrupt (from the IOM after it loads the first
        // tape record and sends a terminate interrupt).
    int tape_chan;  // Which channel of the IOM is the tape drive attached to?
    int opcon_chan;  // Which channel of the IOM has the operator's console?
} sysinfo_t;

// Statistics
typedef struct {
    struct {
        uint nexec;
        uint nmsec; // FIXME: WARNING: if 32 bits, only good for ~47 days :-)
    } instr[1024];
    t_uint64 total_cycles;      // Used for statistics and for simulated clock
    t_uint64 total_instr;
    t_uint64 total_msec;
    uint n_instr;       // Reset to zero on each call to sim_instr()
} stats_t;

// ============================================================================
// === Variables

// Memory.
// Most access is via fetch_abs_word() and store_abs_word(), but a
// few source files make direct access (debugging and the IOM).
#define MAXMEMSIZE (16*1024*1024)
extern t_uint64 *Mem;

// Non CPU
extern int opt_debug;
extern sysinfo_t sys_opts;
extern stats_t sys_stats;
extern flag_t fault_gen_no_fault;   // Allows cmd-line to use APU w/o faulting

// Parts of the CPU
extern t_uint64 reg_A;      // Accumulator, 36 bits
extern t_uint64 reg_Q;      // Quotient, 36 bits
extern int8 reg_E;          // Floating Point exponent, 8 bits
extern uint32 reg_X[8];     // Index Registers, 18 bits
extern IR_t IR;             // Indicator Register
extern BAR_reg_t BAR;       // Base Address Register (BAR); 18 bits
extern uint32 reg_TR;       // Timer Reg, 27 bits -- only valid after calls to SIMH clock routines
extern AR_PR_t AR_PR[8];    // Combined Pointer Registers and Address Registers
extern PPR_t PPR;           // Procedure Pointer Reg, 37 bits, internal only
extern TPR_t TPR;           // Temporary Pointer Reg, 42 bits, internal only
extern mode_reg_t MR;       // "MR" Mode Register
extern t_uint64 FR;         // "FR" Fault Register
extern t_uint64 CMR;        // Cache Mode Register (ignored)
extern uint8 reg_RALR;      // Ring Alarm Reg, 3 bits
extern cpu_t *cpup;     // Almost Everything you ever wanted to know about a CPU
extern ctl_unit_data_t cu;
extern cpu_state_t cpu;
extern t_uint64 calendar_a;
extern t_uint64 calendar_q;

// ============================================================================
// === Functions

/* misc.c */
extern int log_any_io(int val);
extern int log_ignore_ic_change(void);
extern int log_notice_ic_change(void);
extern void log_forget_ic(void);
extern void log_msg(enum log_level, const char* who, const char* format, ...);
extern void out_msg(const char* format, ...);
extern t_stat cmd_seginfo(int32 arg, char *buf);    // display segment info
extern int apu_show_seg(FILE *st, UNIT *uptr, int val, void *desc); // display segment info
extern int words2its(t_uint64 word1, t_uint64 word2, AR_PR_t *prp);
extern void word2pr(t_uint64 word, AR_PR_t *prp);
extern int cmd_find(int32 arg, char *buf);
extern int cmd_symtab_parse(int32 arg, char *buf);
extern t_stat fprint_sym (FILE *ofile, t_addr simh_addr, t_value *val, UNIT *uptr, int32 sw);
extern void fprint_addr(FILE *stream, DEVICE *dptr, t_addr simh_addr);
extern void out_sym(int is_write, t_addr simh_addr, t_value *val, UNIT *uptr, int32 sw);
extern void flush_logs(void);
extern int get_seg_name(uint segno);
extern int scan_seg(uint segno, int msgs);  // scan definitions section for procedure entry points

/* debug_run.cpp */
extern void check_seg_debug(void);
extern void state_invalidate_cache(void);
extern void state_save(void);
extern void state_dump_changes(void);
extern void ic2text(char *icbuf, addr_modes_t addr_mode, uint seg, uint ic);
extern void ic_history_init(void);
extern void ic_history_add(void);
extern void ic_history_add_fault(int fault);
extern void ic_history_add_intr(int intr);
extern int cmd_dump_history(int32 arg, char *buf);
extern int cpu_set_history(UNIT *uptr, int32 val, char* cptr, void *desc);
extern int cpu_show_history(FILE *st, UNIT *uptr, int val, void *desc);
extern int show_location(int show_source_lines);
extern int cmd_xdebug(int32 arg, char *buf);
extern char *ir2text(const IR_t *irp);
extern int cmd_stack_trace(int32 arg, char *buf);
extern int cpu_show_stack(FILE *st, UNIT *uptr, int val, void *desc);
extern void show_variables(unsigned segno, int ic);
extern int seginfo_show_all(int seg, int first);
extern int cmd_stats(int32 arg, char *buf);
extern void trace_init();

/* hw6180_cpu.c */
extern void cancel_run(enum sim_stops reason);
extern void restore_from_simh(void);    // SIMH has a different form of some internal variables
extern int cmd_load_listing(int32 arg, char *buf);
extern void load_IR(IR_t *irp, t_uint64 word);
extern void save_IR(t_uint64* wordp);
extern void load_PPR(t_uint64 word, PPR_t *pprp);
extern void load_TPR(t_uint64 word, TPR_t *pprp);
extern t_uint64 save_PPR(const PPR_t *pprp);
extern void fault_gen(enum faults);
extern int fault_check_group(int group);    // Do faults exist a given or higher priority?
extern int fetch_word(uint addr, t_uint64 *wordp);
extern int fetch_abs_word(uint addr, t_uint64 *wordp);
extern int store_word(uint addr, t_uint64 word);
extern int store_abs_word(uint addr, t_uint64 word);
extern int store_abs_pair(uint addr, t_uint64 word0, t_uint64 word1);
extern int store_pair(uint addr, t_uint64 word0, t_uint64 word1);
extern int fetch_abs_pair(uint addr, t_uint64* word0p, t_uint64* word1p);
extern int fetch_pair(uint addr, t_uint64* word0p, t_uint64* word1p);
extern int fetch_yblock(uint addr, int aligned, uint n, t_uint64 *wordsp);
extern int fetch_yblock8(uint addr, t_uint64 *wordsp);
extern int store_yblock8(uint addr, const t_uint64 *wordsp);
extern int store_yblock16(uint addr, const t_uint64 *wordsp);
extern void word2instr(t_uint64 word, instr_t *ip);
extern void decode_instr(t_uint64 word);
extern void encode_instr(const instr_t *ip, t_uint64 *wordp);
extern char *bin2text(t_uint64 word, int n);

/* opu.c */
extern void execute_instr(void);
extern void cu_safe_store(void);
extern int add72(t_uint64 ahi, t_uint64 alow, t_uint64* dest1, t_uint64* dest2, int is_unsigned);

/* eis_opu.cpp */
extern int op_move_alphanum(const instr_t* ip, int fwd);
extern int op_tct(const instr_t* ip, int fwd);
extern int op_mvt(const instr_t* ip);
extern int op_cmpc(const instr_t* ip);
extern int op_cmpb(const instr_t* ip);
extern int op_csl(const instr_t* ip);
extern int op_btd(const instr_t* ip);
extern int op_dtb(const instr_t* ip);
extern int op_scm(const instr_t* ip, int fwd);
extern int op_mvne(const instr_t* ip);
extern int op_mvn(const instr_t* ip);
extern int op_dv3d(const instr_t* ip);

/* scu.c */
extern int scu_cioc(t_uint64 addr);
extern int scu_get_mask(t_uint64 addr, int port);
extern int scu_set_mask(t_uint64 addr, int port);
extern int scu_get_cpu_mask(t_uint64 addr);
extern int scu_set_cpu_mask(t_uint64 addr);
extern int scu_get_mode_register(t_uint64 addr);
extern int scu_get_config_switches(t_uint64 addr);
extern int scu_set_config_switches(t_uint64 addr);
extern void scu_clock_service(void);
extern int scu_get_calendar(t_uint64 addr);
extern int scu_set_interrupt(int inum);

/* apu.c */
extern void set_addr_mode(addr_modes_t mode);
extern addr_modes_t get_addr_mode(void);
extern int is_priv_mode(void);
extern void mod2text(char *buf, uint tm, uint td);
extern char* instr2text(const instr_t* ip);
extern char* print_instr(t_uint64 word);
extern int get_address(uint y, uint xbits, flag_t ar, uint reg, int nbits, uint *addrp, uint* bitnop, uint *minaddrp, uint* maxaddrp);
int decode_eis_address(uint y, flag_t ar, uint reg, int nbits, uint *ringp, uint*segp, uint *offsetp, uint *bitnop);
int get_ptr_address(uint ringno, uint segno, uint offset, uint *addrp, uint *minaddrp, uint* maxaddrp);
extern int addr_mod(void);
extern void reg_mod(uint td, int off);          // FIXME: might be performance boost if inlined
extern int fetch_appended(uint addr, t_uint64 *wordp);
extern int store_appended(uint offset, t_uint64 word);
extern int cmd_dump_vm(int32 arg, char *buf);
extern int apu_show_vm(FILE *st, UNIT *uptr, int val, void *desc);
extern SDW_t* get_sdw();
extern int addr_any_to_abs(uint *addrp, addr_modes_t mode, int segno, int offset);
extern int convert_address(uint* addrp, int seg, int offset, int fault);
extern int get_seg_addr(uint offset, uint perm_mode, uint *addrp);
extern char* print_ptw(t_uint64 word);
extern char* print_sdw(t_uint64 word0, t_uint64 word1);
extern char* sdw2text(const SDW_t *sdwp);

/* hw6180_sys.c */
/* The emulator gives SIMH a "packed" address form that encodes mode, segment,
 * and offset */
extern t_uint64 addr_emul_to_simh(addr_modes_t mode, unsigned segno, unsigned offset);
extern int addr_simh_to_emul(t_uint64 addr, addr_modes_t *modep, unsigned *segnop, unsigned *offsetp);
extern int activate_timer();

// extern int decode_addr(instr_t* ip, t_uint64* addrp);
// extern int decode_ypair_addr(instr_t* ip, t_uint64* addrp);

/* iom.c */
extern void iom_init(void);
extern void iom_interrupt(void);
extern t_stat channel_svc(UNIT *up);
extern int iom_show_mbx(FILE *st, UNIT *uptr, int val, void *desc);
extern char* print_dcw(t_addr addr);

/* math.c */
extern void mpy(t_uint64 a, t_uint64 b, t_uint64* hip, t_uint64 *lowp);
extern void div72(t_uint64 hi, t_uint64 low, t_uint64 divisor, t_uint64* quotp, t_uint64* remp);
extern void mpy72fract(t_uint64 ahi, t_uint64 alow, t_uint64 b, t_uint64* hip, t_uint64 *lowp);

/* math_real.c */
double multics_to_double(t_uint64 xhi, t_uint64 xlo, int show, int is_signed);
extern int instr_dvf(t_uint64 word);
extern int instr_ufas(t_uint64 word, flag_t subtract);  // ufa and ufs
extern int instr_ufm(t_uint64 word);
extern int instr_fno(void);

/* eis_desc.cpp */
// EIS misc
extern eis_mf_t* parse_mf(uint mf, eis_mf_t* mfp);
extern const char* mf2text(const eis_mf_t* mfp);
extern int fetch_mf_ops(const eis_mf_t* mf1p, t_uint64* word1p, const eis_mf_t* mf2p, t_uint64* word2p, const eis_mf_t* mf3p, t_uint64* word3p);
extern int get_eis_indir_addr(t_uint64 word, uint* addrp);
extern int addr_mod_eis_addr_reg(instr_t *ip);

/* mt.c */
extern void mt_init(void);
extern int mt_iom_cmd(chan_devinfo* devinfop);
extern int mt_iom_io(chan_devinfo* devinfop, t_uint64 *wordp);

/* disk.c */
extern void disk_init(void);
extern int disk_iom_cmd(chan_devinfo* devinfop);
extern int disk_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);

/* console.c */
extern void console_init(void);
extern int opcon_autoinput_set(UNIT *uptr, int32 val, char *cptr, void *desc);
extern int opcon_autoinput_show(FILE *st, UNIT *uptr, int val, void *desc);
extern int con_iom_cmd(int chan, int dev_cmd, int dev_code, int* majorp, int* subp);
extern int con_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);

/* debug_io.c */
// extern void setup_streams(void);

// ============================================================================

#ifdef __cplusplus
}   // extern "C"
#endif

// ============================================================================

#ifndef __cplusplus
// C++ only features:
// extern ostream cdebug;
#endif

#include "opcodes.h"
#include "bit36.h"

#include "options.h"

#endif  // _HW6180_H
