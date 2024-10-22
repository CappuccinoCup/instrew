
#ifndef _INSTREW_STATE_H
#define _INSTREW_STATE_H

#include <common.h>
#include <rtld.h>
#include <translator.h>

#include <asm/siginfo.h>
#include <asm/signal.h>


struct State {
    Rtld rtld;
    Translator translator;

    uint64_t rew_time;

    struct sigaction sigact[_NSIG];

    struct TranslatorServerConfig tsc;
    struct TranslatorConfig tc;
};

#define QUICK_TLB_BITS 10

struct CpuState {
    struct CpuState* self;
    struct State* state;
    uintptr_t _unused[6];

    _Alignas(64) uint8_t regdata[0x400];

    _Alignas(64) uint64_t quick_tlb[1 << QUICK_TLB_BITS][2];

    _Atomic volatile int sigpending;
    sigset_t sigmask;
    stack_t sigaltstack;
    struct siginfo siginfo;

#ifdef CC_PROFILE_CODECACHE
    uint64_t code_cache_fast;   // Number of execution of the code cache fast path
    uint64_t code_cache_slow;   // Number of execution of the code cache slow path
    uint64_t code_cache_tran;   // Number of times client asks server do translate
#endif
};

#define CPU_STATE_REGDATA_OFFSET 0x40
_Static_assert(offsetof(struct CpuState, regdata) == CPU_STATE_REGDATA_OFFSET,
               "CPU_STATE_REGDATA_OFFSET mismatch");

#define CPU_STATE_QTLB_OFFSET 0x440
_Static_assert(offsetof(struct CpuState, quick_tlb) == CPU_STATE_QTLB_OFFSET,
               "CPU_STATE_QTLB_OFFSET mismatch");

#define CPU_STATE_FROM_REGS(regdata) ((struct CpuState*) \
                                   ((char*) regdata - CPU_STATE_REGDATA_OFFSET))

#ifdef CC_PROFILE_CODECACHE
#define CPU_STATE_CC_FAST_OFFSET 0x44e8
_Static_assert(offsetof(struct CpuState, code_cache_fast) == CPU_STATE_CC_FAST_OFFSET,
               "CPU_STATE_CC_FAST_OFFSET mismatch");
#endif

#endif
