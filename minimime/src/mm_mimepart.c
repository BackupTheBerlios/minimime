/*
 * $Id: mm_mimepart.c,v 1.3 2004/06/03 13:05:31 jfi Exp $
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
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of the contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#include "mm_internal.h"

/** @file mm_mimepart.c
 *
 * This module contains functions for manipulating MIME header objects.
 */

/** @defgroup mimepart Accessing and manipulating MIME parts 
 *
 * MIME parts, also called entities, represent the structure of a MIME
 * message. ``Normal'' internet messages have only a single part, and
 * are called ``flat'' messages. Multipart messages have more then one
 * part, and each MIME part can have it's own subset of headers.
 *
 * Provided here are functions to easily access all informations from
 * a MIME part, including their specific headers and bodies.
 */

/** @{ 
 * @name Creating and destroying MIME parts 
 */

/**
 * Allocates memory for a new mm_mimepart structure and initializes it.
 *
 * @return A pointer to a struct of type mm_mimeheader or NULL on failure
 * @see mm_mimepart_free
 * @note The memory must be freed by using mm_mimepart_free() later on.
 */
struct mm_mimepart *
mm_mimepart_new(void)
{
	struct mm_mimepart *part;

	part = (struct mm_mimepart *)xmalloc(sizeof(struct mm_mimepart));

	SLIST_INIT(&part->headers);

	part->opaque_length = 0;
	part->opaque_body = NULL;
	
	part->length = 0;
	part->body = NULL;
	
	part->type = NULL;

	part->disposition_type = NULL;
	part->filename = NULL;
	part->creation_date = NULL;
	part->modification_date = NULL;
	part->read_date = NULL;
	part->disposition_size = NULL;

	return part;
}

/**
 * Creates a MIME part from a file
 *
 * @param filename The name of the file to create the MIME part from
 * @return A pointer to a new MIME part object
 *
 * This function creates a new MIME part object from a file. The object should
 * be freed using mm_mimepart_free() later on. This function does NOT set the
 * Content-Type and neither does any encoding work.
 */
struct mm_mimepart *
mm_mimepart_fromfile(const char *filename)
{
	int fd;
	char *data;
	size_t r;
	struct stat st;
	struct mm_mimepart *part;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		mm_errno = MM_ERROR_ERRNO;
		return NULL;
	}

	if ((stat(filename, &st)) == -1) {
		mm_errno = MM_ERROR_ERRNO;
		close(fd);
		return NULL;
	}

	data = xmalloc(st.st_size);
	r = read(fd, data, st.st_size);
	if (r != st.st_size) {
		mm_errno = MM_ERROR_ERRNO;
		close(fd);
		return(NULL);
	}

	data[r] = '\0';
	close(fd);

	part = mm_mimepart_new();
	part->length = r;
	part->body = data;

	return part;
}


/**
 * Frees all memory allocated by a mm_mimepart object.
 *
 * @param part A pointer to an allocated mm_mimepart object
 * @see mm_mimepart_new
 */
void
mm_mimepart_free(struct mm_mimepart *part)
{
	struct mm_mimeheader *header;

	assert(part != NULL);

	SLIST_FOREACH(header, &part->headers, next) {
		mm_mimeheader_free(header);
		SLIST_REMOVE(&part->headers, header, mm_mimeheader, next);
	}

	if (part->opaque_body != NULL) {
		xfree(part->opaque_body);
		part->opaque_body = NULL;
		part->body = NULL;
	} else if (part->body != NULL) {
		xfree(part->body);
		part->body = NULL;
	}

	if (part->type != NULL) {
		mm_content_free(part->type);
		part->type = NULL;
	}

	if (part->disposition_type != NULL) {
		xfree(part->disposition_type);
		part->disposition_type = NULL;
	}	
	if (part->filename != NULL) {
		xfree(part->filename );
		part->filename = NULL;
	}	
	if (part->creation_date != NULL) {
		xfree(part->creation_date );
		part->creation_date = NULL;
	}
	if (part->modification_date != NULL) {
		xfree(part->modification_date );
		part->modification_date = NULL;
	}
	if (part->read_date != NULL) {
		xfree(part->read_date );
		part->read_date = NULL;
	}
	if (part->disposition_size != NULL) {
		xfree(part->disposition_size);
		part->read_date = NULL;
	}

	xfree(part);
	part = NULL;
}

/** @} */

/** @{ 
 * @name Accessing the MIME part's mail header
 */

/**
 * Attaches a mm_mimeheader object to a MIME part
 *
 * @param part A valid MIME part object
 * @param header A valid MIME header object
 * @return 0 if successfull or -1 if the header could not be attached
 */
int
mm_mimepart_attachheader(struct mm_mimepart *part, struct mm_mimeheader *header)
{
	struct mm_mimeheader *theader, *lheader;

	assert(part != NULL);
	assert(header != NULL);

	if (SLIST_EMPTY(&part->headers)) {
		SLIST_INSERT_HEAD(&part->headers, header, next);
	} else {
		SLIST_FOREACH(theader, &part->headers, next) 
			if (theader != NULL)
				lheader = theader;
		SLIST_INSERT_AFTER(lheader, header, next);
	}

	return(0);
}

/**
 * Retrieves the number of MIME headers available in a MIME part
 *
 * @param part A valid MIME part object
 * @return The number of MIME headers within the MIME part
 */
int
mm_mimepart_countheaders(struct mm_mimepart *part)
{
	int found;
	struct mm_mimeheader *header;

	assert(part != NULL);

	found = 0;

	SLIST_FOREACH(header, &part->headers, next) {
		found++;
	}

	return found;
}

/**
 * Retrieves the number of MIME headers with a given name in a MIME part
 *
 * @param part A valid MIME part object
 * @param name The name of the MIME header which to count for
 * @return The number of MIME headers within the MIME part
 */
int
mm_mimepart_countheaderbyname(struct mm_mimepart *part, const char *name)
{
	int found;
	struct mm_mimeheader *header;

	assert(part != NULL);

	found = 0;

	SLIST_FOREACH(header, &part->headers, next) {
		if (strcasecmp(header->name, name) == 0) {
			found++;
		}
	}

	return found;
}

/**
 * Get a MIME header object from a MIME part
 *
 * @param part A valid MIME part object
 * @param name The name of the MIME header which to retrieve
 * @param idx Which header field to get (in case of multiple headers of the
 *	      same name).
 * @return A pointer to the requested MIME header on success, or NULL if there
 *         either isn't a header with the requested name or idx is out of 
 *         range.
 */
struct mm_mimeheader *
mm_mimepart_getheaderbyname(struct mm_mimepart *part, const char *name, int idx)
{
	struct mm_mimeheader *header;
	int curidx;

	curidx = 0;

	SLIST_FOREACH(header, &part->headers, next) {
		if (!strcasecmp(header->name, name)) {
			if (curidx == idx)
				return header;
			else
				curidx++;
		}
	}

	/* Not found */
	return NULL;
}

/**
 * Gets the value of a MIME header object
 *
 * @param part A valid MIME part object
 * @param name The name of the header field to get the value from
 * @param idx The index of the header field to get, in case there are multiple
 *            headers with the same name.
 * @return A pointer to the requested value on success, or NULL if there either
 *         isn't a header with the requested name or idx is out of range.
 *
 */
const char *
mm_mimepart_getheadervalue(struct mm_mimepart *part, const char *name, int idx)
{
	struct mm_mimeheader *header;

	header = mm_mimepart_getheaderbyname(part, name, idx);
	if (header == NULL)
		return NULL;
	else
		return header->value;
}

/**
 * Initializes a header loop for a given MIME part
 *
 * @param part A valid MIME part object
 * @param id The address of a MIME header object (to allow reentrance)
 * @return 0 on success or -1 on failure
 * @see mm_mimepart_headers_next
 * 
 * Looping through headers can be done in the following way:
 *
 * @code
 * struct mm_mimeheader *header, *lheader;
 *
 * mm_mimepart_headers_start(part, &lheader);
 *
 * while ((header = mm_mimepart_headers_next(part, &lheader)) != NULL) {
 *	printf("%s: %s\n", header->name, header->value);	
 * }
 *
 * @endcode
 *
 * For convienience, the macro mm_mimepart_headers_foreach() can be used to
 * loop through headers in a one-shot manner.
 */
int
mm_mimepart_headers_start(struct mm_mimepart *part, struct mm_mimeheader **id)
{
	assert(part != NULL);
	
	if (SLIST_EMPTY(&part->headers)) {
		return -1;
	}
	*id = NULL;
	return 0;
}

/**
 * Returns the next MIME header of a given MIME part object
 *
 * @param part A valid MIME part object
 * @param id A previously initialized MIME header object
 * @return A pointer to the MIME header object or NULL if end of headers was
 * 	reached.
 * @see mm_mimepart_headers_start
 */
struct mm_mimeheader *
mm_mimepart_headers_next(struct mm_mimepart *part, struct mm_mimeheader **id)
{
	struct mm_mimeheader *header;

	assert(part != NULL);

	if (*id == NULL) {
		header = SLIST_FIRST(&part->headers);
	} else {
		header = SLIST_NEXT(*id, next);
	}
	*id = header;

	return header;
}

/** @} */

/** @{
 * @name Accessing and manipulating the MIME part's body
 */

/**
 * Gets the pointer to the MIME part's body data
 *
 * @param part A valid MIME part object
 * @param opaque Whether to get the opaque part or not
 * @return A pointer to the MIME part's body
 * @see mm_mimepart_setbody
 *
 */
char *
mm_mimepart_getbody(struct mm_mimepart *part, int opaque)
{
	assert(part != NULL);

	if (opaque)
		return part->opaque_body;
	else	
		return part->body;
}

/**
 * Sets the MIME part's body data
 *
 * @param part A valid MIME part object
 * @param data A pointer to the data which to set
 * @see mm_mimepart_getbody
 *
 * This functions sets the body data for a given MIME part. The string pointed
 * to by data must be NUL-terminated. The data is copied into the MIME part's
 * body, and thus, the memory pointed to by data can be freed after the
 * operation. 
 */
void
mm_mimepart_setbody(struct mm_mimepart *part, const char *data)
{
	assert(part != NULL);
	assert(data != NULL);

	if (part->body != NULL) {
		xfree(part->body);
		part->body = NULL;
	}

	part->body = xstrdup(data);
	part->length = strlen(data);
}

/**
 * Decodes a MIME part according to it's encoding using MiniMIME codecs
 *
 * @param A valid MIME part object
 * @return 0 if the MIME part could be successfully decoded or -1 if not
 * @note Sets mm_errno on error
 *
 * This function decodes the body of a MIME part with a registered decoder
 * according to it's Content-Transfer-Encoding header field. 
 */
char *
mm_mimepart_decode(struct mm_mimepart *part)
{
	extern struct mm_codecs codecs;
	struct mm_codec *codec;
	void *decoded;
	
	assert(part != NULL);
	assert(part->type != NULL);

	decoded = NULL;

	/* No encoding associated */
	if (part->type->encstring == NULL)
		return NULL;

	/* Loop through codecs and find a suitable one */
	SLIST_FOREACH(codec, &codecs, next) {
		if (!strcasecmp(part->type->encstring, codec->encoding)) {
			decoded = codec->decoder((char *)part->body);
			break;
		}
	}

	return decoded;
}

/** @{ 
 * @name Accessing the MIME part's Content-Type information
 */

/**
 * Attaches a context type object to a MIME part
 *
 * @param part A valid MIME part object
 * @param ct The content type object to attach
 */
void
mm_mimepart_attachcontenttype(struct mm_mimepart *part, struct mm_content *ct)
{
	part->type = ct;
}

struct mm_content *
mm_mimepart_gettype(struct mm_mimepart *part)
{
	assert(part != NULL);

	return part->type;
}

size_t
mm_mimepart_getlength(struct mm_mimepart *part)
{
	assert(part != NULL);

	return part->length;
}

/** } */
