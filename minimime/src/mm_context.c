/*
 * $Id: mm_context.c,v 1.1 2004/05/03 22:05:58 jfi Exp $
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
#include <assert.h>

#include "mm_internal.h"

/** @file mm_context.c
 *
 * Modules for manipulating MiniMIME contexts
 */

struct mm_required_headerfields
{
	int gotit;
	const char *name;
};

static struct mm_required_headerfields required_headers[] = {
	{
		0,
		"From"
	},
	{
		0,
		"To"
	},
	{
		0,
		"Date"
	},
	{
		0,
		NULL
	}
};

/** 
 * @{
 * @name Manipulating MiniMIME contexts
 */

/**
 * Creates a new MiniMIME context object. This object must later be freed
 * using the mm_context_free() function.
 *
 * @return a new MiniMIME context object
 * @ref mm_context_free
 */
MM_CTX *
mm_context_new(void)
{
	MM_CTX *ctx;

	MM_ISINIT();

	ctx = (MM_CTX *)xmalloc(sizeof(MM_CTX));
	ctx->initialized = 1;
	ctx->finalized = 0;
	ctx->messagetype = MM_MSGTYPE_FLAT; /* This is the default */

	SLIST_INIT(&ctx->parts);
	SLIST_INIT(&ctx->warnings);

	return ctx;
}

/**
 * Releases all memory associated with MiniMIME context object that was
 * created using mm_context_new(). 
 *
 * @param ctx the MiniMIME context object to free
 * @ref mm_context_new
 */
void
mm_context_free(MM_CTX *ctx)
{
	struct mm_mimepart *part;
	struct mm_warning *warning;
	
	assert(ctx != NULL);
	ctx->initialized = 0;

	SLIST_FOREACH(part, &ctx->parts, next) {
		SLIST_REMOVE(&ctx->parts, part, mm_mimepart, next);
		mm_mimepart_free(part);
	}

	SLIST_FOREACH(warning, &ctx->warnings, next) {
		SLIST_REMOVE(&ctx->warnings, warning, mm_warning, next);
		if (warning->message != NULL) {
			xfree(warning->message);
			warning->message = NULL;
		}
		xfree(warning);
	}

	xfree(ctx);
	ctx = NULL;
}

/**
 * Attaches a MIME part object to a MiniMIME context.
 *
 * @param ctx the MiniMIME context
 * @param part the MIME part object to attach
 * @return 0 on success or -1 on failure. Sets mm_errno on failure.
 */
int
mm_context_attachpart(MM_CTX *ctx, struct mm_mimepart *part)
{
	assert(ctx != NULL);
	assert(part != NULL);
	
	if (SLIST_EMPTY(&ctx->parts)) {
		SLIST_INSERT_HEAD(&ctx->parts, part, next);
	} else {
		struct mm_mimepart *last, *tmp;
		SLIST_FOREACH(tmp, &ctx->parts, next)
			if (tmp != NULL)
				last = tmp;
		SLIST_INSERT_AFTER(last, part, next);
	}

	return 0;
}

/**
 * Deletes a MIME part object from a MiniMIME context
 *
 * @param ctx The MiniMIME context
 * @param which The number of the MIME part object to delete
 * @param freemem Whether to free the memory associated with the MIME part
 *        object
 * @return 0 on success or -1 on failure. Sets mm_errno on failure.
 */
int
mm_context_deletepart(MM_CTX *ctx, int which, int freemem)
{
	struct mm_mimepart *part;
	int cur;

	assert(ctx != NULL);
	assert(which >= 0);

	cur = 0;

	SLIST_FOREACH(part, &ctx->parts, next) {
		if (cur == which) {
			SLIST_REMOVE(&ctx->parts, part, mm_mimepart, next);
			if (freemem)
				mm_mimepart_free(part);
			return 0;
		}
		cur++;
	}

	return -1;
}

/**
 * Counts the number of attached MIME part objects in a given MiniMIME context
 *
 * @param ctx The MiniMIME context
 * @returns The number of attached MIME part objects
 */
int
mm_context_countparts(MM_CTX *ctx)
{
	int count;
	struct mm_mimepart *part;
	
	assert(ctx != NULL);
	assert(ctx->initialized == 1);

	count = 0;

	if (SLIST_EMPTY(&ctx->parts)) {
		return 0;
	} else {
		SLIST_FOREACH(part, &ctx->parts, next) {
			count++;
		}
	}

	assert(count > -1);

	return count;
}

/**
 * Gets a specified MIME part object from a MimeMIME context
 *
 * @param ctx The MiniMIME context
 * @param which The number of the MIME part object to retrieve
 * @returns The requested MIME part object on success or a NULL pointer if
 *          there is no such part.
 */
struct mm_mimepart *
mm_context_getpart(MM_CTX *ctx, int which)
{
	struct mm_mimepart *part;
	int cur;
	
	assert(ctx != NULL);

	cur = 0;
	
	SLIST_FOREACH(part, &ctx->parts, next) {
		if (cur == which) {
			return part;
		}
		cur++;
	}

	return NULL;
}

/**
 * Sets an envelope header field
 *
 * @param ctx A valid MiniMIME context
 * @param fmt A format string representing an RFC822 formatted header
 * @return 0 if the header was successfully set or -1 if there was an error
 * @note Sets mm_errno on error
 *
 * This function takes the format string in fmt, parses it into a MIME header
 * and sets it in the envelope for the MiniMIME context ctx. If the header
 * field already exist, it will be replaced.
 */
int
mm_context_setheader(MM_CTX *ctx, const char *fmt, ...)
{
	struct mm_mimepart *part;
	struct mm_mimeheader *header, *lheader, *pheader;
	va_list ap;
	char buf[MM_MIME_LINELEN];

	mm_errno = MM_ERROR_NONE;

	lheader = NULL;
	pheader = NULL;

	part = mm_context_getpart(ctx, 0);
	assert(part != NULL);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	header = mm_mimeheader_parse(buf, 0, &lheader);
	if (header == NULL) {
		return -1;
	}

	SLIST_FOREACH(lheader, &part->headers, next) {
		if (lheader != NULL) {
			assert(lheader->name != NULL);
			assert(lheader->value != NULL);
			if (!strcasecmp(header->name, lheader->name)) {
				xfree(lheader->name);
				lheader->name = xstrdup(header->name);
				mm_mimeheader_free(header);
				return 0;
			}
			pheader = lheader;
		} else {
			assert(0);
		}
	}

	SLIST_INSERT_AFTER(pheader, header, next);
	
	return 0;
}

/**
 * Adds a recipient to an envelope
 *
 * @param ctx A valid MiniMIME context
 * @param which Which destination field to use
 * @param address The address of the recipient
 * @param fullname The optional full name of the recipient
 * 
 * This functions adds a recipient to a message envelope. The destination field
 * where the recipient is added can be specified by setting the which parameter
 * to one of the following values:
 *
 *	- MM_FIELD_TO: The "To:" field
 *	- MM_FIELD_CC: The "Cc:" field
 *	- MM_FIELD_BCC: The "Bcc: field
 *	- MM_FIELD_FROM: The "From:" field
 *
 * The parameter address MUST be a valid pointer to a string which holds the
 * recipients e-mail address in the format "local@domain". The parameter
 * fullname is optional. If not set to NULL, the resulting recipient's
 * address will be formatted as in "\"fullname\" <local@domain>".
 */
int
mm_context_addaddress(MM_CTX *ctx, int which, const char *address, 
    const char *fullname)
{
	struct mm_mimepart *part;
	struct mm_mimeheader *header;
	char *recipient, *field;
	size_t recipient_len;
	
	assert(ctx != NULL);
	assert(address != NULL);

	/* Figure out which field to set
	 */
	switch (which) {
		case MM_ADDR_TO:
			field = "To";
			break;
		case MM_ADDR_CC:
			field = "Cc";
			break;
		case MM_ADDR_BCC:
			field = "Bcc";
			break;
		case MM_ADDR_FROM:
			field = "From";
		default:
			mm_errno = MM_ERROR_UNDEF;
			mm_error_setmsg("Invalid field specified");
			return -1;
	}

	part = mm_context_getpart(ctx, 0);
	if (part == NULL) {
		part = mm_mimepart_new();
		mm_context_attachpart(ctx, part);
	}

	recipient_len = strlen(address) + 1;
	if (fullname != NULL) {
		recipient_len += strlen(fullname) + 6;
	}

	recipient_len += 2;
	recipient = xmalloc(recipient_len);
	
	if (fullname != NULL) {
		snprintf(recipient, recipient_len, "\"%s\" <%s>; ", fullname,
		    address);
	} else {
		snprintf(recipient, recipient_len, "%s; ", address);
	}

	/* Get the corresponding header field from the envelope. If we found
	 * one existing, we append the new recipient to it and make sure it
	 * is correctly delimeted by a semicolon. Else, we'll create a new
	 * header object and attach it to the envelope.
	 */
	header = mm_mimepart_getheaderbyname(part, "To");
	if (header == NULL) {
		header = mm_mimeheader_parsefmt(0, "To: %s\n", recipient);
		mm_mimepart_attachheader(part, header);
	} else {
		size_t semicolon;
		
		semicolon = 0;
		
		if (header->value[strlen(header->value)-2] != ';' &&
		    header->value[strlen(header->value)-1] != ';') {
			semicolon = 2;
		}
		
		recipient_len += strlen(header->value) + semicolon;
		header->value = xrealloc(header->value, recipient_len);
		
		if (semicolon) {
			strlcat(header->value, "; ", recipient_len);
		}	
		
		strlcat(header->value, recipient, recipient_len);
	}

	return 0;
}

static void
mm_context_lookupheader(const char *name, struct mm_required_headerfields headers[])
{
	int i;

	for (i = 0; headers[i].name != NULL; i++) {
		if (!strcasecmp(name, headers[i].name)) {
			headers[i].gotit = 1;
			break;
		}
	}
}

/**
 * Checks whether a made up message conforms MIME standards 
 *
 * @param ctx A valid MiniMIME context
 * @return 0 if the message checked is compliant or -1 if not.
 * @note Sets mm_errno in case of an error
 *
 */
int
mm_context_finalize(MM_CTX *ctx)
{
	struct mm_mimeheader *header;
	struct mm_mimepart *part;
	int i;

	part = NULL;
	header = NULL;

	part = mm_context_getpart(ctx, 0);
	if (part == NULL) {
		mm_errno = MM_ERROR_PROGRAM;
		mm_error_setmsg("No such MIME part: 0");
		return -1;
	}

	/* TODO: this is a slow way to lookup the headers. make it faster */
	SLIST_FOREACH(header, &part->headers, next) {
		mm_context_lookupheader(header->name, required_headers);
	}

	for (i = 0; required_headers[i].name != NULL; i++) {
		if (required_headers[i].gotit == 0) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("Required header field missing: %s",
			    required_headers[i].name);
			goto cleanup;
		}
	}
	
cleanup:
	if (mm_errno != MM_ERROR_NONE) {
		return -1;
	} else {
		return 0;
	}
}

int
mm_context_iscomposite(MM_CTX *ctx)
{
	if (ctx->messagetype == MM_MSGTYPE_MULTIPART) {
		return 1;
	} else {
		return 0;
	}
}

int
mm_context_haswarnings(MM_CTX *ctx)
{
	if (SLIST_EMPTY(&ctx->warnings)) {
		return 0;
	} else {
		return 1;
	}
}

/** @} */
