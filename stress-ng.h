/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */

#ifndef __STRESS_NG_H__
#define __STRESS_NG_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <semaphore.h>

#define _GNU_SOURCE
/* GNU HURD */
#ifndef PATH_MAX
#define PATH_MAX 		(4096)
#endif

#define STRESS_FD_MAX		(65536)
#define STRESS_PROCS_MAX	(1024)

#ifndef PIPE_BUF
#define PIPE_BUF		(512)
#endif
#define SOCKET_BUF		(8192)

/* Option bit masks */
#define OPT_FLAGS_NO_CLEAN	0x00000001	/* Don't remove hdd files */
#define OPT_FLAGS_DRY_RUN	0x00000002	/* Don't actually run */
#define OPT_FLAGS_METRICS	0x00000004	/* Dump metrics at end */
#define OPT_FLAGS_VM_KEEP	0x00000008	/* Don't keep re-allocating */
#define OPT_FLAGS_RANDOM	0x00000010	/* Randomize */
#define OPT_FLAGS_SET		0x00000020	/* Set if user specifies stress procs */
#define OPT_FLAGS_KEEP_NAME	0x00000040	/* Keep stress names to stress-ng */
#define OPT_FLAGS_UTIME_FSYNC	0x00000080	/* fsync after utime modification */
#define OPT_FLAGS_METRICS_BRIEF	0x00000100	/* dump brief metrics */
#define OPT_FLAGS_VERIFY	0x00000200	/* verify mode */

/* debug output bitmasks */
#define PR_ERROR		0x00010000	/* Print errors */
#define PR_INFO			0x00020000	/* Print info */
#define PR_DEBUG		0x00040000	/* Print debug */
#define PR_FAIL			0x00080000	/* Print test failure message */
#define PR_ALL			(PR_ERROR | PR_INFO | PR_DEBUG | PR_FAIL)

#define pr_dbg(fp, fmt, args...)	print(fp, PR_DEBUG, fmt, ## args)
#define pr_inf(fp, fmt, args...)	print(fp, PR_INFO, fmt, ## args)
#define pr_err(fp, fmt, args...)	print(fp, PR_ERROR, fmt, ## args)
#define pr_fail(fp, fmt, args...)	print(fp, PR_FAIL, fmt, ## args)

#define pr_failed_err(name, what)	pr_failed(PR_ERROR, name, what)
#define pr_failed_dbg(name, what)	pr_failed(PR_DEBUG, name, what)

#define KB			(1024ULL)
#define	MB			(KB * KB)
#define GB			(KB * KB * KB)

#define PAGE_4K_SHIFT		(12)
#define PAGE_4K			(1 << PAGE_4K_SHIFT)

#define MIN_VM_BYTES		(4 * KB)
#define MAX_VM_BYTES		(1 * GB)
#define DEFAULT_VM_BYTES	(256 * MB)

#define MIN_MMAP_BYTES		(4 * KB)
#define MAX_MMAP_BYTES		(1 * GB)
#define DEFAULT_MMAP_BYTES	(256 * MB)

#define MIN_VM_STRIDE		(1)
#define MAX_VM_STRIDE		(1 * MB)
#define DEFAULT_VM_STRIDE	(4 * KB)

#define MIN_HDD_BYTES		(1 * MB)
#define MAX_HDD_BYTES		(256 * GB)
#define DEFAULT_HDD_BYTES	(1 * GB)

#define MIN_HDD_WRITE_SIZE	(1)
#define MAX_HDD_WRITE_SIZE	(4 * MB)
#define DEFAULT_HDD_WRITE_SIZE	(64 * 1024)

#define MIN_VM_HANG		(0)
#define MAX_VM_HANG		(3600)
#define DEFAULT_VM_HANG		(~0ULL)

#define DEFAULT_TIMEOUT		(60 * 60 * 24)
#define DEFAULT_BACKOFF		(0)
#define DEFAULT_DENTRIES	(2048)
#define DEFAULT_LINKS		(8192)
#define DEFAULT_DIRS		(8192)

#define DEFAULT_OPS_MIN		(100ULL)
#define DEFAULT_OPS_MAX		(100000000ULL)

#define SWITCH_STOP		'X'
#define PIPE_STOP		"PIPE_STOP"

#define MEM_CHUNK_SIZE		(65536 * 8)
#define UNDEFINED		(-1)

#define PAGE_MAPPED		(0x01)
#define PAGE_MAPPED_FAIL	(0x02)

#define FFT_SIZE		(4096)

#define SIEVE_GETBIT(a, i)	(a[i / 32] & (1 << (i & 31)))
#define SIEVE_CLRBIT(a, i)	(a[i / 32] &= ~(1 << (i & 31)))
#define SIEVE_SIZE 		(10000000)

#define MWC_SEED_Z		(362436069)
#define MWC_SEED_W		(521288629)

#define MWC_SEED()		mwc_seed(MWC_SEED_W, MWC_SEED_Z)

/* stress process prototype */
typedef int (*func)(uint64_t *const counter, const uint32_t instance,
		    const uint64_t max_ops, const char *name);

/* Help information for options */
typedef struct {
	const char *opt_s;		/* short option */
	const char *opt_l;		/* long option */
	const char *description;	/* description */
} help_t;

/* Stress tests */
typedef enum {
	STRESS_IOSYNC	= 0,
	STRESS_CPU,
	STRESS_VM,
	STRESS_HDD,
	STRESS_FORK,
	STRESS_SWITCH,
	STRESS_PIPE,
	STRESS_CACHE,
	STRESS_SOCKET,
	STRESS_YIELD,
	STRESS_FALLOCATE,
	STRESS_FLOCK,
	STRESS_AFFINITY,
	STRESS_TIMER,
	STRESS_DENTRY,
	STRESS_URANDOM,
	STRESS_SEMAPHORE,
	STRESS_OPEN,
	STRESS_SIGQUEUE,
	STRESS_POLL,
	STRESS_LINK,
	STRESS_SYMLINK,
	STRESS_DIR,
	STRESS_SIGSEGV,
	STRESS_MMAP,
	STRESS_QSORT,
	STRESS_BIGHEAP,
	STRESS_RENAME,
	STRESS_UTIME,
	STRESS_FSTAT,
	STRESS_MSG,
	/* Add new stress tests here */
	STRESS_MAX
} stress_id;

/* Command line long options */
typedef enum {
	OPT_QUERY = '?',
	OPT_ALL = 'a',
	OPT_BACKOFF = 'b',
	OPT_CPU = 'c',
	OPT_HDD = 'd',
	OPT_FORK = 'f',
	OPT_IOSYNC = 'i',
	OPT_HELP = 'h',
	OPT_KEEP_NAME = 'k',
	OPT_CPU_LOAD = 'l',
	OPT_VM = 'm',
	OPT_DRY_RUN = 'n',
	OPT_RENAME = 'R',
	OPT_OPEN = 'o',
	OPT_PIPE = 'p',
	OPT_QUIET = 'q',
	OPT_RANDOM = 'r',
	OPT_SWITCH = 's',
	OPT_TIMEOUT = 't',
#if defined (__linux__)
	OPT_URANDOM = 'u',
#endif
	OPT_VERBOSE = 'v',
	OPT_YIELD = 'y',
	OPT_CACHE = 'C',
	OPT_DENTRY = 'D',
	OPT_FALLOCATE = 'F',
	OPT_METRICS = 'M',
	OPT_POLL = 'P',
	OPT_SOCKET = 'S',
#if defined (__linux__)
	OPT_TIMER = 'T',
#endif
	OPT_VERSION = 'V',
	OPT_BIGHEAP = 'B',
	OPT_VM_BYTES = 0x80,
	OPT_VM_STRIDE,
	OPT_VM_HANG,
	OPT_VM_KEEP,
#ifdef MAP_POPULATE
	OPT_VM_MMAP_POPULATE,
#endif
#ifdef MAP_LOCKED
	OPT_VM_MMAP_LOCKED,
#endif
	OPT_HDD_BYTES,
	OPT_HDD_NOCLEAN,
	OPT_HDD_WRITE_SIZE,
	OPT_CPU_OPS,
	OPT_CPU_METHOD,
	OPT_IOSYNC_OPS,
	OPT_VM_OPS,
	OPT_HDD_OPS,
	OPT_FORK_OPS,
	OPT_SWITCH_OPS,
	OPT_PIPE_OPS,
	OPT_CACHE_OPS,
	OPT_SOCKET_OPS,
	OPT_SOCKET_PORT,
#if defined (__linux__)
	OPT_SCHED,
	OPT_SCHED_PRIO,
	OPT_IONICE_CLASS,
	OPT_IONICE_LEVEL,
	OPT_AFFINITY,
	OPT_AFFINITY_OPS,
	OPT_TIMER_OPS,
	OPT_TIMER_FREQ,
	OPT_URANDOM_OPS,
#endif
#if _POSIX_C_SOURCE >= 199309L
	OPT_SIGQUEUE,
	OPT_SIGQUEUE_OPS,
#endif
#if defined (_POSIX_PRIORITY_SCHEDULING)
	OPT_YIELD_OPS,
#endif
#if _XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L
	OPT_FALLOCATE_OPS,
#endif
	OPT_FLOCK,
	OPT_FLOCK_OPS,
	OPT_DENTRY_OPS,
	OPT_DENTRIES,
	OPT_SEMAPHORE,
	OPT_SEMAPHORE_OPS,
	OPT_OPEN_OPS,
	OPT_POLL_OPS,
	OPT_LINK,
	OPT_LINK_OPS,
	OPT_SYMLINK,
	OPT_SYMLINK_OPS,
	OPT_DIR,
	OPT_DIR_OPS,
	OPT_SIGSEGV,
	OPT_SIGSEGV_OPS,
	OPT_MMAP,
	OPT_MMAP_OPS,
	OPT_MMAP_BYTES,
	OPT_QSORT,
	OPT_QSORT_OPS,
	OPT_QSORT_INTEGERS,
	OPT_BIGHEAP_OPS,
	OPT_RENAME_OPS,
	OPT_UTIME,
	OPT_UTIME_OPS,
	OPT_UTIME_FSYNC,
	OPT_FSTAT,
	OPT_FSTAT_OPS,
	OPT_FSTAT_DIR,
	OPT_METRICS_BRIEF,
	OPT_VERIFY,
	OPT_MSG,
	OPT_MSG_OPS
} stress_op;

/* stress test metadata */
typedef struct {
	const func stress_func;		/* stress test function */
	const stress_id id;		/* stress test ID */
	const short int short_getopt;	/* getopt short option */
	const stress_op op;		/* ops option */
	const char *name;		/* name of stress test */
} stress_t;

/*
 *  the CPU stress test has different classes of cpu stressor
 */
typedef void (*stress_cpu_func)(void);

typedef struct {
	const char		*name;	/* human readable form of stressor */
	const stress_cpu_func	func;	/* the stressor function */
} stress_cpu_stressor_info_t;

#if defined (__linux__)
/*
 *  See ioprio_set(2) and linux/ioprio.h, glibc has no definitions
 *  for these at present. Also refer to Documentation/block/ioprio.txt
 *  in the Linux kernel source.
 */
#define IOPRIO_CLASS_RT 	(1)
#define IOPRIO_CLASS_BE		(2)
#define IOPRIO_CLASS_IDLE	(3)

#define IOPRIO_WHO_PROCESS	(1)
#define IOPRIO_WHO_PGRP		(2)
#define IOPRIO_WHO_USER		(3)

#define IOPRIO_PRIO_VALUE(class, data)	(((class) << 13) | data)
#endif

typedef struct {
	pid_t	pid;		/* process id */
	double	start;		/* time process started */
	double	finish;		/* time process got reaped */
} proc_info_t;

typedef struct {
	const char	ch;	/* Scaling suffix */
	const uint64_t	scale;	/* Amount to scale by */
} scale_t;

/* stress process prototype */
typedef int (*func)(uint64_t *const counter, const uint32_t
	instance, const uint64_t max_ops, const char *name);


/* Various option settings and flags */
extern const char *app_name;				/* Name of application */
extern sem_t	sem;					/* stress_semaphore sem */
extern uint8_t *mem_chunk;				/* Cache load shared memory */
extern uint64_t	opt_dentries;				/* dentries per loop */
extern uint64_t opt_ops[STRESS_MAX];			/* max number of bogo ops */
extern uint64_t	opt_vm_hang; 				/* VM delay */
extern uint64_t	opt_hdd_bytes; 				/* HDD size in byts */
extern uint64_t opt_hdd_write_size;
extern uint64_t	opt_timeout;				/* timeout in seconds */
extern uint64_t	mwc_z, mwc_w;				/* random number vals */
extern uint64_t opt_qsort_size; 			/* Default qsort size */
extern int64_t	opt_backoff ;				/* child delay */
extern int32_t	started_procs[STRESS_MAX];		/* number of processes per stressor */
extern int32_t	opt_flags;				/* option flags */
extern int32_t  opt_cpu_load;				/* CPU max load */
extern stress_cpu_stressor_info_t *opt_cpu_stressor;	/* Default stress CPU method */
extern size_t	opt_vm_bytes;				/* VM bytes */
extern size_t	opt_vm_stride;				/* VM stride */
extern int	opt_vm_flags;				/* VM mmap flags */
extern size_t	opt_mmap_bytes;				/* MMAP size */
extern pid_t	socket_server, socket_client;		/* pids of socket client/servers */
#if defined (__linux__)
extern uint64_t	opt_timer_freq;				/* timer frequency (Hz) */
extern int	opt_sched;				/* sched policy */
extern int	opt_sched_priority;			/* sched priority */
extern int 	opt_ionice_class;			/* ionice class */
extern int	opt_ionice_level;			/* ionice level */
#endif
extern int	opt_socket_port;			/* Default socket port */
extern long int	opt_nprocessors_online;			/* Number of processors online */
extern char	*opt_fstat_dir;				/* Default fstat directory */
extern volatile bool opt_do_run;			/* false to exit stressor */
extern proc_info_t *procs[STRESS_MAX];			/* per process info */
extern stress_cpu_stressor_info_t cpu_methods[];	/* cpu stressor methods */

/*
 *  externs to force gcc to stash computed values and hence
 *  to stop the optimiser optimising code away to zero. The
 *  *_put funcs are essentially no-op functions.
 */
extern void double_put(const double a);
extern void uint64_put(const uint64_t a);

extern double timeval_to_double(const struct timeval *tv);
extern double time_now(void);
extern uint64_t mwc(void);
extern void mwc_seed(const uint64_t w, const uint64_t z);
extern void mwc_reseed(void);
extern stress_cpu_stressor_info_t *stress_cpu_find_by_name(const char *name);
extern void pr_failed(const int flag, const char *name, const char *what);
extern void set_oom_adjustment(const char *name, bool killable);

#define STRESS(name)								\
	extern int name(uint64_t *const counter, const uint32_t instance,	\
        const uint64_t max_ops, const char *name)				

STRESS(stress_affinity);
STRESS(stress_bigheap);
STRESS(stress_cache);
STRESS(stress_cpu);
STRESS(stress_dir);
STRESS(stress_dentry);
STRESS(stress_hdd);
STRESS(stress_fallocate);
STRESS(stress_flock);
STRESS(stress_fork);
STRESS(stress_fstat);
STRESS(stress_iosync);
STRESS(stress_link);
STRESS(stress_mmap);
STRESS(stress_msg);
STRESS(stress_noop);
STRESS(stress_open);
STRESS(stress_pipe);
STRESS(stress_poll);
STRESS(stress_qsort);
STRESS(stress_rename);
STRESS(stress_semaphore);
STRESS(stress_sigsegv);
STRESS(stress_sigq);
STRESS(stress_socket);
STRESS(stress_switch);
STRESS(stress_symlink);
STRESS(stress_timer);
STRESS(stress_urandom);
STRESS(stress_utime);
STRESS(stress_vm);
STRESS(stress_yield);

extern int print(FILE *fp, const int flag,
	const char *const fmt, ...) __attribute__((format(printf, 3, 4)));

#endif