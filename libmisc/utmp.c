/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include "defines.h"
#include "prototypes.h"

#include <utmp.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#ident "$Id$"


/*
 * is_my_tty -- determine if "tty" is the same TTY stdin is using
 */
static bool is_my_tty (const char *tty)
{
	/* full_tty shall be at least sizeof utmp.ut_line + 5 */
	char full_tty[200];
	/* tmptty shall be bigger than full_tty */
	static char tmptty[sizeof (full_tty)+1];

	if ('/' != *tty) {
		(void) snprintf (full_tty, sizeof full_tty, "/dev/%s", tty);
		tty = &full_tty[0];
	}

	if ('\0' == tmptty[0]) {
		const char *tname = ttyname (STDIN_FILENO);
		if (NULL != tname)
			(void) strlcpy (tmptty, tname, sizeof tmptty);
	}

	if ('\0' == tmptty[0]) {
		(void) puts (_("Unable to determine your tty name."));
		exit (EXIT_FAILURE);
	} else if (strncmp (tty, tmptty, sizeof (tmptty)) != 0) {
		return false;
	} else {
		return true;
	}
}

/*
 * get_current_utmp - return the most probable utmp entry for the current
 *                    session
 *
 *	The utmp file is scanned for an entry with the same process ID.
 *	The line entered by the *getty / telnetd, etc. should also match
 *	the current terminal.
 *
 *	When an entry is returned by get_current_utmp, and if the utmp
 *	structure has a ut_id field, this field should be used to update
 *	the entry information.
 *
 *	Return NULL if no entries exist in utmp for the current process.
 */
/*@null@*/ /*@only@*/struct utmp *get_current_utmp (void)
{
	struct utmp *ut;
	struct utmp *ret = NULL;

	setutent ();

	/* First, try to find a valid utmp entry for this process.  */
	while ((ut = getutent ()) != NULL) {
		if (   (ut->ut_pid == getpid ())
#ifdef HAVE_STRUCT_UTMP_UT_ID
		    && ('\0' != ut->ut_id[0])
#endif
#ifdef HAVE_STRUCT_UTMP_UT_TYPE
		    && (   (LOGIN_PROCESS == ut->ut_type)
		        || (USER_PROCESS  == ut->ut_type))
#endif
		    /* A process may have failed to close an entry
		     * Check if this entry refers to the current tty */
		    && is_my_tty (ut->ut_line)) {
			break;
		}
	}

	if (NULL != ut) {
		ret = (struct utmp *) xmalloc (sizeof (*ret));
		memcpy (ret, ut, sizeof (*ret));
	}

	endutent ();

	return ret;
}


#ifndef USE_PAM
/*
 * Some systems already have updwtmp() and possibly updwtmpx().  Others
 * don't, so we re-implement these functions if necessary.
 */
#ifndef HAVE_UPDWTMP
static void updwtmp (const char *filename, const struct utmp *ut)
{
	int fd;

	fd = open (filename, O_APPEND | O_WRONLY, 0);
	if (fd >= 0) {
		write (fd, ut, sizeof (*ut));
		close (fd);
	}
}
#endif				/* ! HAVE_UPDWTMP */

#endif				/* ! USE_PAM */


/*
 * prepare_utmp - prepare an utmp entry so that it can be logged in a
 *                utmp/wtmp file.
 *
 *	It accepts an utmp entry in input (ut) to return an entry with
 *	the right ut_id. This is typically an entry returned by
 *	get_current_utmp
 *	If ut is NULL, ut_id will be forged based on the line argument.
 *
 *	The ut_host field of the input structure may also be kept, and is
 *	used to define the ut_addr/ut_addr_v6 fields. (if these fields
 *	exist)
 *
 *	Other fields are discarded and filed with new values (if they
 *	exist).
 *
 *	The returned structure shall be freed by the caller.
 */
/*@only@*/struct utmp *prepare_utmp (const char *name,
                                     const char *line,
                                     const char *host,
                                     /*@null@*/const struct utmp *ut)
{
	struct timeval tv;
	char *hostname = NULL;
	struct utmp *utent;

	assert (NULL != name);
	assert (NULL != line);



	if (   (NULL != host)
	    && ('\0' != host[0])) {
		hostname = (char *) xmalloc (strlen (host) + 1);
		strcpy (hostname, host);
#ifdef HAVE_STRUCT_UTMP_UT_HOST
	} else if (   (NULL != ut)
	           && ('\0' != ut->ut_host[0])) {
		hostname = (char *) xmalloc (sizeof (ut->ut_host) + 1);
		strncpy (hostname, ut->ut_host, sizeof (ut->ut_host));
		hostname[sizeof (ut->ut_host)] = '\0';
#endif				/* HAVE_STRUCT_UTMP_UT_HOST */
	}

	if (strncmp(line, "/dev/", 5) == 0) {
		line += 5;
	}


	utent = (struct utmp *) xmalloc (sizeof (*utent));
	memzero (utent, sizeof (*utent));



#ifdef HAVE_STRUCT_UTMP_UT_TYPE
	utent->ut_type = USER_PROCESS;
#endif				/* HAVE_STRUCT_UTMP_UT_TYPE */
	utent->ut_pid = getpid ();
	strncpy (utent->ut_line, line,      sizeof (utent->ut_line) - 1);
#ifdef HAVE_STRUCT_UTMP_UT_ID
	if (NULL != ut) {
		strncpy (utent->ut_id, ut->ut_id, sizeof (utent->ut_id));
	} else {
		/* XXX - assumes /dev/tty?? */
		strncpy (utent->ut_id, line + 3, sizeof (utent->ut_id) - 1);
	}
#endif				/* HAVE_STRUCT_UTMP_UT_ID */
#ifdef HAVE_STRUCT_UTMP_UT_NAME
	strncpy (utent->ut_name, name,      sizeof (utent->ut_name));
#endif				/* HAVE_STRUCT_UTMP_UT_NAME */
#ifdef HAVE_STRUCT_UTMP_UT_USER
	strncpy (utent->ut_user, name,      sizeof (utent->ut_user) - 1);
#endif				/* HAVE_STRUCT_UTMP_UT_USER */
	if (NULL != hostname) {
		struct addrinfo *info = NULL;
#ifdef HAVE_STRUCT_UTMP_UT_HOST
		strncpy (utent->ut_host, hostname, sizeof (utent->ut_host) - 1);
#endif				/* HAVE_STRUCT_UTMP_UT_HOST */
#ifdef HAVE_STRUCT_UTMP_UT_SYSLEN
		utent->ut_syslen = MIN (strlen (hostname),
		                        sizeof (utent->ut_host));
#endif				/* HAVE_STRUCT_UTMP_UT_SYSLEN */
#if defined(HAVE_STRUCT_UTMP_UT_ADDR) || defined(HAVE_STRUCT_UTMP_UT_ADDR_V6)
		if (getaddrinfo (hostname, NULL, NULL, &info) == 0) {
			/* getaddrinfo might not be reliable.
			 * Just try to log what may be useful.
			 */
			if (info->ai_family == AF_INET) {
				struct sockaddr_in *sa =
					(struct sockaddr_in *) info->ai_addr;
#ifdef HAVE_STRUCT_UTMP_UT_ADDR
				memcpy (&(utent->ut_addr),
				        &(sa->sin_addr),
				        MIN (sizeof (utent->ut_addr),
				             sizeof (sa->sin_addr)));
#endif				/* HAVE_STRUCT_UTMP_UT_ADDR */
#ifdef HAVE_STRUCT_UTMP_UT_ADDR_V6
				memcpy (utent->ut_addr_v6,
				        &(sa->sin_addr),
				        MIN (sizeof (utent->ut_addr_v6),
				             sizeof (sa->sin_addr)));
			} else if (info->ai_family == AF_INET6) {
				struct sockaddr_in6 *sa =
					(struct sockaddr_in6 *) info->ai_addr;
				memcpy (utent->ut_addr_v6,
				        &(sa->sin6_addr),
				        MIN (sizeof (utent->ut_addr_v6),
				             sizeof (sa->sin6_addr)));
#endif				/* HAVE_STRUCT_UTMP_UT_ADDR_V6 */
			}
			freeaddrinfo (info);
		}
#endif		/* HAVE_STRUCT_UTMP_UT_ADDR || HAVE_STRUCT_UTMP_UT_ADDR_V6 */
		free (hostname);
	}
	/* ut_exit is only for DEAD_PROCESS */
	utent->ut_session = getsid (0);
	if (gettimeofday (&tv, NULL) == 0) {
#ifdef HAVE_STRUCT_UTMP_UT_TIME
		utent->ut_time = tv.tv_sec;
#endif				/* HAVE_STRUCT_UTMP_UT_TIME */
#ifdef HAVE_STRUCT_UTMP_UT_XTIME
		utent->ut_xtime = tv.tv_usec;
#endif				/* HAVE_STRUCT_UTMP_UT_XTIME */
#ifdef HAVE_STRUCT_UTMP_UT_TV
		utent->ut_tv.tv_sec  = tv.tv_sec;
		utent->ut_tv.tv_usec = tv.tv_usec;
#endif				/* HAVE_STRUCT_UTMP_UT_TV */
	}

	return utent;
}

/*
 * setutmp - Update an entry in utmp and log an entry in wtmp
 *
 *	Return 1 on failure and 0 on success.
 */
int setutmp (struct utmp *ut)
{
	int err = 0;

	assert (NULL != ut);

	setutent ();
	if (pututline (ut) == NULL) {
		err = 1;
	}
	endutent ();

#ifndef USE_PAM
	/* This is done by pam_lastlog */
	updwtmp (_WTMP_FILE, ut);
#endif				/* ! USE_PAM */

	return err;
}
