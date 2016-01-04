/*
 * Copyright 2011-2012 Con Kolivas
 * Copyright 2012-2013 Luke Dashjr
 * Copyright 2016 John Doering
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include "config.h"

#include <unistd.h>

#include "compat.h"
#include "logging.h"
#include "miner.h"

bool opt_debug = false;
bool opt_debug_console = false;  // Only used if opt_debug is also enabled
bool opt_log_output = false;

uint last_day = 0;
bool opt_log_show_date = false;

/* per default priorities higher than LOG_NOTICE are logged */
int opt_log_level = LOG_NOTICE;

static void my_log_curses(int prio, char *f, va_list ap) FORMAT_SYNTAX_CHECK(printf, 2, 0);

static void my_log_curses(__maybe_unused int prio, char *f, va_list ap)
{
	if (opt_quiet && prio != LOG_ERR)
		return;

#ifdef HAVE_CURSES
	extern bool use_curses;
	if (use_curses && log_curses_only(prio, f, ap))
		;
	else
#endif
	{
		int len = strlen(f);
		int cancelstate;
		bool scs;

		strcpy(f + len - 1, "                    \n");

		scs = !pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelstate);
		mutex_lock(&console_lock);
		vprintf(f, ap);
		mutex_unlock(&console_lock);
		if (scs)
			pthread_setcancelstate(cancelstate, &cancelstate);
	}
}

static void log_generic(int prio, const char *fmt, va_list ap) FORMAT_SYNTAX_CHECK(printf, 2, 0);

void vapplog(int prio, const char *fmt, va_list ap)
{
	if (!opt_debug && prio == LOG_DEBUG)
		return;

	log_generic(prio, fmt, ap);
}

void applog(int prio, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vapplog(prio, fmt, ap);
	va_end(ap);
}


/* high-level logging functions, based on global opt_log_level */

/*
 * generic log function used by priority specific ones
 * equals vapplog() without additional priority checks
 */
static void log_generic(int prio, const char *fmt, va_list ap)
{
#ifdef HAVE_SYSLOG_H
	if (use_syslog) {
		vsyslog(prio, fmt, ap);
	}
#else
	if (0) {}
#endif
	else {
		bool writetocon = opt_debug_console || (opt_log_output && prio != LOG_DEBUG) || prio <= LOG_NOTICE;
		bool writetofile = !isatty(fileno((FILE *)stderr));
		if (!(writetocon || writetofile))
			return;

		char *f;
		int len;
		struct timeval tv = {0, 0};
		struct tm _tm;
		struct tm *tm = &_tm;

		gettimeofday(&tv, NULL);

		localtime_r(&tv.tv_sec, tm);

		len = 40 + strlen(fmt) + 22;
		f = alloca(len);

        if(opt_log_show_date || (last_day != tm->tm_mday)) {
            last_day = tm->tm_mday;
            sprintf(f, "[%d-%02d-%02d %02d:%02d:%02d] %s\n",
              tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
              tm->tm_hour, tm->tm_min, tm->tm_sec, fmt);
        } else {
            sprintf(f, "[%02d:%02d:%02d] %s\n",
              tm->tm_hour, tm->tm_min, tm->tm_sec, fmt);
        }

		/* Only output to stderr if it's not going to the screen as well */
		if (writetofile) {
			va_list apc;

			va_copy(apc, ap);
			vfprintf(stderr, f, apc);	/* atomic write to stderr */
			va_end(apc);
			fflush(stderr);
		}

		if (writetocon)
			my_log_curses(prio, f, ap);
	}
}
/* we can not generalize variable argument list */
#define LOG_TEMPLATE(PRIO)		\
	if (PRIO <= opt_log_level) {	\
		va_list ap;		\
		va_start(ap, fmt);	\
		vapplog(PRIO, fmt, ap);	\
		va_end(ap);		\
	}

void log_error(const char *fmt, ...)
{
	LOG_TEMPLATE(LOG_ERR);
}

void log_warning(const char *fmt, ...)
{
	LOG_TEMPLATE(LOG_WARNING);
}

void log_notice(const char *fmt, ...)
{
	LOG_TEMPLATE(LOG_NOTICE);
}

void log_info(const char *fmt, ...)
{
	LOG_TEMPLATE(LOG_INFO);
}

void log_debug(const char *fmt, ...)
{
	LOG_TEMPLATE(LOG_DEBUG);
}
