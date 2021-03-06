/*
 * Copyright (C) 2014-2016 Canonical, Ltd.
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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <inttypes.h>
#include <libgen.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/prctl.h>
#endif

#include "stress-ng.h"

#if !defined(PR_SET_DISABLE)
#define SUID_DUMP_DISABLE	(0)       /* No setuid dumping */
#endif
#if !defined(SUID_DUMP_USER)
#define SUID_DUMP_USER		(1)       /* Dump as user of process */
#endif

#if defined(__linux__) && defined(PRCTL_TIMER_SLACK)
static unsigned long timer_slack = 0;
#endif

static char *stress_temp_path = ".";

/*
 *  stress_get_pagesize()
 *	get pagesize
 */
size_t stress_get_pagesize(void)
{
#ifdef _SC_PAGESIZE
	long sz;
#endif
	static size_t page_size = 0;

	if (page_size > 0)
		return page_size;

#ifdef _SC_PAGESIZE
        sz = sysconf(_SC_PAGESIZE);
	page_size = (sz <= 0) ? PAGE_4K : (size_t)sz;
#else
        page_size = PAGE_4K;
#endif

	return page_size;
}

/*
 *  stress_get_processors_online()
 *	get number of processors that are online
 */
long stress_get_processors_online(void)
{
	static long processors_online = 0;

	if (processors_online > 0)
		return processors_online;

#ifdef _SC_NPROCESSORS_ONLN
	processors_online = sysconf(_SC_NPROCESSORS_ONLN);
	return processors_online;
#else
	return -1;
#endif
}

/*
 *  stress_get_processors_configured()
 *	get number of processors that are configured
 */
long stress_get_processors_configured(void)
{
	static long processors_configured = 0;

	if (processors_configured > 0)
		return processors_configured;

#ifdef _SC_NPROCESSORS_CONF
	processors_configured = sysconf(_SC_NPROCESSORS_CONF);
	return processors_configured;
#else
	return -1;
#endif
}

/*
 *  stress_get_ticks_per_second()
 *	get number of ticks perf second
 */
long stress_get_ticks_per_second(void)
{
	static long ticks_per_second = 0;

	if (ticks_per_second > 0)
		return ticks_per_second;

#ifdef _SC_CLK_TCK
	ticks_per_second = sysconf(_SC_CLK_TCK);
	return ticks_per_second;
#else
	return -1;
#endif
}

/*
 *  stress_get_memlimits()
 *	get SHMALL and memory in system
 *	these are set to zero on failure
 */
void stress_get_memlimits(
	size_t *shmall,
	size_t *freemem,
	size_t *totalmem)
{
#if defined(__linux__)
	struct sysinfo info;
	FILE *fp;
#endif
	*shmall = 0;
	*freemem = 0;
	*totalmem = 0;

#if defined(__linux__)
	if (sysinfo(&info) == 0) {
		*freemem = info.freeram * info.mem_unit;
		*totalmem = info.totalram * info.mem_unit;
	}

	fp = fopen("/proc/sys/kernel/shmall", "r");
	if (!fp)
		return;

	if (fscanf(fp, "%zu", shmall) != 1) {
		fclose(fp);
		return;
	}
	fclose(fp);
#endif
}

/*
 *  stress_get_load_avg()
 *	get load average
 */
int stress_get_load_avg(
	double *min1,
	double *min5,
	double *min15)
{
#if NEED_GLIBC(2, 2, 0)
	int rc;
	double loadavg[3];

	rc = getloadavg(loadavg, 3);
	if (rc < 0)
		goto fail;

	*min1 = loadavg[0];
	*min5 = loadavg[1];
	*min15 = loadavg[2];

	return 0;
fail:
#elif defined(__linux__)
	struct sysinfo info;
	const double scale = 1.0 / (double)(1 << SI_LOAD_SHIFT);

	if (sysinfo(&info) < 0)
		goto fail;

	*min1 = info.loads[0] * scale;
	*min5 = info.loads[1] * scale;
	*min15 = info.loads[2] * scale;

	return 0;
fail:
#endif
	*min1 = *min5 = *min15 = 0.0;
	return -1;
}

/*
 *  stress_parent_died_alarm()
 *	send child SIGALRM if the parent died
 */
void stress_parent_died_alarm(void)
{
#if defined(__linux__) && defined(PR_SET_PDEATHSIG)
	(void)prctl(PR_SET_PDEATHSIG, SIGALRM);
#endif
}

/*
 *  stress_process_dumpable()
 *	set dumpable flag, e.g. produce a core dump or not,
 *	don't print an error if these fail, it's not that
 *	critical
 */
int stress_process_dumpable(const bool dumpable)
{
	char path[PATH_MAX];
	int fd, rc = 0;

#if defined(__linux__) && defined(PR_SET_DUMPABLE)
	(void)prctl(PR_SET_DUMPABLE, 
		dumpable ? SUID_DUMP_USER : SUID_DUMP_DISABLE);
#endif
	snprintf(path, sizeof(path), "/proc/%u/coredump_filter", getpid());
	if ((fd = open(path, O_WRONLY)) >= 0) {
		char const *str =
			dumpable ? "0x33" : "0x00";

		if (write(fd, str, strlen(str)) < 0)
			rc = -1;
		(void)close(fd);
	}
	return rc;
}

/*
 *  stress_set_timer_slackigned_longns()
 *	set timer slack in nanoseconds
 */
void stress_set_timer_slack_ns(const char *optarg)
{
#if defined(__linux__) && defined(PRCTL_TIMER_SLACK)
	timer_slack = get_unsigned_long(optarg);
#else
	(void)optarg;
#endif
}

/*
 *  stress_set_timer_slack()
 *	set timer slack
 */
void stress_set_timer_slack(void)
{
#if defined(__linux__) && defined(PRCTL_TIMER_SLACK)
	(void)prctl(PR_SET_TIMERSLACK, timer_slack);
#endif
}

/*
 *  set_proc_name()
 *	Set process name, we don't care if it fails
 */
void set_proc_name(const char *name)
{
#if defined(__linux__) && defined(PR_SET_NAME)
	if (!(opt_flags & OPT_FLAGS_KEEP_NAME))
		(void)prctl(PR_SET_NAME, name);
#else
	(void)name;	/* No-op */
#endif
}

/*
 *  munge_underscore()
 *	turn '_' to '-' in strings
 */
char *munge_underscore(char *str)
{
	static char munged[128];
	char *src, *dst;
	size_t str_len = strlen(str);
	ssize_t len = STRESS_MINIMUM(str_len, sizeof(munged) - 1);

	for (src = str, dst = munged; *src && (dst - munged) < len; src++)
		*dst++ = (*src == '_' ? '-' : *src);

	*dst = '\0';

	return munged;
}

/*
 *  stress_get_stack_direction()
 *      determine which way the stack goes, up / down
 *	just pass in any var on the stack before calling
 */
ssize_t stress_get_stack_direction(const void *val1)
{
	uint8_t val2;

	return ((uint8_t *)val1 - &val2) > 0 ? 1 : -1;
}

/*
 *  uint64_zero()
 *	return uint64 zero in way that force less smart
 *	static analysers to realise we are doing this
 *	to force a division by zero. I'd like to have
 *	a better solution than this ghastly way.
 */
uint64_t uint64_zero(void)
{
	return 0ULL;
}

/*
 *  stress_set_temp_path()
 *	set temporary file path, default
 *	is . - current dir
 */
int stress_set_temp_path(char *path)
{
	stress_temp_path = path;

	if (access(path, R_OK | W_OK) < 0) {
		fprintf(stderr, "temp-path '%s' must be readable "
			"and writeable\n", path);
		return -1;
	}

	return 0;
}

/*
 *  stress_temp_filename()
 *      construct a temp filename
 */
int stress_temp_filename(
        char *path,
        const size_t len,
        const char *name,
        const pid_t pid,
        const uint32_t instance,
        const uint64_t magic)
{
	return snprintf(path, len, "%s/tmp-%s-%i-%"
		PRIu32 "/%s-%i-%"
                PRIu32 "-%" PRIu64,
		stress_temp_path,
                name, pid, instance,
		name, pid, instance, magic);
}

/*
 *  stress_temp_dir()
 *	create a temporary directory name
 */
int stress_temp_dir(
	char *path,
        const size_t len,
	const char *name,
        const pid_t pid,
        const uint32_t instance)
{
	return snprintf(path, len, "%s/tmp-%s-%i-%" PRIu32,
		stress_temp_path, name, pid, instance);
}

/*
 *   stress_temp_dir_mk()
 *	create a temporary directory
 */
int stress_temp_dir_mk(
	const char *name,
        const pid_t pid,
        const uint32_t instance)
{
	int ret;
	char tmp[PATH_MAX];

	stress_temp_dir(tmp, sizeof(tmp), name, pid, instance);
	ret = mkdir(tmp, S_IRWXU);
	if (ret < 0)
		pr_fail_err(name, "mkdir");

	return ret;
}

/*
 *  stress_temp_dir_rm()
 *	remove a temporary directory
 */
int stress_temp_dir_rm(
	const char *name,
        const pid_t pid,
        const uint32_t instance)
{
	int ret;
	char tmp[PATH_MAX + 1];

	stress_temp_dir(tmp, sizeof(tmp), name, pid, instance);
	ret = rmdir(tmp);
	if (ret < 0)
		pr_fail_err(name, "rmdir");

	return ret;
}

/*
 *  stress_cwd_readwriteable()
 *	check if cwd is read/writeable
 */
void stress_cwd_readwriteable(void)
{
	char path[PATH_MAX];

	if (getcwd(path, sizeof(path)) == NULL) {
		pr_dbg(stderr, "Cannot determine current working directory\n");
		return;
	}
	if (access(path, R_OK | W_OK)) {
		pr_inf(stderr, "Working directory %s is not read/writeable, "
			"some I/O tests may fail\n", path);
		return;
	}
}

/*
 *  stress_strsignal()
 *	signum to human readable string
 */
const char *stress_strsignal(const int signum)
{
	static char buffer[128];
	char *str = NULL;

#if defined(NSIG)
	if ((signum >= 0) && (signum < NSIG))
		str = strsignal(signum);
#elif defined(_NSIG)
	if ((signum >= 0) && (signum < N_SIG))
		str = strsignal(signum);
#endif
	if (str) {
		snprintf(buffer, sizeof(buffer), "signal %d (%s)",
			signum, str);
	} else {
		snprintf(buffer, sizeof(buffer), "signal %d",
			signum);
	}
	return buffer;
}


/*
 *  stress_strnrnd()
 *	fill string with random chars
 */
void stress_strnrnd(char *str, const size_t len)
{
	char *end = str + len;

	while (str < end - 1)
		*str++ = (mwc8() % 26) + 'a';

	*str = '\0';
}

/*
 *  pr_yaml_runinfo()
 *	log info about the system we are running stress-ng on
 */
void pr_yaml_runinfo(FILE *yaml)
{
#if defined(__linux__)
	struct utsname uts;
	struct sysinfo info;
#endif
	time_t t;
	struct tm *tm = NULL;
	char hostname[128];
	char *user = getlogin();

	pr_yaml(yaml, "system-info:\n");
	if (time(&t) != ((time_t)-1))
		tm = localtime(&t);

	pr_yaml(yaml, "      stress-ng-version: " VERSION "\n");
	pr_yaml(yaml, "      run-by: %s\n", user ? user : "unknown");
	if (tm) {
		pr_yaml(yaml, "      date-yyyy-mm-dd: %4.4d:%2.2d:%2.2d\n",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
		pr_yaml(yaml, "      time-hh-mm-ss: %2.2d:%2.2d:%2.2d\n",
			tm->tm_hour, tm->tm_min, tm->tm_sec);
		pr_yaml(yaml, "      epoch-secs: %ld\n", (long)t);
	}
	if (!gethostname(hostname, sizeof(hostname)))
		pr_yaml(yaml, "      hostname: %s\n", hostname);
#if defined(__linux__)
	if (uname(&uts) == 0) {
		pr_yaml(yaml, "      sysname: %s\n", uts.sysname);
		pr_yaml(yaml, "      nodename: %s\n", uts.nodename);
		pr_yaml(yaml, "      release: %s\n", uts.release);
		pr_yaml(yaml, "      version: %s\n", uts.version);
		pr_yaml(yaml, "      machine: %s\n", uts.machine);
	}
	if (sysinfo(&info) == 0) {
		pr_yaml(yaml, "      uptime: %ld\n", info.uptime);
		pr_yaml(yaml, "      totalram: %lu\n", info.totalram);
		pr_yaml(yaml, "      freeram: %lu\n", info.freeram);
		pr_yaml(yaml, "      sharedram: %lu\n", info.sharedram);
		pr_yaml(yaml, "      bufferram: %lu\n", info.bufferram);
		pr_yaml(yaml, "      totalswap: %lu\n", info.totalswap);
		pr_yaml(yaml, "      freeswap: %lu\n", info.freeswap);
	}
#endif
	pr_yaml(yaml, "      pagesize: %zd\n", stress_get_pagesize());
	pr_yaml(yaml, "      cpus: %ld\n", stress_get_processors_configured());
	pr_yaml(yaml, "      cpus-online: %ld\n", stress_get_processors_online());
	pr_yaml(yaml, "      ticks-per-second: %ld\n", stress_get_ticks_per_second());
	pr_yaml(yaml, "\n");
}
