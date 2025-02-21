/*
 * SPDX-FileCopyrightText: 1990       , Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "prototypes.h"

/*
 * valid_field - insure that a field contains all legal characters
 *
 * The supplied field is scanned for non-printable and other illegal
 * characters.
 *  + -1 is returned if an illegal character is present.
 *  +  1 is returned if no illegal characters are present, but the field
 *       contains a non-printable character.
 *  +  0 is returned otherwise.
 */
int valid_field (const char *field, const char *illegal)
{
	const char *cp;
	int err = 0;

	if (NULL == field) {
		return -1;
	}

	/* For each character of field, search if it appears in the list
	 * of illegal characters. */
	for (cp = field; '\0' != *cp; cp++) {
		if (strchr (illegal, *cp) != NULL) {
			err = -1;
			break;
		}
	}

	if (0 == err) {
		/* Search if there are some non-printable characters */
		for (cp = field; '\0' != *cp; cp++) {
			if (!isprint (*cp)) {
				err = 1;
				break;
			}
		}
	}

	return err;
}

/*
 * change_field - change a single field if a new value is given.
 *
 * prompt the user with the name of the field being changed and the
 * current value.
 */
void change_field (char *buf, size_t maxsize, const char *prompt)
{
	char newf[200];
	char *cp;

	if (maxsize > sizeof (newf)) {
		maxsize = sizeof (newf);
	}

	printf ("\t%s [%s]: ", prompt, buf);
	(void) fflush (stdout);
	if (fgets (newf, maxsize, stdin) != newf) {
		return;
	}

	cp = strchr (newf, '\n');
	if (NULL == cp) {
		return;
	}
	*cp = '\0';

	if ('\0' != newf[0]) {
		/*
		 * Remove leading and trailing whitespace.  This also
		 * makes it possible to change the field to empty, by
		 * entering a space.  --marekm
		 */

		while (--cp >= newf && isspace (*cp));
		cp++;
		*cp = '\0';

		cp = newf;
		while (('\0' != *cp) && isspace (*cp)) {
			cp++;
		}

		strlcpy (buf, cp, maxsize);
	}
}

