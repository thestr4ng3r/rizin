/*	$OpenBSD: regexec.c,v 1.11 2005/08/05 13:03:00 espie Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)regexec.c	8.3 (Berkeley) 3/20/94
 */

/*
 * the outer shell of regexec()
 *
 * This file includes engine.c *twice*, after muchos fiddling with the
 * macros that code uses.  This lets the same code operate on two different
 * representations for state sets.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <rz_regex.h>

#include "utils.h"
#include "regex2.h"

/* macros for manipulating states, small version */
#define states1          long long /* for later use in regexec() decision */
#define states           states1
#define CLEAR(v)         ((v) = 0)
#define SET0(v, n)       ((v) &= ~((unsigned states)1 << (n)))
#define SET1(v, n)       ((v) |= (unsigned states)1 << (n))
#define ISSET(v, n)      (((v) & ((unsigned states)1 << (n))) != 0)
#define ASSIGN(d, s)     ((d) = (s))
#define EQ(a, b)         ((a) == (b))
#define STATEVARS        states dummy /* dummy version */
#define STATESETUP(m, n) /* nothing */
#define STATETEARDOWN(m) /* nothing */
#define SETUP(v)         ((v) = 0)
#define onestate         states
#define INIT(o, n)       ((o) = (unsigned states)1 << (n))
#define INC(o)           ((o) <<= 1)
#define ISSTATEIN(v, o)  (((v) & (o)) != 0)
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define FWD(dst, src, n)  ((dst) |= ((unsigned states)(src) & (here)) << (n))
#define BACK(dst, src, n) ((dst) |= ((unsigned states)(src) & (here)) >> (n))
#define ISSETBACK(v, n)   (((v) & ((unsigned states)here >> (n))) != 0)
/* function names */
#define SNAMES /* engine.c looks after details */

#include "engine.c"

/* now undo things */
#undef states
#undef CLEAR
#undef SET0
#undef SET1
#undef ISSET
#undef ASSIGN
#undef EQ
#undef STATEVARS
#undef STATESETUP
#undef STATETEARDOWN
#undef SETUP
#undef onestate
#undef INIT
#undef INC
#undef ISSTATEIN
#undef FWD
#undef BACK
#undef ISSETBACK
#undef SNAMES

/* macros for manipulating states, large version */
#define states       char *
#define CLEAR(v)     memset(v, 0, m->g->nstates)
#define SET0(v, n)   ((v)[n] = 0)
#define SET1(v, n)   ((v)[n] = 1)
#define ISSET(v, n)  ((v)[n])
#define ASSIGN(d, s) memcpy(d, s, m->g->nstates)
#define EQ(a, b)     (memcmp(a, b, m->g->nstates) == 0)
#define STATEVARS \
	states1 vn; \
	char *space
#define STATESETUP(m, nv) \
	{ \
		(m)->space = malloc((nv) * (m)->g->nstates); \
		if (!(m)->space) \
			return RZ_REGEX_ESPACE; \
		(m)->vn = 0; \
	}
#define STATETEARDOWN(m) \
	{ free((m)->space); }
#define SETUP(v)        ((v) = &m->space[m->vn++ * m->g->nstates])
#define onestate        states1
#define INIT(o, n)      ((o) = (n))
#define INC(o)          ((o)++)
#define ISSTATEIN(v, o) ((v)[o])
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define FWD(dst, src, n)  ((dst)[here + (n)] |= (src)[here])
#define BACK(dst, src, n) ((dst)[here - (n)] |= (src)[here])
#define ISSETBACK(v, n)   ((v)[here - (n)])
/* function names */
#define LNAMES /* flag */

#include "engine.c"

RZ_API bool rz_regex_check(const RzRegex *rr, const char *str) {
	return rz_regex_exec(rr, str, 0, NULL, rr->re_flags);
}
/*
 - regexec - interface for matching
 *
 * We put this here so we can exploit knowledge of the state representation
 * when choosing which matcher to call.  Also, by this point the matchers
 * have been prototyped.
 */
/* 0 success, RZ_REGEX_NOMATCH failure */
RZ_API int rz_regex_exec(const RzRegex *preg, const char *string, size_t nmatch,
	RzRegexMatch pmatch[], int eflags) {
	struct re_guts *g;
#ifdef REDEBUG
#define GOODFLAGS(f) (f)
#else
#define GOODFLAGS(f) ((f) & (RZ_REGEX_NOTBOL | RZ_REGEX_NOTEOL | RZ_REGEX_STARTEND | RZ_REGEX_LARGE))
#endif
	if (!preg || !string) {
		return RZ_REGEX_ASSERT;
	}

	g = preg->re_g;
	if (preg->re_magic != MAGIC1 || g->magic != MAGIC2) {
		return (RZ_REGEX_BADPAT);
	}
	if (g->iflags & BAD) { /* backstop for no-debug case */
		return (RZ_REGEX_BADPAT);
	}
	eflags = GOODFLAGS(eflags);
	if (g->nstates <= CHAR_BIT * sizeof(states1) && !(eflags & RZ_REGEX_LARGE)) {
		return (smatcher(g, (char *)string, nmatch, pmatch, eflags));
	} else {
		return (lmatcher(g, (char *)string, nmatch, pmatch, eflags));
	}
}
