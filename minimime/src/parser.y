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
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "parser.h"
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

//%token <string> EOL
%token <string> CONTENTDISPOSITION_HEADER
%token <string> CONTENTENCODING_HEADER
%token <string> CONTENTTYPE_HEADER
%token <string> MAIL_HEADER
%token <string> HEADERVALUE
%token <string> BOUNDARY
%token <string> ENDBOUNDARY
%token <string> CONTENTTYPE_VALUE 
%token <string> WORD
%token <string> TSPECIAL

%token <position> BODY
%token <position> PREAMBLE

%type  <string> content_parameter_value
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
	mimeparts endboundary
	{
		dprintf("This was a multipart message\n");
	}
	;

singlepart_message:	
	headers body
	{
		dprintf("This was a single part message\n");
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
		if (!have_contenttype) {
			ct = mm_content_new();
			mm_content_settype(ct, "text/plain");
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
		//printf("PREAMBLE: (%d/%d)\n", $1.start, $1.end);
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
		hdr = mm_mimeheader_generate($1, strdup(""));
		mm_mimepart_attachheader(current_mimepart, hdr);
	}
	;

contenttype_header:
	CONTENTTYPE_HEADER COLON mimetype EOL
	{
		struct mm_mimeheader *hdr;
		hdr = mm_mimeheader_generate($1, $3);
		mm_mimepart_attachheader(current_mimepart, hdr);

		mm_content_settype(ctype, "%s", $3);
		mm_mimepart_attachcontenttype(current_mimepart, ctype);
		dprintf("Content-Type -> %s\n", $3);
		ctype = mm_content_new();
	}
	|
	CONTENTTYPE_HEADER COLON mimetype content_parameters EOL
	{
		struct mm_mimeheader *hdr;
		hdr = mm_mimeheader_generate($1, $3);
		mm_mimepart_attachheader(current_mimepart, hdr);
		mm_content_settype(ctype, "%s", $3);
		mm_mimepart_attachcontenttype(current_mimepart, ctype);
		dprintf("Content-Type (P) -> %s\n", $3);
		ctype = mm_content_new();
	}
	;

contentdisposition_header:
	CONTENTDISPOSITION_HEADER COLON WORD EOL
	{
		dprintf("Content-Disposition -> %s\n", $3);
	}
	|
	CONTENTDISPOSITION_HEADER COLON WORD content_parameters EOL
	{
		dprintf("Content-Disposition (P) -> %s\n", $3);
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
	ANY any
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

content_parameters: 
	SEMICOLON content_parameter content_parameters
	|
	SEMICOLON content_parameter
	|
	SEMICOLON
	;

content_parameter:	
	WORD EQUAL content_parameter_value
	{
		struct mm_param *param;
		param = mm_param_new();
		
		dprintf("Param: '%s', Value: '%s'\n", $1, $3);
		
		if (!strcasecmp($1, "boundary")) {
			if (boundary_string == NULL) {
				set_boundary($3);
			} else {
				//printf("Double Boundary not allowed!\n");
			}	
		}

		param->name = strdup($1);
		param->value = strdup($3);

		mm_content_attachparam(ctype, param);
	}
	;

content_parameter_value:
	WORD
	{
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
			return(-1);
		}
		if (strcmp(boundary_string, $1)) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("invalid boundary: %s", $1);
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
			return(-1);
		}
		if (strcmp(endboundary_string, $1)) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("invalid end boundary: %s", $1);
			return(-1);
		}
		dprintf("End of MIME message\n");
	}
	;

body:
	BODY
	{
		size_t body_size;
		size_t current;
		size_t start;
		size_t offset;
		char *body;

		dprintf("BODY (%d/%d), SIZE %d\n", $1.start, $1.end, $1.end - $1.start);

		/* calculate start and offset markers for the opaque and
		 * header stripped body message.
		 */
		if ($1.opaque_start > 0) {
			if ($1.start < $1.opaque_start) {
				mm_errno = MM_ERROR_PARSE;
				mm_error_setmsg("internal incosistency,1");
				return(-1);
			}
			start = $1.opaque_start;
			offset = $1.start - start;
			//assert(offset < 0);
		} else {
			start = $1.start;
			offset = 0;
		}

		/* The next three cases should NOT happen anytime */
		if ($1.end <= start) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("internal incosistency,2");
			return(-1);
		}
		if (start < offset) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("internal incosistency, S:%d,O:%d,L:%d", start, offset, lineno);
			return(-1);
		}	
		if (start < 0 || $1.end < 0) {
			mm_errno = MM_ERROR_PARSE;
			mm_error_setmsg("internal incosistency,4");
			return(-1);
		}	

		/* XXX: do we want to enforce a maximum body size? make it a
		 * parser option? */

		/* Read in the body message */
		body_size = $1.end - start;
		body = (char *)malloc(body_size + 1);
		if (body == NULL) {
			mm_errno = MM_ERROR_ERRNO;
			return(-1);
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

		current_mimepart->opaque_body = body;
		current_mimepart->body = body + offset;
	}
	;

%%

void mm_yyerror(const char *str)
{
	fprintf(stderr, "Error: %s at line %d\n", str, lineno);
	fprintf(stderr, "In condition: %d\n", condition);
	exit(1);
}

int mm_yywrap(void)
{
	return 1;
}

/**
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

int
PARSER_initialize(MM_CTX *newctx)
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
	envelope = mm_mimepart_new();
	current_mimepart = envelope;
	ctype = mm_content_new();

	have_contenttype = 0;

	curin = mm_yyin;

	return 1;
}

/*
int
main(int argc, char **argv)
{
	FILE *fp;
	struct mm_mimeheader *last, *this;
	int parts, i;

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s <file>\n", argv[0]);
		exit(1);
	}

	if ((fp = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	yyin = fp;
	curin = fp;
	
	mm_library_init();

	ctx = mm_context_new();
	envelope = mm_mimepart_new();

	assert(ctx != NULL);
	assert(envelope != NULL);

	current_mimepart = envelope;
	ctype = mm_content_new();
	
	if (yyparse() != 0) {
		printf("ERROR: ERROR\n");
	}

	dprintf("Message parsed successfully, %d lines\n", lineno);
	dprintf("Message contained %d MIME %s\n", mime_parts, 
	    mime_parts > 1 ? "parts" : "part");

	parts = mm_context_countparts(ctx);

	dprintf("MiniMIME says: %d parts\n", parts);
	
	for (i = 0; i < parts; i++) {
		dprintf("SHOWING PART %d\n", i);
		tmppart = mm_context_getpart(ctx, i);
		assert(tmppart != NULL);
		if (mm_mimepart_headers_start(tmppart, &last) == -1) {
			dprintf("NO HEADERS FOR THIS MIMEPART\n");
		} else {	
			while ((this = mm_mimepart_headers_next(tmppart, &last)) != NULL) {
				dprintf("%s: %s\n", this->name, this->value);
			}
			if (tmppart->type != NULL) {
				dprintf("CT-P: %s\n", mm_content_paramstostring(tmppart->type));
			}
		}
	}	
	
	exit(0);
}
*/
