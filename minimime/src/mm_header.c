/*
 * $Id: mm_header.c,v 1.2 2004/06/01 02:52:40 jfi Exp $
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

/*
static const char *mm_headers_loose[] = {
	"From",
	"To",
	"Date",
	NULL,
};

static const char *mm_headers_strict[] = {
	"From",
	"To",
	"Date",
	"Subject",
	"MIME-Version",
	"Content-Type",
	NULL,
};
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
	header->opaque = NULL;

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
	if (header->opaque != NULL) {
		xfree(header->opaque);
		header->opaque = NULL;
	}	

	xfree(header);
	header = NULL;
}

/**
 * Appends a value to a MIME header object
 *
 * @param header The MIME header which to append to
 * @param value The value which to append
 * @return 0 on success or -1 on failure
 *
 * This function appends a value to a MIME header, for example in case of a
 * continuation. It takes care that MIME limits, such as the 998 chars/line,
 * are not stepped over.
 */
static int
mm_mimeheader_append(struct mm_mimeheader *header, char *value)
{
	size_t new_hdrlen, skipped;
	char *new, *append;

	assert(header != NULL);
	assert(value != NULL);

	append = value;
	skipped = 0;

	/*
	 * Skip all leading whitespaces, but remain one space.
	 */
	while (*append != '\0' && isspace(*append)) {
		append++;
		skipped++;
	}
	if (skipped) {
		append--;
		if (isspace(*append)) {
			*append = ' ';
		}
	}	
	
	new_hdrlen = strlen(header->value) + strlen(append) + 1;
	assert(new_hdrlen > 0);

	new = xrealloc(header->value, new_hdrlen);
	header->value = new;
	strlcat(header->value, append, new_hdrlen);

	return 0;
}

/**
 * Parses a string into a MIME header object
 *
 * @param string The string which to parse
 * @param flags The parser flags to use
 * @param last A pointer to a MIME header object for reeantrance
 * @return A new MIME header object on success or a NULL pointer on failure.
 *	Sets mm_errno in case of an error.
 *
 * This function parses a MIME header (as in RFC 2822) into a MIME header. 
 */
struct mm_mimeheader *
mm_mimeheader_parse(const char *string, int flags, struct mm_mimeheader **last)
{
	struct mm_mimeheader *header;
	char *buf, *orig;
	char error;

	assert(string != NULL);

	header = NULL;
	orig = NULL;
	error = 0;

	mm_errno = MM_ERROR_NONE;
	
	if ((flags & ~MM_PARSE_LOOSE) && strlen(string) > MM_MIME_LINELEN) {
		mm_errno = MM_ERROR_MIME;
		mm_error_setmsg("Header line too long");
		return NULL;
	}

	buf = xstrdup(string);
	orig = buf;

	/* Strip trailing CR's and LF's, we do not need them */
	if (strlen(buf) > 1) {
		STRIP_TRAILING(buf, "\r\n");
	}

	if (isspace(buf[0])) {
		assert(*last != NULL);
		mm_mimeheader_append(*last, buf);
	} else {
		char *name, *value;

		name = strsep(&buf, ":");
		if (name == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("Invalid header format");
			goto cleanup;
		}

		value = strsep(&buf, "");
		if (value == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("Invalid header format");
			goto cleanup;
		}

		/* Skip leading whitespaces in the value */
		while (*value != '\0' && isspace(*value))
			value++;
		if (*value == '\0') {
			mm_errno = MM_ERROR_MIME;
			goto cleanup;
		}
		
		header = mm_mimeheader_new();

		header->name = xstrdup(name);
		header->value = xstrdup(value);

		*last = header;
	}

cleanup:
	if (orig != NULL) {
		xfree(orig);
		orig = NULL;
	}

	if (mm_errno != MM_ERROR_NONE)
		return NULL;
	else
		return header;
}

struct mm_mimeheader *
mm_mimeheader_parsefmt(int flags, const char *fmt, ...)
{
	struct mm_mimeheader *header, *lheader;
	char result[MM_MIME_LINELEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(result, sizeof result, fmt, ap);
	va_end(ap);

	header = mm_mimeheader_parse(result, flags, &lheader);
	return header;
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

	SLIST_FOREACH(header, &part->headers, next) {
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

	SLIST_FOREACH(header, &part->headers, next) {
		if ((r = mm_mimeheader_uncomment(header)) == -1) {
			ret = -1;
		}
	}

	return ret;
}
