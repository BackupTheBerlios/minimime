/*
 * $Id: mm_context.c,v 1.4 2004/06/04 14:27:29 jfi Exp $
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
	ctx->messagetype = MM_MSGTYPE_FLAT; /* This is the default */
	ctx->boundary = NULL;

	TAILQ_INIT(&ctx->parts);
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

	TAILQ_FOREACH(part, &ctx->parts, next) {
		TAILQ_REMOVE(&ctx->parts, part, next);
		mm_mimepart_free(part);
	}

	if (ctx->boundary != NULL) {
		xfree(ctx->boundary);
		ctx->boundary = NULL;
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
	
	if (TAILQ_EMPTY(&ctx->parts)) {
		TAILQ_INSERT_HEAD(&ctx->parts, part, next);
	} else {
		TAILQ_INSERT_TAIL(&ctx->parts, part, next);
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

	TAILQ_FOREACH(part, &ctx->parts, next) {
		if (cur == which) {
			TAILQ_REMOVE(&ctx->parts, part, next);
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

	count = 0;

	if (TAILQ_EMPTY(&ctx->parts)) {
		return 0;
	} else {
		TAILQ_FOREACH(part, &ctx->parts, next) {
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
	
	TAILQ_FOREACH(part, &ctx->parts, next) {
		if (cur == which) {
			return part;
		}
		cur++;
	}

	return NULL;
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

	TAILQ_FOREACH(header, &part->headers, next) {
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

/**
 * Checks whether a given context represents a composite (multipart) message
 *
 * @param ctx A valid MiniMIME context object
 * @return 1 if the context is a composite message or 0 if it's flat
 *
 */
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

/**
 * Creates an ASCII message of the specified context
 *
 * @param ctx A valid MiniMIME context object
 * @param flat Where to store the message
 * @param opaque Whether the MIME parts should be included opaque
 *
 */
int
mm_context_flatten(MM_CTX *ctx, char **flat, int opaque)
{
	struct mm_mimepart *part;
	char *message;
	char *flatpart;
	size_t message_size;
	char envelope;

	mm_errno = MM_ERROR_NONE;
	envelope = 1;

	TAILQ_FOREACH(part, &ctx->parts, next) {
		if (envelope) {
			envelope = 0;
			continue;
		}	
		/*
		if (mm_mimepart_flatten(part, &flatpart, opaque) == -1) {
		}
		*/
		
		message_size += strlen(flatpart);
	}

}

/** @} */
