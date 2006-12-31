/* -*-c++-*-  vi: set ts=4 sw=4 :

  Copyright (C) 1987, 88, 89, 90, 91, 92, 93, 94, 95
  Free Software Foundation, Inc.
  (C) Copyright 2006-2007, vitki.net. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  $Date$
  $Revision$
  $Source$

  Getopt for GNU.
  Drop-in replacement.

*/

/* This tells Alpha OSF/1 not to define a getopt prototype in <stdio.h>.
   Ditto for AIX 3.2 and <stdlib.h>.  */
#ifndef _NO_PROTO
#define _NO_PROTO
#endif

#if !defined (__STDC__) || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */
#ifndef const
#define const
#endif
#endif

#include <stdio.h>
#include <string.h>

/* This needs to come after some library #include
   to get __GNU_LIBRARY__ defined.  */
#ifdef	__GNU_LIBRARY__
/* Don't include stdlib.h for non-GNU C libraries because some of them
   contain conflicting prototypes for getopt.  */
#include <stdlib.h>
#endif /* GNU C library.  */

/* This is for other GNU distributions with internationalized messages.
   The GNU C Library itself does not yet support such messages.  */
#if HAVE_LIBINTL_H
# include <libintl.h>
#else
# define gettext(msgid) (msgid)
#endif

/* This version of `getopt' appears to the caller like standard Unix `getopt'
   but it behaves differently for the user, since it allows the user
   to intersperse the options with the other arguments.

   As `getopt' works, it permutes the elements of ARGV so that,
   when it is done, all the options precede everything else.  Thus
   all application programs are extended to handle flexible argument order.

   Setting the environment variable POSIXLY_CORRECT disables permutation.
   Then the behavior is completely standard.

   GNU application programs can use a third alternative mode in which
   they can distinguish the relative order of options and other arguments.  */

#include <optikus/getopt.h>

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

char   *ooptarg = NULL;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns EOF, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `ooptind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

/* XXX 1003.2 says this must be 1 before any call.  */
int     ooptind = 0;

/* The next char to be scanned in the option-element
   in which the last option character we returned was found.
   This allows us to pick up the scan where we left off.

   If this is zero, or a null string, it means resume the scan
   by advancing to the next ARGV-element.  */

static char *nextchar;

/* Callers store zero here to inhibit the error message
   for unrecognized options.  */

int     oopterr = 1;

/* Set to an option character which was unrecognized.
   This must be initialized on some systems to avoid linking in the
   system's own getopt implementation.  */

int     ooptopt = '?';

/* Describe how to deal with options that follow non-option ARGV-elements.

   If the caller did not specify anything,
   the default is REQUIRE_ORDER if the environment variable
   POSIXLY_CORRECT is defined, PERMUTE otherwise.

   REQUIRE_ORDER means don't recognize them as options;
   stop option processing when the first non-option is seen.
   This is what Unix does.
   This mode of operation is selected by either setting the environment
   variable POSIXLY_CORRECT, or using `+' as the first character
   of the list of option characters.

   PERMUTE is the default.  We permute the contents of ARGV as we scan,
   so that eventually all the non-options are at the end.  This allows options
   to be given in any order, even with programs that were not written to
   expect this.

   RETURN_IN_ORDER is an option available to programs that were written
   to expect options and other ARGV-elements in any order and that care about
   the ordering of the two.  We describe each non-option ARGV-element
   as if it were the argument of an option with character code 1.
   Using `-' as the first character of the list of option characters
   selects this mode of operation.

   The special argument `--' forces an end of option-scanning regardless
   of the value of `ordering'.  In the case of RETURN_IN_ORDER, only
   `--' can cause `getopt' to return EOF with `ooptind' != ARGC.  */

static enum
{
	REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER
} ordering;

/* Value of POSIXLY_CORRECT environment variable.  */
static char *posixly_correct;

#ifdef	__GNU_LIBRARY__
/* We want to avoid inclusion of string.h with non-GNU libraries
   because there are many ways it can cause trouble.
   On some systems, it contains special magic macros that don't work
   in GCC.  */
#include <string.h>
#define	my_index	strchr
#else

/* Avoid depending on library functions or files
   whose names are inconsistent.  */

char   *getenv();

static char *
my_index(const char *str, int chr)
{
	while (*str) {
		if (*str == chr)
			return (char *) str;
		str++;
	}
	return 0;
}

/* If using GCC, we can safely declare strlen this way.
   If not using GCC, it is ok not to declare it.  */
#ifdef __GNUC__
/* Note that Motorola Delta 68k R3V7 comes with GCC but not stddef.h.
   That was relevant to code that was here before.  */
#if !defined (__STDC__) || !__STDC__
/* gcc with -traditional declares the built-in strlen to return int,
   and has done so at least since version 2.4.5. -- rms.  */
extern int strlen(const char *);
#endif /* not __STDC__ */
#endif /* __GNUC__ */

#endif /* not __GNU_LIBRARY__ */

/* Handle permutation of arguments.  */

/* Describe the part of ARGV that contains non-options that have
   been skipped.  `first_nonopt' is the index in ARGV of the first of them;
   `last_nonopt' is the index after the last of them.  */

static int first_nonopt;
static int last_nonopt;

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,ooptind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

static void
exchange(char **argv)
{
	int     bottom = first_nonopt;
	int     middle = last_nonopt;
	int     top = ooptind;
	char   *tem;

	/* Exchange the shorter segment with the far end of the longer segment.
	   That puts the shorter segment into the right place.
	   It leaves the longer segment in the right place overall,
	   but it consists of two parts that need to be swapped next.  */

	while (top > middle && middle > bottom) {
		if (top - middle > middle - bottom) {
			/* Bottom segment is the short one.  */
			int     len = middle - bottom;
			register int i;

			/* Swap it with the top part of the top segment.  */
			for (i = 0; i < len; i++) {
				tem = argv[bottom + i];
				argv[bottom + i] = argv[top - (middle - bottom) + i];
				argv[top - (middle - bottom) + i] = tem;
			}
			/* Exclude the moved bottom segment from further swapping.  */
			top -= len;
		} else {
			/* Top segment is the short one.  */
			int     len = top - middle;
			register int i;

			/* Swap it with the bottom part of the bottom segment.  */
			for (i = 0; i < len; i++) {
				tem = argv[bottom + i];
				argv[bottom + i] = argv[middle + i];
				argv[middle + i] = tem;
			}
			/* Exclude the moved top segment from further swapping.  */
			bottom += len;
		}
	}

	/* Update records for the slots the non-options now occupy.  */

	first_nonopt += (ooptind - last_nonopt);
	last_nonopt = ooptind;
}


/* Initialize the internal data when the first call is made.  */

static const char *
_getopt_initialize(const char *optstring)
{
	/* Start processing options with ARGV-element 1 (since ARGV-element 0
	   is the program name); the sequence of previously skipped
	   non-option ARGV-elements is empty.  */

	first_nonopt = last_nonopt = ooptind = 1;

	nextchar = NULL;

	posixly_correct = getenv("POSIXLY_CORRECT");

	/* Determine how to handle the ordering of options and nonoptions.  */

	if (optstring[0] == '-') {
		ordering = RETURN_IN_ORDER;
		++optstring;
	} else if (optstring[0] == '+') {
		ordering = REQUIRE_ORDER;
		++optstring;
	} else if (posixly_correct != NULL)
		ordering = REQUIRE_ORDER;
	else
		ordering = PERMUTE;

	return optstring;
}


/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `getopt'
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If `getopt' finds another option character, it returns that character,
   updating `ooptind' and `nextchar' so that the next call to `getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `getopt' returns `EOF'.
   Then `ooptind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?' after printing an error message.  If you set `oopterr' to
   zero, the error message is suppressed but we still return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `ooptarg'.  Two colons mean an option that
   wants an optional arg; if there is text in the current ARGV-element,
   it is returned in `ooptarg', otherwise `ooptarg' is set to zero.

   If OPTSTRING starts with `-' or `+', it requests different methods of
   handling the non-option ARGV-elements.
   See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.

   Long-named options begin with `--' instead of `-'.
   Their names may be abbreviated as long as the abbreviation is unique
   or is an exact match for some defined option.  If they have an
   argument, it follows the option name in the same ARGV-element, separated
   from the option name by a `=', or else the in next ARGV-element.
   When `getopt' finds a long-named option, it returns 0 if that option's
   `flag' field is nonzero, the value of the option's `val' field
   if the `flag' field is zero.

   The elements of ARGV aren't really const, because we permute them.
   But we pretend they're const in the prototype to be compatible
   with other systems.

   LONGOPTS is a vector of `struct option' terminated by an
   element containing a name which is zero.

   LONGIND returns the index in LONGOPT of the long-named option found.
   It is only valid when a long-named option has been found by the most
   recent call.

   If LONG_ONLY is nonzero, '-' as well as '--' can introduce
   long-named options.  */

int
_ogetopt_internal(int argc, char *const *argv, const char *optstring,
				  const struct ooption *longopts, int *longind, int long_only)
{
	ooptarg = NULL;

	if (ooptind == 0) {
		optstring = _getopt_initialize(optstring);
		ooptind = 1;			/* Don't scan ARGV[0], the program name.  */
	}

	if (nextchar == NULL || *nextchar == '\0') {
		/* Advance to the next ARGV-element.  */

		if (ordering == PERMUTE) {
			/* If we have just processed some options following some non-options,
			   exchange them so that the options come first.  */

			if (first_nonopt != last_nonopt && last_nonopt != ooptind)
				exchange((char **) argv);
			else if (last_nonopt != ooptind)
				first_nonopt = ooptind;

			/* Skip any additional non-options
			   and extend the range of non-options previously skipped.  */

			while (ooptind < argc
				   && (argv[ooptind][0] != '-' || argv[ooptind][1] == '\0'))
				ooptind++;
			last_nonopt = ooptind;
		}

		/* The special ARGV-element `--' means premature end of options.
		   Skip it like a null option,
		   then exchange with previous non-options as if it were an option,
		   then skip everything else like a non-option.  */

		if (ooptind != argc && !strcmp(argv[ooptind], "--")) {
			ooptind++;

			if (first_nonopt != last_nonopt && last_nonopt != ooptind)
				exchange((char **) argv);
			else if (first_nonopt == last_nonopt)
				first_nonopt = ooptind;
			last_nonopt = argc;

			ooptind = argc;
		}

		/* If we have done all the ARGV-elements, stop the scan
		   and back over any non-options that we skipped and permuted.  */

		if (ooptind == argc) {
			/* Set the next-arg-index to point at the non-options
			   that we previously skipped, so the caller will digest them.  */
			if (first_nonopt != last_nonopt)
				ooptind = first_nonopt;
			return EOF;
		}

		/* If we have come to a non-option and did not permute it,
		   either stop the scan or describe it to the caller and pass it by.  */

		if ((argv[ooptind][0] != '-' || argv[ooptind][1] == '\0')) {
			if (ordering == REQUIRE_ORDER)
				return EOF;
			ooptarg = argv[ooptind++];
			return 1;
		}

		/* We have found another option-ARGV-element.
		   Skip the initial punctuation.  */

		nextchar = (argv[ooptind] + 1
					+ (longopts != NULL && argv[ooptind][1] == '-'));
	}

	/* Decode the current option-ARGV-element.  */

	/* Check whether the ARGV-element is a long option.

	   If long_only and the ARGV-element has the form "-f", where f is
	   a valid short option, don't consider it an abbreviated form of
	   a long option that starts with f.  Otherwise there would be no
	   way to give the -f short option.

	   On the other hand, if there's a long option "fubar" and
	   the ARGV-element is "-fu", do consider that an abbreviation of
	   the long option, just like "--fu", and not "-f" with arg "u".

	   This distinction seems to be the most useful approach.  */

	if (longopts != NULL
		&& (argv[ooptind][1] == '-'
			|| (long_only && (argv[ooptind][2] ||
							  !my_index(optstring, argv[ooptind][1])))
		)
		) {
		char   *nameend;
		const struct ooption *p;
		const struct ooption *pfound = NULL;
		int     exact = 0;
		int     ambig = 0;
		int     indfound = 0;
		int     option_index;

		for (nameend = nextchar; *nameend && *nameend != '='; nameend++)
			/* Do nothing.  */ ;

		/* Test all long options for either exact match
		   or abbreviated matches.  */
		for (p = longopts, option_index = 0; p->name; p++, option_index++)
			if (!strncmp(p->name, nextchar, nameend - nextchar)) {
				if ((int) (nameend - nextchar) == (int) strlen(p->name)) {
					/* Exact match found.  */
					pfound = p;
					indfound = option_index;
					exact = 1;
					break;
				} else if (pfound == NULL) {
					/* First nonexact match found.  */
					pfound = p;
					indfound = option_index;
				} else
					/* Second or later nonexact match found.  */
					ambig = 1;
			}

		if (ambig && !exact) {
			if (oopterr)
				fprintf(stderr, gettext("%s: option `%s' is ambiguous\n"),
						argv[0], argv[ooptind]);
			nextchar += strlen(nextchar);
			ooptind++;
			return '?';
		}

		if (pfound != NULL) {
			option_index = indfound;
			ooptind++;
			if (*nameend) {
				/* Don't test has_arg with >, because some C compilers don't
				   allow it to be used on enums.  */
				if (pfound->has_arg)
					ooptarg = nameend + 1;
				else {
					if (oopterr) {
						if (argv[ooptind - 1][1] == '-')
							/* --option */
							fprintf(stderr,
									gettext
									("%s: option `--%s' doesn't allow an argument\n"),
									argv[0], pfound->name);
						else
							/* +option or -option */
							fprintf(stderr,
									gettext
									("%s: option `%c%s' doesn't allow an argument\n"),
									argv[0], argv[ooptind - 1][0],
									pfound->name);

						nextchar += strlen(nextchar);
						return '?';
					}
				}
			} else if (pfound->has_arg == 1) {
				if (ooptind < argc)
					ooptarg = argv[ooptind++];
				else {
					if (oopterr)
						fprintf(stderr,
								gettext
								("%s: option `%s' requires an argument\n"),
								argv[0], argv[ooptind - 1]);
					nextchar += strlen(nextchar);
					return optstring[0] == ':' ? ':' : '?';
				}
			}
			nextchar += strlen(nextchar);
			if (longind != NULL)
				*longind = option_index;
			if (pfound->flag) {
				*(pfound->flag) = pfound->val;
				return 0;
			}
			return pfound->val;
		}

		/* Can't find it as a long option.  If this is not getopt_long_only,
		   or the option starts with '--' or is not a valid short
		   option, then it's an error.
		   Otherwise interpret it as a short option.  */
		if (!long_only || argv[ooptind][1] == '-'
			|| my_index(optstring, *nextchar) == NULL) {
			if (oopterr) {
				if (argv[ooptind][1] == '-')
					/* --option */
					fprintf(stderr, gettext("%s: unrecognized option `--%s'\n"),
							argv[0], nextchar);
				else
					/* +option or -option */
					fprintf(stderr, gettext("%s: unrecognized option `%c%s'\n"),
							argv[0], argv[ooptind][0], nextchar);
			}
			nextchar = (char *) "";
			ooptind++;
			return '?';
		}
	}

	/* Look at and handle the next short option-character.  */

	{
		char    c = *nextchar++;
		char   *temp = my_index(optstring, c);

		/* Increment `ooptind' when we start to process its last character.  */
		if (*nextchar == '\0')
			++ooptind;

		if (temp == NULL || c == ':') {
			if (oopterr) {
				if (posixly_correct)
					/* 1003.2 specifies the format of this message.  */
					fprintf(stderr, gettext("%s: illegal option -- %c\n"),
							argv[0], c);
				else
					fprintf(stderr, gettext("%s: invalid option -- %c\n"),
							argv[0], c);
			}
			ooptopt = c;
			return '?';
		}
		if (temp[1] == ':') {
			if (temp[2] == ':') {
				/* This is an option that accepts an argument optionally.  */
				if (*nextchar != '\0') {
					ooptarg = nextchar;
					ooptind++;
				} else
					ooptarg = NULL;
				nextchar = NULL;
			} else {
				/* This is an option that requires an argument.  */
				if (*nextchar != '\0') {
					ooptarg = nextchar;
					/* If we end this ARGV-element by taking the rest as an arg,
					   we must advance to the next element now.  */
					ooptind++;
				} else if (ooptind == argc) {
					if (oopterr) {
						/* 1003.2 specifies the format of this message.  */
						fprintf(stderr,
								gettext
								("%s: option requires an argument -- %c\n"),
								argv[0], c);
					}
					ooptopt = c;
					if (optstring[0] == ':')
						c = ':';
					else
						c = '?';
				} else
					/* We already incremented `ooptind' once;
					   increment it again when taking next ARGV-elt as argument.  */
					ooptarg = argv[ooptind++];
				nextchar = NULL;
			}
		}
		return c;
	}
}


int
ogetopt(int argc, char *const *argv, const char *optstring)
{
	return _ogetopt_internal(argc, argv, optstring,
							 (const struct ooption *) 0, (int *) 0, 0);
}


int
ogetopt_long(int argc, char *const *argv, const char *options,
			 const struct ooption *long_options, int *opt_index)
{
	return _ogetopt_internal(argc, argv, options, long_options, opt_index, 0);
}


/* Like getopt_long, but '-' as well as '--' can indicate a long option.
   If an option that starts with '-' (not '--') doesn't match a long option,
   but does match a short option, it is parsed as a short option
   instead.  */
int
ogetopt_long_only(int argc, char *const *argv, const char *options,
				  const struct ooption *long_options, int *opt_index)
{
	return _ogetopt_internal(argc, argv, options, long_options, opt_index, 1);
}
