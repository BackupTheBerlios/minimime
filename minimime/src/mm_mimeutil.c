/*
 * $Id: mm_mimeutil.c,v 1.1 2004/06/08 09:53:46 jfi Exp $
 *
 * MiniMIME - a library for handling MIME messages
 *
 * Copyright (C) 2004 Jann Fischer <rezine@mistrust.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JANN FISCHER AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JANN FISCHER OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "mm_internal.h"

static const char boundary_charset[] = 
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.=";

/** @file mm_mimeutil.c
 *
 * This module contains various MIME related utility functions.
 */

/** @defgroup mimeutil MIME related utility functions */

/**
 * Generates an RFC 2822 conform date string
 *
 * @param timezone Whether to include timezone information
 * @returns A pointer to the actual date string
 * @note The pointer returned must be freed some time
 *
 * This function generates an RFC 2822 conform date string to use in message
 * headers. It allocates memory to hold the string and returns a pointer to
 * it. The generated date is in the format (example):
 *
 *	Thu, 25 December 2003 16:35:22 +0100 (CET)
 *
 * This function dynamically allocates memory and returns a pointer to it.
 * This memory should be released with free() once not needed anymore.
 */
char *
mm_mimeutil_gendate(void)
{
	time_t curtime;
	struct tm *curtm;
	char buf[50];
	
	curtime = time(NULL);
	curtm = localtime(&curtime);

	strftime(buf, sizeof buf, "%a, %d %b %G %T %z (%Z)", curtm);

	return xstrdup(buf);
}


char *
mm_mimeutil_genboundary(char *prefix, size_t length)
{
	char *buf;
	size_t total;
	size_t preflen;
	struct timeval curtm;
	int i;

	total = 0;
	buf = NULL;
	preflen = 0;

	gettimeofday(&curtm, NULL);
	srandom(curtm.tv_usec);
	
	if (prefix != NULL) {
		total = strlen(prefix);
		preflen = total;
	}

	total += length;

	buf = (char *) xmalloc(total);
	if (buf == NULL) {
		return(NULL);
	}

	*buf = '\0';

	if (prefix != NULL) {
		strlcat(buf, prefix, total);
	}

	for (i = 0; i < length; i++) {
		int pos;
		
		pos = random() % strlen(boundary_charset);
		buf[i + preflen] = boundary_charset[pos];
	}
	buf[total] = '\0';

	return buf;
}
