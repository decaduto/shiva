#ifndef __SHIVA_H_
#define __SHIVA_H_

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/queue.h>
#include <elf.h>
#include <errno.h>
#include <sys/prctl.h>

#include <udis86.h>
#include "/opt/elfmaster/include/libelfmaster.h"
#include "shiva_debug.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define ELF_MIN_ALIGN 4096
#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v, _a) (((_v) + _a - 1) & ~(_a - 1))

#define SHIVA_RUNTIME_ADDR(addr) (addr + ctx->ulexec.base)

#define SHIVA_F_JMP_CFLOW		(1UL << 0)
#define SHIVA_F_STRING_ARGS		(1UL << 1)
#define SHIVA_F_RETURN_FLOW		(1UL << 2)

#define SHIVA_F_ULEXEC_LDSO_NEEDED	(1UL << 0)

#define SHIVA_STACK_SIZE	(PAGE_SIZE * 1000)

#define SHIVA_LDSO_BASE		0x600000
#define SHIVA_TARGET_BASE	0x1000000

#define SHIVA_MODULE_F_RUNTIME	(1UL << 0)
#define SHIVA_MODULE_F_INIT	(1UL << 1)

#define SHIVA_ULEXEC_LDSO_TRANSFER(stack, addr, entry) __asm__ __volatile__("mov %0, %%rsp\n" \
                                            "push %1\n" \
                                            "mov %2, %%rax\n" \
                                            "mov $0, %%rbx\n" \
                                            "mov $0, %%rcx\n" \
                                            "mov $0, %%rdx\n" \
                                            "mov $0, %%rsi\n" \
                                            "mov $0, %%rdi\n" \
					    "mov $0, %%rbp\n" \
                                            "ret" :: "r" (stack), "g" (addr), "g"(entry))


typedef enum shiva_iterator_res {
	SHIVA_ITER_OK = 0,
	SHIVA_ITER_DONE,
	SHIVA_ITER_ERROR
} shiva_iterator_res_t;

typedef struct shiva_maps_iterator {
	struct shiva_ctx *ctx;
	struct shiva_mmap_entry *current;
} shiva_maps_iterator_t;

typedef struct shiva_callsite_iterator {
	struct shiva_branch_site *current;
	struct shiva_ctx *ctx;
} shiva_callsite_iterator_t;

typedef struct shiva_auxv_iterator {
	unsigned int index;
	struct shiva_ctx *ctx;
	Elf64_auxv_t *auxv;
} shiva_auxv_iterator_t;

typedef struct shiva_auxv_entry {
	uint64_t value;
	int type;
	char *string;
} shiva_auxv_entry_t;

#define SHIVA_TRACE_MAX_ERROR_STRLEN 4096

typedef struct shiva_error {
	char string[SHIVA_TRACE_MAX_ERROR_STRLEN];
	int _errno;
} shiva_error_t;

typedef enum shiva_branch_type {
	SHIVA_BRANCH_JMP = 0,
	SHIVA_BRANCH_CALL,
	SHIVA_BRANCH_RET
} shiva_branch_type_t;

struct shiva_branch_site {
	struct elf_symbol symbol; // symbol being called
	shiva_branch_type_t branch_type;
	uint64_t target_vaddr;
	uint64_t branch_site;
	TAILQ_ENTRY(shiva_branch_site) _linkage;
};

typedef enum shiva_module_section_map_attr {
        LP_SECTION_TEXTSEGMENT = 0,
        LP_SECTION_DATASEGMENT,
        LP_SECTION_UNKNOWN
} shiva_module_section_map_attr_t;

struct shiva_module_section_mapping {
        struct elf_section section;
        shiva_module_section_map_attr_t map_attribute;
        uint64_t vaddr; /* Which memory address the section contents is placed in */
        uint64_t offset;
        uint64_t size;
        char *name;
        TAILQ_ENTRY(shiva_module_section_mapping) _linkage;
};

#define SHIVA_MODULE_MAX_PLT_COUNT 4096

struct shiva_module_plt_entry {
        char *symname;
        uint64_t vaddr;
        size_t offset;
        TAILQ_ENTRY(shiva_module_plt_entry) _linkage;
};

typedef struct shiva_mmap_entry {
	uint64_t base;
	size_t len;
	uint32_t prot;    // mapping prot
	uint32_t mapping; // shared, private
	bool debugger_mapping;
	TAILQ_ENTRY(shiva_mmap_entry) _linkage;
} shiva_mmap_entry_t;

struct shiva_module {
        int fd;
	uint64_t flags;
        uint8_t *text_mem;
        uint8_t *data_mem; /* Includes .bss */
        uintptr_t *pltgot;
        uintptr_t *plt;
        size_t pltgot_size;
        size_t plt_size;
        size_t plt_off;
        size_t plt_count;
        size_t pltgot_off;
        size_t text_size;
        size_t data_size;
        uint64_t text_vaddr;
        uint64_t data_vaddr;
        elfobj_t elfobj; /* elfobj to the module */
	elfobj_t self; /* elfobj to self (Debugger binary) */
        struct {
                TAILQ_HEAD(, shiva_module_section_mapping) section_maplist;
                TAILQ_HEAD(, shiva_module_plt_entry) plt_list;
        } tailq;
};

typedef struct shiva_trace_regset_x86_64 {
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi;
        uint64_t rbp, rsp, rip;
        uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t flags, cs, ss, fs, ds;
} shiva_trace_regset_x86_64_t;

typedef struct shiva_ctx {
	char *path;
	int argc;
	char **args;
	char **argv;
	char **envp;
	int argcount;
	elfobj_t elfobj;
	elfobj_t ldsobj;
	uint64_t flags;
	int pid;
	union {
		struct shiva_trace_regset_x86_64 regset_x86_64;
	} regs;
	struct {
		struct shiva_module *runtime;
		struct shiva_module *initcode;
	} module;
	struct {
		ud_t ud_obj;
		uint8_t *textptr;
		uint64_t base;
	} disas;
	struct {
		/*
		 * basic runtime data created during
		 * userland exec.
		 */
		uint8_t *stack;
		uint8_t *mem;
		uint64_t rsp_start;
		uint64_t entry_point;
		uint64_t base_vaddr;
		uint64_t phdr_vaddr; // vaddr of phdr table for mapped binary
		size_t arglen;
		size_t envpcount;
		size_t envplen;
		char *envstr;
		char *argstr;
		struct {
			size_t sz;
			size_t count;
			uint8_t *vector;
		} auxv;
		/*
		 * mapped LDSO specific data
		 */
		struct {
			uint64_t entry_point;
			uint64_t base_vaddr;
			uint64_t phdr_vaddr;
		} ldso;
		uint64_t flags; // SHIVA_F_ULEXEC_* flags
	} ulexec;
	struct {
		TAILQ_HEAD(, shiva_trace_thread) thread_tqlist;
		TAILQ_HEAD(, shiva_mmap_entry) mmap_tqlist;
		TAILQ_HEAD(, shiva_branch_site) branch_tqlist;
		TAILQ_HEAD(, shiva_trace_handler) trace_handlers_tqlist;
	} tailq;
} shiva_ctx_t;

extern struct shiva_ctx *ctx_global;

/*
 * util.c
 */

char * shiva_strdup(const char *);
char * shiva_xfmtstrdup(char *, ...);
void * shiva_malloc(size_t);

/*
 * signal.c
 */

void shiva_sighandle(int);

/*
 * shiva_iter.c
 */
bool shiva_auxv_iterator_init(struct shiva_ctx *, struct shiva_auxv_iterator *, void *);
shiva_iterator_res_t shiva_auxv_iterator_next(struct shiva_auxv_iterator *, struct shiva_auxv_entry *);
bool shiva_auxv_set_value(struct shiva_auxv_iterator *, long);

/*
 * shiva_ulexec.c
 */
bool shiva_ulexec_prep(shiva_ctx_t *);

/*
 * shiva_module.c
 */
bool shiva_module_loader(shiva_ctx_t *, const char *, struct shiva_module **, uint64_t);

/*
 * shiva_error.c
 */
bool shiva_error_set(shiva_error_t *, const char *, ...);
const char * shiva_error_msg(shiva_error_t *);

/*
 * shiva_maps.c
 */
bool shiva_maps_prot_by_addr(struct shiva_ctx *, uint64_t, int *);
bool shiva_maps_build_list(shiva_ctx_t *);
bool shiva_maps_validate_addr(shiva_ctx_t *, uint64_t);
void shiva_maps_iterator_init(shiva_ctx_t *, shiva_maps_iterator_t *);
shiva_iterator_res_t shiva_maps_iterator_next(shiva_maps_iterator_t *, struct shiva_mmap_entry *);

/*
 * shiva_callsite.c
 */
void shiva_callsite_iterator_init(struct shiva_ctx *, struct shiva_callsite_iterator *);
shiva_iterator_res_t shiva_callsite_iterator_next(shiva_callsite_iterator_t *, struct shiva_branch_site *);

/*
 * shiva_analyze.c
 */
bool shiva_analyze_find_calls(shiva_ctx_t *);
bool shiva_analyze_run(shiva_ctx_t *);

/*
 * Shiva tracing functionality.
 * shiva_trace.c
 * shiva_trace_thread.c
 */

#define SHIVA_TRACE_THREAD_F_TRACED	(1UL << 0)	// thread is traced by SHIVA
#define SHIVA_TRACE_THREAD_F_PAUSED	(1UL << 1)	// pause thread
#define SHIVA_TRACE_THREAD_F_EXTERN_TRACER	(1UL << 2) // thread is traced by ptrace
#define SHIVA_TRACE_THREAD_F_COREDUMPING	(1UL << 3)
#define SHIVA_TRACE_THREAD_F_NEW		(1UL << 4) // newly added into thread list

#define SHIVA_TRACE_HANDLER_F_CALL	(1UL << 0) // handler is invoked via  call
#define SHIVA_TRACE_HANDLER_F_JMP	(1UL << 1) // handler is invoked via jmp
#define SHIVA_TRACE_HANDLER_F_INT3	(1UL << 2) // handler is invoked via int3

typedef enum shiva_trace_op {
	SHIVA_TRACE_OP_CONT = 0,
	SHIVA_TRACE_OP_ATTACH,
	SHIVA_TRACE_OP_POKE,
	SHIVA_TRACE_OP_PEEK,
	SHIVA_TRACE_OP_GETREGS,
	SHIVA_TRACE_OP_SETREGS,
	SHIVA_TRACE_OP_SETFPREGS,
	SHIVA_TRACE_OP_GETSIGINFO,
	SHIVA_TRACE_OP_SETSIGINFO
} shiva_trace_op_t;

#define SHIVA_MAX_INST_LEN 15

typedef struct shiva_trace_insn
{
	uint8_t o_insn[SHIVA_MAX_INST_LEN];
	uint8_t n_insn[SHIVA_MAX_INST_LEN];
	size_t o_insn_len;
	size_t n_insn_len;
} shiva_trace_insn_t;

typedef enum shiva_trace_bp_type {
	SHIVA_TRACE_BP_JMP = 0,
	SHIVA_TRACE_BP_CALL,
	SHIVA_TRACE_BP_INT3
} shiva_trace_bp_type_t;

typedef struct shiva_trace_bp {
	shiva_trace_bp_type_t bp_type;
	uint64_t bp_addr;
	size_t bp_len;
	uint8_t *inst_ptr;
	uint64_t retaddr;
	uint64_t o_target; // if this is a call or jmp breakpoint, o_target holds original target address
	int64_t o_call_offset; // if this is a call or jmp breakpoint, o_offset holds the original target offset
	struct elf_symbol symbol;
	char *call_target_symname;
	bool symbol_location;
	struct shiva_trace_insn insn;
	TAILQ_ENTRY(shiva_trace_bp) _linkage;
} shiva_trace_bp_t;

typedef struct shiva_trace_handler {
	shiva_trace_bp_type_t type;
	void * (*handler_fn)(shiva_ctx_t *); // points to handler triggered by BP
	TAILQ_HEAD(, shiva_trace_bp) bp_tqlist; // list of current bp's
	TAILQ_ENTRY(shiva_trace_handler) _linkage;
} shiva_trace_handler_t;

#if 0
typedef struct shiva_trace_regset_x86_64 {
	uint64_t rax, rbx, rcx, rdx;
	uint64_t rsi, rdi;
	uint64_t rbp, rsp, rip;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t flags, cs, ss, fs, ds;
} shiva_trace_regset_x86_64_t;
#endif

typedef struct shiva_trace_regs {
	struct shiva_trace_regset_x86_64 x86_64;
} shiva_trace_regs_t;

typedef struct shiva_trace_thread {
	char *name;
	uid_t uid;
	gid_t gid;
	pid_t pid;
	pid_t ppid;
	pid_t external_tracer_pid;
	uint64_t flags;
	TAILQ_ENTRY(shiva_trace_thread) _linkage;
} shiva_trace_thread_t;

bool shiva_trace(shiva_ctx_t *, pid_t, shiva_trace_op_t, void *, void *, size_t, shiva_error_t *);
bool shiva_trace_register_handler(shiva_ctx_t *, void * (*)(shiva_ctx_t *), shiva_trace_bp_type_t,
    shiva_error_t *);
bool shiva_trace_set_breakpoint(shiva_ctx_t *, void * (*)(shiva_ctx_t *), uint64_t, shiva_error_t *);
bool shiva_trace_write(struct shiva_ctx *, pid_t, void *, const void *, size_t, shiva_error_t *);
/*
 * shiva_trace_thread.c
 */
bool shiva_trace_thread_insert(shiva_ctx_t *, pid_t, uint64_t *);
#endif
