/*
 * $Id: mm_contenttype.c,v 1.1 2004/05/03 22:05:56 jfi Exp $
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

/* This file is documented using Doxygen */

/**
 * @file mm_contenttype.c 
 *
 * This module contains functions for manipulating Content-Type objects.
 */

struct mm_encoding_mappings {
	const char *idstring;
	int type;
};

static struct mm_encoding_mappings mm_content_enctypes[] = {
	{ "Base64", MM_ENCODING_BASE64 },
	{ "Quoted-Printable", MM_ENCODING_QUOTEDPRINTABLE },
	{ NULL, - 1},
};

static const char *mm_composite_maintypes[] = {
	"multipart",
	"message",
	NULL,
};

static const char *mm_composite_encodings[] = {
	"7bit",
	"8bit",
	"binary",
	NULL,
};		

//static const char *quoted_charset = "()<>@,;:\"/[]?=";

/**
 * @{ 
 *
 * @name Functions for manipulating Content-Type parameters
 */

/**
 * Creates a new object to hold a Content-Type parameter. 
 * The allocated memory must later be freed using mm_ctparam_free()
 *
 * @return An object representing a Content-Type parameter
 * @ref mm_ctparam_free
 */
struct mm_ct_param *
mm_ctparam_new(void)
{
	struct mm_ct_param *param;

	param = (struct mm_ct_param *)xmalloc(sizeof(struct mm_ct_param));
	
	param->name = NULL;
	param->value = NULL;

	return param;
}

/**
 * Releases all memory associated with a Content-Type parameter object.
 *
 * @param param The object to be freed
 * @return Nothing
 */
void
mm_ctparam_free(struct mm_ct_param *param)
{
	assert(param != NULL);

	if (param->name != NULL) {
		xfree(param->name);
		param->name = NULL;
	}
	if (param->value != NULL) {
		xfree(param->value);
		param->value = NULL;
	}
	xfree(param);
}

/** @} */

/** @{
 * @name Functions for manipulating Content-Type objects
 */

/**
 * Creates a new object to hold a Content-Type representation.
 * The allocated memory must later be freed using mm_content_free()
 *
 * @return An object representing a MIME Content-Type
 * @see mm_content_free
 */
struct mm_content *
mm_content_new(void)
{
	struct mm_content *ct;

	ct = (struct mm_content *)xmalloc(sizeof(struct mm_content));

	ct->maintype = NULL;
	ct->subtype = NULL;

	SLIST_INIT(&ct->params);

	ct->encoding = MM_ENCODING_NONE;
	ct->encstring = NULL;

	return ct;
}

/**
 * Releases all memory associated with an Content-Type object
 *
 * @param ct A Content-Type object
 * @return Nothing
 */
void
mm_content_free(struct mm_content *ct)
{
	struct mm_ct_param *param;

	assert(ct != NULL);

	if (ct->maintype != NULL) {
		xfree(ct->maintype);
		ct->maintype = NULL;
	}
	if (ct->subtype != NULL) {
		xfree(ct->subtype);
		ct->subtype = NULL;
	}
	if (ct->encstring != NULL) {
		xfree(ct->encstring);
		ct->encstring = NULL;
	}

	SLIST_FOREACH(param, &ct->params, next) {
		SLIST_REMOVE(&ct->params, param, mm_ct_param, next);
		mm_ctparam_free(param);
	}	

	xfree(ct);
}

/**
 * Attaches a parameter to a Content-Type object
 *
 * @param ct The target Content-Type object
 * @param param The Content-Type parameter which to attach
 * @return 0 on success and -1 on failure
 */
int
mm_content_attachparam(struct mm_content *ct, struct mm_ct_param *param)
{
	struct mm_ct_param *tparam, *lparam;

	assert(ct != NULL);
	assert(param != NULL);

	if (SLIST_EMPTY(&ct->params)) {
		SLIST_INSERT_HEAD(&ct->params, param, next);
	} else {
		SLIST_FOREACH(tparam, &ct->params, next)
			if (tparam != NULL)
				lparam = tparam;
		SLIST_INSERT_AFTER(lparam, param, next);
	}

	return 0;
}		

/**
 * Parses a Content-Type header string into a Content-Type object.
 * The object returned needs later to be freed using mm_content_free()
 * This function sets the global variable mm_errno on error.
 *
 * @param string The string to parse
 * @param flags parser flags
 * @return A Content-Type object on success or a NULL pointer on failure.
 * @see mm_parseflags 
 */
struct mm_content *
mm_content_parse(const char *string, int flags)
{
	char *buf, *orig, *token, *ttoken;
	struct mm_content *ct;

	assert(string != NULL);

	mm_errno = MM_ERROR_NONE;

	buf = xstrdup(string);
	orig = buf;
	ct = mm_content_new();

	token = strsep(&buf, ";");
	if (token == NULL) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("No ; separator in Content-Type");
		goto cleanup;
	}

	/* Get main and sub MIME types */
	ttoken = strsep(&token, "/");
	if (ttoken != NULL) {
		ct->maintype = xstrdup(ttoken);
	}
	ttoken = strsep(&token, "");
	if (ttoken != NULL) {
		ct->subtype = xstrdup(ttoken);
	}

	/* Fix broken implementations that put a single semicolon after the
	 * media type without specifying any parameters afterwards, for
	 * example:
	 *
	 * 	Content-Type: text/plain;  
	 *
	 * If we use MM_PARSE_LOOSE, we ignore this quirk, else we trigger
	 * an error of type MM_ERROR_MIME.
	 *
	 * RFC 2045 states in section 5.1 for the grammar of the Content-Type
	 * field:
	 *
	 *     content := "Content-Type" ":" type "/" subtype
	 *                *(";" parameter)
	 */ 
	if (buf != NULL && strlen(buf) < 2) {
		if (flags & MM_PARSE_LOOSE) {
			goto cleanup;
		} else {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("Content-Type header has a semicolon "
			    "but no parameters");
			goto cleanup;
		}
	}

	/* Now as we have the MIME type, go on and extract all parameters
	 * in the form of "option=value". Values might (indeed, they have
	 * to) be quoted in the string. We'll unquote them before we save
	 * them.
	 *
	 * TODO: check parameter values for MIME conformity
	 */
	/** @bug We are not able to parse tspecials this way... */
	while ((token = strsep(&buf, ";")) != NULL) {
		struct mm_ct_param *param;
		ttoken = strsep(&token, "=");
		if (ttoken == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("could not parse parameter");
			goto cleanup;
		}
		
		param = mm_ctparam_new();
		while (*ttoken && isspace(*ttoken)) {
			ttoken++;
		}

		STRIP_TRAILING(ttoken, " \t");
		param->name = xstrdup(ttoken);

		ttoken = strsep(&token, "");
		if (ttoken == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("could not parse parameter");
			mm_ctparam_free(param);
			goto cleanup;
		}
		if (*ttoken == '\"' || *ttoken == '\'') {
		} else {
		}
		param->value = mm_unquote(ttoken);

		mm_content_attachparam(ct, param);
	}

cleanup:
	if (orig != NULL) {
		xfree(orig);
		orig = NULL;
	}

	if (mm_errno != MM_ERROR_NONE) {
		if (ct != NULL) {
			mm_content_free(ct);
			ct = NULL;
		}
		return NULL;
	} else {
		return ct;
	}
}

/**
 * Gets a parameter value from a Content-Type object.
 *
 * @param ct the Content-Type object
 * @param name the name of the parameter to retrieve
 * @return The value of the parameter on success or a NULL pointer on failure
 */
char *
mm_content_getparambyname(struct mm_content *ct, const char *name)
{
	struct mm_ct_param *param;

	assert(ct != NULL);
	
	SLIST_FOREACH(param, &ct->params, next) {
		if (!strcasecmp(param->name, name)) {
			return param->value;
		}
	}

	return NULL;
}

/**
 * Sets the MIME main type for a MIME Content-Type object
 *
 * @param ct The MIME Content-Type object
 * @param value The value which to set the main type to
 * @param copy Whether to make a copy of the value (original value must be
 *        freed afterwards to prevent memory leaks).
 */
int
mm_content_setmaintype(struct mm_content *ct, char *value, int copy)
{
	assert(ct != NULL);
	assert(value != NULL);

	if (copy) {
		/**
		 * @bug The xfree() call could lead to undesirable results. 
 		 * Do we really need it?
		 */
		if (ct->maintype != NULL) {
			xfree(ct->maintype);
		}
		ct->maintype = xstrdup(value);
	} else {
		ct->maintype = value;
	}

	return 0;
}

char *
mm_content_getmaintype(struct mm_content *ct)
{
	assert(ct != NULL);
	assert(ct->maintype != NULL);

	return ct->maintype;
}

char *
mm_content_getsubtype(struct mm_content *ct)
{
	assert(ct != NULL);
	assert(ct->subtype != NULL);

	return ct->subtype;
}

char *
mm_content_gettype(struct mm_content *ct)
{
	assert(ct != NULL);
	assert(ct->subtype != NULL);

	return ct->subtype;
}


/**
 * Sets the MIME sub type for a MIME Content-Type object
 *
 * @param ct The MIME Content-Type object
 * @param value The value which to set the sub type to
 * @param copy Whether to make a copy of the value (original value must be
 *        freed afterwards to prevent memory leaks).
 */
int
mm_content_setsubtype(struct mm_content *ct, char *value, int copy)
{
	assert(ct != NULL);
	assert(value != NULL);

	if (copy) {
		/**
		 * @bug The xfree() call could lead to undesirable results. 
 		 * Do we really need it?
		 */
		if (ct->subtype != NULL) {
			xfree(ct->subtype);
		}
		ct->subtype = xstrdup(value);
	} else {
		ct->subtype = value;
	}

	return 0;
}

int
mm_content_settype(struct mm_content *ct, const char *fmt, ...)
{
	char *maint, *subt;
	char buf[512], *parse;
	va_list ap;
	
	mm_errno = MM_ERROR_NONE;
	
	va_start(ap, fmt);
	/* Make sure no truncation occurs */
	if (vsnprintf(buf, sizeof buf, fmt, ap) > sizeof buf) {
		mm_errno = MM_ERROR_ERRNO;
		mm_error_setmsg("Input string too long");
		return -1;
	}
	va_end(ap);

	parse = buf;
	maint = strsep(&parse, "/");
	if (maint == NULL) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("Invalid type specifier: %s", buf);
		return -1;
	}
	ct->maintype = xstrdup(maint);

	subt = strsep(&parse, "");
	if (subt == NULL) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("Invalid type specifier: %s", buf);
		return -1;
	}
	ct->subtype = xstrdup(subt);
	
	return 0;
}

int
mm_content_iscomposite(struct mm_content *ct)
{
	int i;

	for (i = 0; mm_composite_maintypes[i] != NULL; i++) {
		if (!strcasecmp(ct->maintype, mm_composite_maintypes[i])) {
			return 1;
		}
	}

	/* Not found */
	return 0;
}

int
mm_content_isvalidencoding(const char *encoding)
{
	int i;
	
	for (i = 0; mm_composite_encodings[i] != NULL; i++) {
		if (!strcasecmp(encoding, mm_composite_encodings[i])) {
			return 1;
		}
	}

	/* Not found */
	return 0;
}

/**
 * Set the encoding of a MIME entitity according to a mapping table
 *
 * @param ct A valid content type object
 * @param encoding A string representing the content encoding
 * @return 0 if successfull or -1 if not (i.e. unknown content encoding)
 */
int
mm_content_setencoding(struct mm_content *ct, const char *encoding)
{
	int i;

	assert(ct != NULL);
	assert(encoding != NULL);

	for (i = 0; mm_content_enctypes[i].idstring != NULL; i++) {
		if (!strcasecmp(mm_content_enctypes[i].idstring, encoding)) {
			ct->encoding = mm_content_enctypes[i].type;
			ct->encstring = xstrdup(encoding);
			return 0;
		}
	}

	/* If we didn't find a mapping, set the encoding to unknown */
	ct->encoding = MM_ENCODING_UNKNOWN;
	ct->encstring = NULL;
	return 1;
}

/**
 * Gets the numerical ID of a content encoding identifier
 *
 * @param ct A valid Content Type object
 * @param encoding A string representing the content encoding identifier
 * @return The numerical ID of the content encoding
 */ 
int
mm_content_getencoding(struct mm_content *ct, const char *encoding)
{
	int i;

	assert(ct != NULL);

	for (i = 0; mm_content_enctypes[i].idstring != NULL; i++) {
		if (!strcasecmp(mm_content_enctypes[i].idstring, encoding)) {
			return mm_content_enctypes[i].type;
		}
	}

	/* Not found */
	return MM_ENCODING_UNKNOWN;
}

/** @} */
