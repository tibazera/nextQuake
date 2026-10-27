/*
Copyright (C) 2026 nextQuake contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

// Android crash log: on a fatal signal, append a short report (signal, fault
// address, native backtrace) to <basedir>/ezquake/crash.txt, then hand the
// signal back to the previous handler so Android still records its tombstone.
// Nothing is sent anywhere; players attach the file to bug reports themselves.

#ifdef __ANDROID__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <unwind.h>

// Passed in by CMake (see VERSION); fallback keeps the file self-contained.
#ifndef NEXTQUAKE_VERSION
#define NEXTQUAKE_VERSION "0.0.0-dev"
#endif

#define CRASH_MAX_FRAMES 48

static const int crash_signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGABRT, SIGTRAP };
#define CRASH_SIGNAL_COUNT ((int)(sizeof(crash_signals) / sizeof(crash_signals[0])))

static char crash_path[512];
static struct sigaction crash_previous[CRASH_SIGNAL_COUNT];
static volatile sig_atomic_t crash_in_handler;
static char crash_altstack[64 * 1024];

// Async-signal-safe helpers: no stdio, no malloc.
static void crash_write_str(int fd, const char *s)
{
	if (s) {
		write(fd, s, strlen(s));
	}
}

static void crash_write_hex(int fd, uintptr_t value)
{
	char buf[2 + sizeof(uintptr_t) * 2];
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < (int)sizeof(uintptr_t) * 2; ++i) {
		buf[2 + i] = "0123456789abcdef"[(value >> ((sizeof(uintptr_t) * 2 - 1 - i) * 4)) & 0xf];
	}
	write(fd, buf, sizeof(buf));
}

static void crash_write_dec(int fd, long value)
{
	char buf[24];
	int pos = sizeof(buf);
	int negative = value < 0;
	unsigned long v = negative ? (unsigned long)(-value) : (unsigned long)value;

	do {
		buf[--pos] = (char)('0' + v % 10);
		v /= 10;
	} while (v && pos > 1);
	if (negative) {
		buf[--pos] = '-';
	}
	write(fd, buf + pos, sizeof(buf) - pos);
}

static const char *crash_signal_name(int sig)
{
	switch (sig) {
		case SIGSEGV: return "SIGSEGV";
		case SIGBUS:  return "SIGBUS";
		case SIGILL:  return "SIGILL";
		case SIGABRT: return "SIGABRT";
		case SIGTRAP: return "SIGTRAP";
		default:      return "signal";
	}
}

typedef struct {
	uintptr_t frames[CRASH_MAX_FRAMES];
	int count;
} crash_trace_t;

static _Unwind_Reason_Code crash_unwind(struct _Unwind_Context *context, void *arg)
{
	crash_trace_t *trace = (crash_trace_t *)arg;
	uintptr_t pc = _Unwind_GetIP(context);

	if (pc) {
		if (trace->count >= CRASH_MAX_FRAMES) {
			return _URC_END_OF_STACK;
		}
		trace->frames[trace->count++] = pc;
	}
	return _URC_NO_REASON;
}

static void crash_write_report(int sig, siginfo_t *info)
{
	crash_trace_t trace;
	int fd, i;

	fd = open(crash_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) {
		return;
	}

	crash_write_str(fd, "==== nextQuake " NEXTQUAKE_VERSION " crash, unix time ");
	crash_write_dec(fd, (long)time(NULL));
	crash_write_str(fd, " ====\n");
	crash_write_str(fd, crash_signal_name(sig));
	crash_write_str(fd, " (");
	crash_write_dec(fd, sig);
	crash_write_str(fd, ") code ");
	crash_write_dec(fd, info ? info->si_code : 0);
	crash_write_str(fd, " fault address ");
	crash_write_hex(fd, info ? (uintptr_t)info->si_addr : 0);
	crash_write_str(fd, "\nbacktrace:\n");

	// Unwinding from inside a signal handler is best effort; it works for the
	// common case of a fault in engine code and is skipped silently otherwise.
	trace.count = 0;
	_Unwind_Backtrace(crash_unwind, &trace);
	for (i = 0; i < trace.count; ++i) {
		Dl_info dl;
		uintptr_t pc = trace.frames[i];

		crash_write_str(fd, "  #");
		crash_write_dec(fd, i);
		crash_write_str(fd, " pc ");
		if (dladdr((void *)pc, &dl) && dl.dli_fname) {
			const char *lib = strrchr(dl.dli_fname, '/');

			crash_write_hex(fd, pc - (uintptr_t)dl.dli_fbase);
			crash_write_str(fd, " ");
			crash_write_str(fd, lib ? lib + 1 : dl.dli_fname);
			if (dl.dli_sname) {
				crash_write_str(fd, " (");
				crash_write_str(fd, dl.dli_sname);
				crash_write_str(fd, ")");
			}
		}
		else {
			crash_write_hex(fd, pc);
		}
		crash_write_str(fd, "\n");
	}
	crash_write_str(fd, "\n");
	close(fd);
}

static void crash_handler(int sig, siginfo_t *info, void *ucontext)
{
	int i;

	if (!crash_in_handler) {
		crash_in_handler = 1;
		crash_write_report(sig, info);
	}

	// Restore the previous (debuggerd) handler and let it run, so the system
	// crash dialog, tombstone and exit status stay what Android expects.
	for (i = 0; i < CRASH_SIGNAL_COUNT; ++i) {
		if (crash_signals[i] == sig) {
			sigaction(sig, &crash_previous[i], NULL);
			if ((crash_previous[i].sa_flags & SA_SIGINFO) && crash_previous[i].sa_sigaction) {
				crash_previous[i].sa_sigaction(sig, info, ucontext);
				return;
			}
			break;
		}
	}
	raise(sig);
}

void Android_InstallCrashHandler(const char *basedir)
{
	struct sigaction action;
	stack_t stack;
	int i;

	strlcpy(crash_path, basedir, sizeof(crash_path));
	strlcat(crash_path, "/ezquake/crash.txt", sizeof(crash_path));

	// Separate stack so a stack overflow can still be reported.
	stack.ss_sp = crash_altstack;
	stack.ss_size = sizeof(crash_altstack);
	stack.ss_flags = 0;
	sigaltstack(&stack, NULL);

	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_sigaction = crash_handler;
	action.sa_flags = SA_SIGINFO | SA_ONSTACK;

	for (i = 0; i < CRASH_SIGNAL_COUNT; ++i) {
		sigaction(crash_signals[i], &action, &crash_previous[i]);
	}
}

#endif // __ANDROID__
