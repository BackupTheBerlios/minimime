%{
/*
 * Copyright (c) 2004 Jann Fischer. All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * These are the grammatic definitions in yacc syntax to parse MIME conform
 * messages.
 *
 * TODO:
 *	- honour parse flags passed to us (partly done)
 *	- parse Content-Disposition header (partly done)
 *	- parse Content-Encoding header
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mimeparser.h"
#include "mm.h"
#include "mm_internal.h"

extern int lineno;
extern int condition;

char *boundary_string = NULL;
char *endboundary_string = NULL;

const char *message_buffer = NULL;

extern FILE *mm_yyin;
FILE *curin;

static int mime_parts = 0;
static int debug = 0;

/* MiniMIME specific object pointers */
static MM_CTX *ctx;
static struct mm_mimepart *envelope = NULL;
static struct mm_mimepart *tmppart = NULL;
static struct mm_content *ctype = NULL;

/* Always points to the current MIME part */
static struct mm_mimepart *current_mimepart = NULL;

/* Marker for indicating a found Content-Type header */
static int have_contenttype;

/* The parse mode */
static int parsemode;

static char *PARSE_readmessagepart(size_t, size_t, size_t, size_t *);

%}

%union
{
	int number;
	char *string;
	struct s_position position;
}

%token ANY
%token COLON 
%token DASH
%token DQUOTE
%token ENDOFHEADERS
%token EOL
%token EOM
%token EQUAL
%token MIMEVERSION_HEADER
%token SEMICOLON

%token <string> CONTENTDISPOSITION_HEADER
%token <string> CONTENTENCODING_HEADER
%token <string> CONTENTTYPE_HEADER
%token <string> MAIL_HEADER
%token <string> HEADERVALUE
%token <string> BOUNDARY
%token <string> ENDBOUNDARY
%token <string> CONTENTTYPE_VALUE 
%token <string> TSPECIAL
%token <string> WORD

%token <position> BODY
%token <position> PREAMBLE
%token <position> POSTAMBLE

%type  <string> content_disposition
%type  <string> contenttype_parameter_value
%type  <string> mimetype
%type  <string> body

%start message

%%

/* This is a parser for a MIME-conform message, which is in either single
 * part or multi part format.
 */
message :
	multipart_message
	|
	singlepart_message
	;

multipart_message:
	headers preamble 
	{ 
		mm_context_attachpart(ctx, current_mimepart);
		current_mimepart = mm_mimepart_new();
		have_contenttype = 0;
	}
	mimeparts endboundary postamble
	{
		dprintf("This was a multipart message\n");
	}
	;

singlepart_message:	
	headers body
	{
		dprintf("This was a single part message\n");
		mm_context_attachpart(ctx, current_mimepart);
	}
	;
	
headers :
	header headers
	|
	end_headers
	{
		/* If we did not find a Content-Type header for the current
		 * MIME part (or envelope), we create one and attach it.
		 * According to the RFC, a type of "text/plain" and a
		 * charset of "us-ascii" can be assumed.
		 */
		struct mm_content *ct;
		struct mm_param *param;

		if (!have_contenttype) {
			ct = mm_content_new();
			mm_content_settype(ct, "text/plain");
			
			param = mm_param_new();
			param->name = xstrdup("charset");
			param->value = xstrdup("us-ascii");

			mm_content_attachparam(ct, param);
			mm_mimepart_attachcontenttype(current_mimepart, ct);
		}	
		have_contenttype = 0;
	}
	|
	header
	;

preamble:
	PREAMBLE
	{
		char *preamble;
		size_t offset;
		
		if ($1.start != $1.end) {
			preamble = PARSE_readmessagepart(0, $1.start, $1.end,
			    &offset);
			if (preamble == NULL) {
				return(-1);
			}
			ctx->preamble = preamble;
			dprintf("PREAMBLE:\n%s\n", preamble);
		}
	}
	|
	;

postamble:
	POSTAMBLE
	{
	}
	|
	;

mimeparts:
	mimeparts mimepart
	|
	mimepart
	;

mimepart:
	boundary headers body
	{

		if (mm_context_attachpart(ctx, current_mimepart) == -1) {
			mm_errno = MM_ERROR_ERRNO;
			return(-1);
		}	

		tmppart = mm_mimepart_new();
		current_mimepart = tmppart;
		mime_parts++;
	}
	;
	
header	:
	mail_header
	|
	contenttype_header
	{
		have_contenttype = 1;
		if (mm_content_iscomposite(envelope->type)) {
			ctx->messagetype = MM_MSGTYPE_MULTIPART;
		} else {
			ctx->messagetype = MM_MSGTYPE_FLAT;
		}	
	}
	|
	contentdisposition_header
	|
	contentencoding_header
	|
	mimeversion_header
	|
	invalid_header
	{
		if (parsemode != MM_PARSE_LOOSE) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("invalid header encountered");
			mm_error_setlineno(lineno);
			return(-1);
		} else {
			/* TODO: attach MM_WARNING_INVHDR */
		}
	}
	;

mail_header:
	MAIL_HEADER COLON WORD EOL
	{
		struct mm_mimeheader *hdr;
		hdr = mm_mimeheader_generate($1, $3);
		mm_mimepart_attachheader(current_mimepart, hdr);
	}
	|
	MAIL_HEADER COLON EOL
	{
		struct mm_mimeheader *hdr;

		if (parsemode != MM_PARSE_LOOSE) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("invalid header encountered");
			mm_error_setlineno(lineno);
			return(-1);
		} else {
			/* TODO: attach MM_WARNING_INVHDR */
		}	
		
		hdr = mm_mimeheader_generate($1, xstrdup(""));
		mm_mimepart_attachheader(current_mimepart, hdr);
	}
	;

contenttype_header:
	CONTENTTYPE_HEADER COLON mimetype EOL
	{
		mm_content_settype(ctype, "%s", $3);
		mm_mimepart_attachcontenttype(current_mimepart, ctype);
		dprintf("Content-Type -> %s\n", $3);
		ctype = mm_content_new();
	}
	|
	CONTENTTYPE_HEADER COLON mimetype contenttype_parameters EOL
	{
		mm_content_settype(ctype, "%s", $3);
		mm_mimepart_attachcontenttype(current_mimepart, ctype);
		dprintf("Content-Type (P) -> %s\n", $3);
		ctype = mm_content_new();
	}
	;

contentdisposition_header:
	CONTENTDISPOSITION_HEADER COLON content_disposition EOL
	{
		dprintf("Content-Disposition -> %s\n", $3);
	}
	|
	CONTENTDISPOSITION_HEADER COLON content_disposition content_disposition_parameters EOL
	{
		dprintf("Content-Disposition (P) -> %s\n", $3);
	}
	;

content_disposition:
	WORD
	{
		/*
		 * According to RFC 2183, the content disposition value may
		 * only be "inline", "attachment" or an extension token. We
		 * catch invalid values here if we are not in loose parsing
		 * mode.
		 */
		if (strcasecmp($1, "inline") && strcasecmp($1, "attachment")
		    && strncasecmp($1, "X-", 2)) {
			if (parsemode != MM_PARSE_LOOSE) {
				mm_errno = MM_ERROR_MIME;
				mm_error_setmsg("invalid content-disposition");
				return(-1);
			}	
		} else {
			/* TODO: attach MM_WARNING_INVHDR */
		}	
		$$ = $1;
	}
	;

contentencoding_header:
	CONTENTENCODING_HEADER COLON WORD EOL
	{
		dprintf("Content-Transfer-Encoding -> %s\n", $3);
	}
	;

mimeversion_header:
	MIMEVERSION_HEADER COLON WORD EOL
	{
		dprintf("MIME-Version -> '%s'\n", $3);
	}
	;

invalid_header:
	any EOL
	;

any:
	any ANY
	|
	ANY
	;
	
mimetype:
	WORD '/' WORD
	{
		char type[255];
		snprintf(type, sizeof(type), "%s/%s", $1, $3);
		$$ = type;
	}	
	;

contenttype_parameters: 
	SEMICOLON contenttype_parameter contenttype_parameters
	|
	SEMICOLON contenttype_parameter
	|
	SEMICOLON
	{
		if (parsemode != MM_PARSE_LOOSE) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("invalid Content-Type header");
			mm_error_setlineno(lineno);
			return(-1);
		} else {
			/* TODO: attach MM_WARNING_INVHDR */
		}	
	}
	;

content_disposition_parameters:
	SEMICOLON content_disposition_parameter content_disposition_parameters
	|
	SEMICOLON content_disposition_parameter
	|
	SEMICOLON
	{	
		if (parsemode != MM_PARSE_LOOSE) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("invalid Content-Disposition header");
			mm_error_setlineno(lineno);
			return(-1);
		} else {
			/* TODO: attach MM_WARNING_INVHDR */
		}
	}	
	;

contenttype_parameter:	
	WORD EQUAL contenttype_parameter_value
	{
		struct mm_param *param;
		param = mm_param_new();
		
		dprintf("Param: '%s', Value: '%s'\n", $1, $3);
		
		/* Catch an eventual boundary identifier */
		if (!strcasecmp($1, "boundary")) {
			if (boundary_string == NULL) {
				set_boundary($3);
			} else {
				if (parsemode != MM_PARSE_LOOSE) {
					mm_errno = MM_ERROR_MIME;
					mm_error_setmsg("duplicate boundary "
					    "found");
					return -1;
				} else {
					/* TODO: attach MM_WARNING_DUPPARAM */
				}
			}
		}

		param->name = xstrdup($1);
		param->value = xstrdup($3);

		mm_content_attachparam(ctype, param);
	}
	;

content_disposition_parameter:
	WORD EQUAL contenttype_parameter_value
	{
		if (!strcasecmp($3, "filename") 
		    && current_mimepart->filename == NULL) {
			current_mimepart->filename = xstrdup($3);
		} else if (!strcasecmp($3, "creation-date")
		    && current_mimepart->creation_date == NULL) {
			current_mimepart->creation_date = xstrdup($3);
		} else if (!strcasecmp($3, "modification-date")
		    && current_mimepart->modification_date == NULL) {
			current_mimepart->modification_date = xstrdup($3);
		} else if (!strcasecmp($3, "read-date")
		    && current_mimepart->read_date == NULL) {
		    	current_mimepart->read_date = xstrdup($3);
		} else if (!strcasecmp($3, "size")
		    && current_mimepart->disposition_size == NULL) {
		    	current_mimepart->disposition_size = xstrdup($3);
		} else {
			if (parsemode != MM_PARSE_LOOSE) {
				mm_errno = MM_ERROR_MIME;
				mm_error_setmsg("invalid disposition "
				    "parameter");
				return -1;
			} else {
				/* TODO: attach MM_WARNING_INVPARAM */
			}	
		}	
	}
	;

contenttype_parameter_value:
	WORD
	{
		$$ = $1;
	}
	|
	TSPECIAL
	{
		/* For broken MIME implementation */
		if (parsemode != MM_PARSE_LOOSE) {
			mm_errno = MM_ERROR_MIME;
			mm_error_setmsg("tspecial without quotes");
			mm_error_setlineno(lineno);
			return(-1);
		} else {
			/* TODO: attach MM_WARNING_INVAL */
		}	
		$$ = $1;
	}
	|
	'"' TSPECIAL '"'
	{
		$$ = $2;
	}
	;
	
end_headers	:
	ENDOFHEADERS
	{
		dprintf("End of headers at line %d\n", lineno);
	}
	;

boundary	:
	BOUNDARY EOL
	{
		if (boundary_string == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("internal incosistency");
			mm_error_setlineno(lineno);
			return(-1);
		}
		if (strcmp(boundary_string, $1)) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("invalid boundary: '%s' (%d)", $1, strlen($1));
			mm_error_setlineno(lineno);
			return(-1);
		}
		dprintf("New MIME part... (%s)\n", $1);
	}
	;

endboundary	:
	ENDBOUNDARY
	{
		if (endboundary_string == NULL) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("internal incosistency");
			mm_error_setlineno(lineno);
			return(-1);
		}
		if (strcmp(endboundary_string, $1)) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("invalid end boundary: %s", $1);
			mm_error_setlineno(lineno);
			return(-1);
		}
		dprintf("End of MIME message\n");
	}
	;

body:
	BODY
	{
		char *body;
		size_t offset;

		dprintf("BODY (%d/%d), SIZE %d\n", $1.start, $1.end, $1.end - $1.start);

		body = PARSE_readmessagepart($1.opaque_start, $1.start, $1.end,
		    &offset);

		if (body == NULL) {
			return(-1);
		}	
		current_mimepart->opaque_body = body;
		current_mimepart->body = body + offset;
	}
	;

%%

/*
 * This function gets the specified part from the currently parsed message.
 */
static char *
PARSE_readmessagepart(size_t opaque_start, size_t real_start, size_t end, 
    size_t *offset)
{
	size_t body_size;
	size_t current;
	size_t start;
	char *body;

	/* calculate start and offset markers for the opaque and
	 * header stripped body message.
	 */
	if (opaque_start > 0) {
		/* Multipart message */
		if (real_start) {
			if (real_start < opaque_start) {
				mm_errno = MM_ERROR_PARSE;
				mm_error_setmsg("internal incosistency (S:%d/O:%d)",
				    real_start,
				    opaque_start);
				return(NULL);
			}
			start = opaque_start;
			*offset = real_start - start;
		/* Flat message */	
		} else {	
			start = opaque_start;
			*offset = 0;
		}	
	} else {
		start = real_start;
		*offset = 0;
	}

	/* The next three cases should NOT happen anytime */
	if (end <= start) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("internal incosistency,2");
		mm_error_setlineno(lineno);
		return(NULL);
	}
	if (start < *offset) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("internal incosistency, S:%d,O:%d,L:%d", start, offset, lineno);
		mm_error_setlineno(lineno);
		return(NULL);
	}	
	if (start < 0 || end < 0) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("internal incosistency,4");
		mm_error_setlineno(lineno);
		return(NULL);
	}	

	/* XXX: do we want to enforce a maximum body size? make it a
	 * parser option? */

	/* Read in the body message */
	body_size = end - start;

	if (body_size < 1) {
		mm_errno = MM_ERROR_PARSE;
		mm_error_setmsg("size of body cannot be < 1");
		mm_error_setlineno(lineno);
		return(NULL);
	}	
	
	body = (char *)malloc(body_size + 1);
	if (body == NULL) {
		mm_errno = MM_ERROR_ERRNO;
		return(NULL);
	}	
		
	/* Get the message body either from a stream or a memory
	 * buffer.
	 */
	if (mm_yyin != NULL) {
		current = ftell(curin);
		fseek(curin, start - 1, SEEK_SET);
		fread(body, body_size - 1, 1, curin);
		fseek(curin, current, SEEK_SET);
	} else if (message_buffer != NULL) {
		strlcpy(body, message_buffer + start - 1, body_size);
	} 
	
	return(body);

}

int
mm_yyerror(const char *str)
{
	mm_errno = MM_ERROR_PARSE;
	mm_error_setmsg("%s", str);
	mm_error_setlineno(lineno);
	return -1;
}

int 
mm_yywrap(void)
{
	return 1;
}

/*
 * Sets the boundary value for the current message
 */
int 
set_boundary(char *str)
{
	size_t blen;

	blen = strlen(str);

	boundary_string = (char *)malloc(blen + 3);
	endboundary_string = (char *)malloc(blen + 5);

	if (boundary_string == NULL || endboundary_string == NULL) {
		if (boundary_string != NULL) {
			free(boundary_string);
		}
		if (endboundary_string != NULL) {
			free(endboundary_string);
		}	
		return -1;
	}
	
	ctx->boundary = xstrdup(str);

	snprintf(boundary_string, blen + 3, "--%s", str);
	snprintf(endboundary_string, blen + 5, "--%s--", str);

	return 0;
}

/**
 * Debug printf()
 */
int
dprintf(const char *fmt, ...)
{
	va_list ap;
	char *msg;
	if (debug == 0) return 1;

	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s", msg);
	free(msg);

	return 0;
	
}

/**
 * Initializes the parser engine.
 */
int
PARSER_initialize(MM_CTX *newctx, int mode)
{
	if (ctx != NULL) {
		xfree(ctx);
		ctx = NULL;
	}
	if (envelope != NULL) {
		xfree(envelope);
		envelope = NULL;
	}	
	if (ctype != NULL) {
		xfree(ctype);
		ctype = NULL;
	}	

	ctx = newctx;
	parsemode = mode;

	envelope = mm_mimepart_new();
	current_mimepart = envelope;
	ctype = mm_content_new();

	have_contenttype = 0;

	curin = mm_yyin;

	return 1;
}

