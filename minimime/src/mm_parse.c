/*
 * $Id: mm_parse.c,v 1.1 2004/05/03 22:05:58 jfi Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "mm_internal.h"
#include "mm_util.h"

#ifdef MM_DEBUG
#define DPRINTF(a, ...) printf(a, ## __VA_ARGS__)
#else
#define DPRINTF(a, ...) {}
#endif

/** @file mm_parse.c
 *
 * Functions to parse data into MiniMIME contexts
 */

/**
 * Parses data into a MIME part object.
 *
 * @param data A pointer to the data to parse
 * @param flags The parser flags
 * @return A pointer to MIME part object on success or a NULL pointer on
 *	failure.
 * @note Will set mm_errno on failure
 *
 * This function parses a MIME entity, which must only consist of headers and
 * the message body, seperated by a CR/LF sequence. It must not contain any
 * stray data.
 */
struct mm_mimepart *
mm_parse_mimepart(char **data, int flags, int stripcr)
{
	struct mm_mimepart *part;
	struct mm_mimeheader *lheader;
	struct mm_content *ct;
	char *token, *body, *odata;

	mm_errno 	= MM_ERROR_NONE;
	part 		= NULL;
	lheader 	= NULL;
	body 		= NULL;
	token 		= NULL;
	ct 		= NULL;

	part = mm_mimepart_new();

	part->copy = xstrdup(*data);

	odata = *data;

	/* Fetch and parse MIME headers. Attach them to the current MIME part,
	 * too.
	 */
	while ((token = strsep(&odata, "\n")) != NULL) {
		struct mm_mimeheader *header;
		
		if (stripcr)
			mm_striptrailing(&token, "\r\n");
		else
			mm_striptrailing(&token, "\n");

		if (*token == '\0' || !strlen(token)) {
			break;
		} 

		header = mm_mimeheader_parse(token, flags, &lheader);
		if (header == NULL) {
			if (mm_errno != MM_ERROR_NONE) {
				mm_error_setmsg("Invalid header %s", token);
				goto cleanup;
			} else {
				DPRINTF("Continuation for '%s: %s'\n",
				    lheader->name, lheader->value);
			}	
		} else {
			DPRINTF("Header: %s -> %s\n", header->name, 
			    header->value);
			mm_mimepart_attachheader(part, header);
		}
	}

	/* Save the message body */
	if (odata != NULL) {
		part->body = xstrdup(odata);
		part->length = strlen(odata);
	} else {
		part->body = "";
		part->length = 0;
	}
	
	*data = odata;

	/* Depending on whether we found a Content-Type header and according
	 * to the parse flags, a missing header will trigger a MIME error. In
	 * case we parse loosely, we add such header by ourself (since it is
	 * required by the MIME standard). Only in MM_PARSE_FASCIST mode, a
	 * missing Content-Type header will trigger an error, since RFC 2045
	 * says that a missing Content-Type header can be replaced by a
	 * default one (see below).
	 *
	 * mm_content_parse() will set mm_errno by itself if an error
	 * occured.
	 */
	lheader = mm_mimepart_getheaderbyname(part, "Content-Type");
	if (lheader == NULL) {
		if (flags & MM_PARSE_FASCIST) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("No Content-Type header and parsing is"
			    " set to fascist");
			goto cleanup;
		}

		/* This is the default Content-Type that we set if none was
		 * found. This is the recommendation in RFC 2045 (see
		 * section 5.2). I don't wheter we should do this for multi-
		 * part entities, too, but for now, we do.
		 *
		 * XXX: do we want to attach the content type?
		 */
		ct = mm_content_parse("plain/text; charset=\"US-ASCII\"", flags);
	} else {
		ct = mm_content_parse(lheader->value, flags);
	}

	/* Trigger an error if we don't have a Content-Type object. */
	if (ct == NULL) {
		DPRINTF("No Content-Type found\n");
		goto cleanup;
	}

	/* According to RFC 2045, the only Content-Transfer-Encoding values
	 * valid for composite media types (multipart or message main types)
	 * are:
	 *
	 *	7bit
	 *	8bit
	 *	binary
	 *
	 * Everything else if forbidden, and we don't tolerate it even when
	 * parsing is set to MM_PARSE_LOOSE.
	 */
	lheader = mm_mimepart_getheaderbyname(part, "Content-Transfer" \
	    "-Encoding");
	if (lheader != NULL) {
		if (mm_content_iscomposite(ct) && 
		    !mm_content_isvalidencoding(lheader->value)) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("Invalid encoding for composite media"
			    " type: %s -> %s", ct->maintype, lheader->value);
			goto cleanup;
		}
		mm_content_setencoding(ct, lheader->value);
	}
	
	part->type = ct;
	
cleanup:
	if (mm_errno != MM_ERROR_NONE) {
		if (part != NULL) {
			mm_mimepart_free(part);
			part = NULL;
		}
		return NULL;
	} else {
		return part;
	}

}

/**
 * Parses a MIME message which is stored in memory
 *
 * @param ctx A valid MiniMIME context
 * @param data A pointer to the memory where the MIME message is located
 * @param flags The flags to pass to the parser
 * @return 0 on success or -1 on failure. 
 * @note Sets mm_errno on failure
 *
 * This function parses a MIME message into the given MiniMIME context.
 * The MIME message must be located in memory, and must be null terminated.
 * If the parser encounters an error, it will set mm_errno and mm_error_data
 * accordingly, providing the programmer with a meaningful error description.
 *
 * The flags parameter can have one of the following values:
 *	- <b>MM_PARSE_LOOSE</b>: The parser will ignore most MIME 
 *	abnormalities from broken implementations (such as messages without
 *	Content-Type or Date header fields).
 *
 * By default, the parsing is set to <b>MM_PARSE_STRICT</b>, which means
 * that an error is triggered when detecting a broken MIME format.
 *
 */
int
mm_parse_mem(MM_CTX *ctx, const char *data, int flags, int stripcr)
{
	char *buf, *orig, *token;
	char *boundary, *pboundary;
	struct mm_mimepart *part;
	struct mm_content *ct;
	struct mm_mimeheader *header;
	int lineno;

	assert(ctx != NULL);
	assert(data != NULL);

	lineno = 1;
	mm_errno = MM_ERROR_NONE;
	
	buf = xstrdup(data);
	orig = buf;
	boundary = NULL;
	pboundary = NULL;
	part = NULL;

	/* First, we'll parse the envelope */
	token = buf;
	part = mm_parse_mimepart(&token, flags, stripcr);
	if (part == NULL) {
		goto cleanup;
	}
	buf = token;

	mm_context_attachpart(ctx, part);
	
	ct = part->type;

	/* Set the type of the message according to what we parsed out of the
	 * Content-Type header.
	 *
	 * TODO: replace through a function which looks up the media type and
	 * sets the type accordingly.
	 */
	if (!strcasecmp(ct->maintype, "multipart") || 
	    !strcasecmp(ct->maintype, "message")) {
		ctx->messagetype = MM_MSGTYPE_MULTIPART;
	} else {
		ctx->messagetype = MM_MSGTYPE_FLAT;
	}
	part->type = ct;

	/* In case we have a flat message, no further processing needs to be
	 * done.
	 */
	if (ctx->messagetype == MM_MSGTYPE_FLAT)
		goto cleanup;

	/* In case of a multipart message, it MUST contain a MIME-Version
	 * header. The value MUST be "1.0" (This applies only to the MIME
	 * envelope). If MM_PARSE_LOOSE is set, we ignore this error.
	 */
	header = mm_mimepart_getheaderbyname(part, "MIME-Version");
	if (header == NULL) {
		if ((flags & MM_PARSE_LOOSE) == 0) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("No MIME-Version header in message");
			goto cleanup;
		} else {
			mm_warning_add(ctx, 0, "No MIME-Version header found"
			    " in envelope, but loose parsing was requested.");
		}
	} else {
		char *value, *orig;
		value = mm_uncomment(header->value);
		if (value == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("Could not uncomment MIME-Version");
			goto cleanup;
		}
		orig = value;
		while (*value != '\0') {
			if (isspace(*value)) {
				value++;
			} else {
				break;
			}
		}
		STRIP_TRAILING(value, "\t ");
		if (strcmp(value, "1.0")) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("Invalid MIME version: %s", value);
			xfree(orig);
			goto cleanup;
		}
		xfree(orig);
	}

	DPRINTF("Getting boundary\n");
	boundary = mm_content_getparambyname(ct, "boundary");
	if (boundary == NULL) {
		mm_errno = MM_ERROR_MIME;
		mm_error_setmsg("No boundary for multipart Content-Type found");
		goto cleanup;
	}

	/* Create a boundary string */
	pboundary = xmalloc(strlen(boundary) + 3);
	snprintf(pboundary, strlen(boundary) + 3, "--%s", boundary);

	/* Advance message pointer to after first boundary. This is really the 
	 * the preamble. We should store it somewhere. It could contain some 
	 * useful data which could be needed some day.
	 */
	buf = strstr(buf, pboundary);
	if (buf == NULL) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("No starting boundary found");
		goto cleanup;
	}
	if (strlen(buf) > strlen(pboundary) + 3) {
		buf += strlen(pboundary) + 1;
		if (*buf == '\r') {
			buf++;
		}
		if (*buf == '\n') {
			buf++;
		}
	} else {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("Message too short, cannot parse");
		goto cleanup;
	}

	DPRINTF("Parsing MIME parts\n");
	/* Parse all MIME parts and attach them to our context */
	while ((token = xstrsep(&buf, pboundary)) != NULL) {
		char *mimepart;

		/* If there is a stray boundary, trigger an error */
		if (*token == '\0') {
			DPRINTF("STRAY BOUNDARY\n");
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("Found a stray boundary");
			goto cleanup;
		}

		/* Skip the leading CR/LF pair.
		 * XXX: Usually, there should only be one pair of CR/LF
		 * to skip, everything else should be considered deformed.
		 */
		if (*token && *token == '\r') {
			token++;
		}	
		if (*token && *token == '\r') {
			token++;
		}

		/* Now we have the raw MIME entity in front of us, which we
		 * safely can pass to mm_parse_mimepart().
		 */
		DPRINTF("PARSING:\n%s\nEND PARSING\n", token);
		mimepart = token;
		part = mm_parse_mimepart(&mimepart, flags, stripcr);
		if (part == NULL) {
			goto cleanup;
		}

		DPRINTF("BODY:\n%s\nENDBODY\n", part->body);
		
		mm_context_attachpart(ctx, part);

		/* If the next two chars are dashes, we know that we have
		 * the end boundary */  
		if (buf != NULL && !strncmp(buf, "--", 2)) {
			DPRINTF("END BOUNDARY: %s\n", buf);
			break;
		}
		buf++;

	}

	/* If buf is NULL, we didn't find the end boundary -- this means we
	 * processed an invalid MIME message */
	if (buf == NULL) {
		mm_errno = MM_ERROR_MIME;
		mm_error_setmsg("Invalid MIME message: No end boundary");
		goto cleanup;
	}

cleanup:
	if (orig != NULL) {
		xfree(orig);
		orig = NULL;
	}
	if (pboundary != NULL) {
		xfree(pboundary);
		pboundary = NULL;
	}
	if (mm_errno != MM_ERROR_NONE) {
		return -1;
	}

	return 0;
}

/**
 * Reads a MIME message from a file and parses it
 *
 * @param ctx A valid MiniMIME context
 * @param filename The name of the file to parse
 * @param flags The flags to pass to the parser
 * @note Sets mm_errno on failure
 */
int
mm_parse_file(MM_CTX *ctx, const char *filename, int flags, int stripcr)
{
	struct stat st;
	size_t rb;
	char *data;
	int fd, ret;

	mm_errno = MM_ERROR_NONE;

	if (stat(filename, &st) == -1) {
		mm_errno = MM_ERROR_ERRNO;
		return -1;
	}

	if ((fd = open(filename, O_RDONLY)) == -1) {
		mm_errno = MM_ERROR_ERRNO;
		return -1;
	}	

	data = (char *)xmalloc(st.st_size);
	rb = read(fd, data, st.st_size);
	
	if (rb != st.st_size) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("Input/ouput error");
		return -1;
	}

	close(fd);
	ret = mm_parse_mem(ctx, data, flags, stripcr);
	xfree(data);
	return ret;
}
