/*
 * $Id: mm_flatten.c,v 1.1 2004/05/03 22:05:58 jfi Exp $
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
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "mm_internal.h"

/**
 * Calculates how much memory is needed for a given MIME part when flattened
 *
 * @param part The MIME part for which to calculate the size
 * @return The number of bytes required
 */
static size_t
mm_flatten_memoryneeded(struct mm_mimepart *part)
{
	struct mm_mimeheader *header;
	size_t needed;
	
	assert(part != NULL);
	needed = 0;

	SLIST_FOREACH(header, &part->headers, next) {
		needed += strlen(header->name);
		needed += strlen(header->value);
		/* Colon, whitespace, CRLF and \0 for a header */
		needed += 5; 
	}

	/* CRLF for end of headers */
	needed += 2; 
	return needed;
}

/**
 * Flattens a MIME part object
 *
 * @param part The MIME part object to flatten
 * @return A pointer to the flattened represantation
 *
 * This function ``flattens'' a MIME part. It concatenatees the header with the
 * body and applies possible encodings.
 *
 * @todo Encoder/decoder implementation
 */
char *
mm_flatten_mimepart(struct mm_mimepart *part)
{
	struct mm_mimeheader *header;
	size_t size;
	char *flattened;

	assert(part != NULL);
	
	mm_errno = MM_ERROR_NONE;

	size = mm_flatten_memoryneeded(part);
	size += strlen(part->body) + 2;
	assert(size > 0);

	flattened = xmalloc(size);
	*flattened = '\0';

	if (!SLIST_EMPTY(&part->headers)) {
		SLIST_FOREACH(header, &part->headers, next) {
			strlcat(flattened, header->name, size);
			strlcat(flattened, ": ", size);
			strlcat(flattened, header->value, size);
			strlcat(flattened, "\r\n", size);
		}
	}

	strlcat(flattened, "\r\n", size);
	strlcat(flattened, part->body, size);

	return flattened;
}

char *
mm_flatten_context(MM_CTX *ctx)
{
	char *result;
	struct mm_mimepart *part;
	size_t size;
	u_int16_t part_id;
	
	assert(ctx != NULL);
	
	result = NULL;
	size = 0;
	part_id = 0;

	if (SLIST_EMPTY(&ctx->parts)) {
		return NULL;
	}

	SLIST_FOREACH(part, &ctx->parts, next) {
		size_t partsize;

		partsize = mm_flatten_memoryneeded(part);
		/* An empty part is not acceptable */
		if (partsize < 1) {
			return NULL;
		}
		size += partsize;
	}
	
	return NULL;
}
