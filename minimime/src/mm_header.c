/*
 * $Id: mm_header.c,v 1.6 2004/06/05 09:11:56 jfi Exp $
 *
 * MiniMIME - a library for handling MIME messages
 *
 * Copyright (C) 2003 Jann Fischer <rezine@mistrust.net>
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "mm_internal.h"
#include "mm_util.h"

/** @file mm_header.c
 *
 * This module contains functions for manipulating MIME headers
 */

/**
 * Creates a new MIME header object
 *
 * @return A new and initialized MIME header object
 * @see mm_mimeheader_free
 *
 * This function creates and initializes a new MIME header object, which must
 * later be freed using mm_mimeheader_free()
 */
struct mm_mimeheader *
mm_mimeheader_new(void)
{
	struct mm_mimeheader *header;

	header = (struct mm_mimeheader *)xmalloc(sizeof(struct mm_mimeheader));
	
	header->name = NULL;
	header->value = NULL;

	return header;
}

/**
 * Frees a MIME header object
 *
 * @param header The MIME header object which to free
 */
void
mm_mimeheader_free(struct mm_mimeheader *header)
{
	assert(header != NULL);

	if (header->name != NULL) {
		xfree(header->name);
		header->name = NULL;
	}
	if (header->value != NULL) {
		xfree(header->value);
		header->value = NULL;
	}

	xfree(header);
	header = NULL;
}

/**
 * Creates a new MIME header, but does no checks whatsoever (create as-is)
 */
struct mm_mimeheader *
mm_mimeheader_generate(const char *name, const char *value)
{
	struct mm_mimeheader *header;

	header = mm_mimeheader_new();

	header->name = xstrdup(name);
	header->value = xstrdup(value);

	return header;
}

int
mm_mimeheader_uncomment(struct mm_mimeheader *header)
{
	char *new;

	assert(header != NULL);
	assert(header->name != NULL);
	assert(header->value != NULL);

	new = mm_uncomment(header->value);
	if (new == NULL)
		return -1;

	xfree(header->value);
	header->value = new;

	return 0;
}

int
mm_mimeheader_uncommentbyname(struct mm_mimepart *part, const char *name)
{
	struct mm_mimeheader *header;

	TAILQ_FOREACH(header, &part->headers, next) {
		if (!strcasecmp(header->name, name)) {
			return mm_mimeheader_uncomment(header);
		}
	}

	/* Not found */
	return -1;
}

int
mm_mimeheader_uncommentall(struct mm_mimepart *part)
{
	struct mm_mimeheader *header;
	int ret, r;

	ret = 0;

	TAILQ_FOREACH(header, &part->headers, next) {
		if ((r = mm_mimeheader_uncomment(header)) == -1) {
			ret = -1;
		}
	}

	return ret;
}
