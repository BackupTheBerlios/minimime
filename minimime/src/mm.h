/*
 * $Id: mm.h,v 1.10 2004/06/04 14:27:29 jfi Exp $
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

/** @file mm.h
 * Data definitions for MiniMIME
 */
#ifndef __MM_H
#define __MM_H
#include <sys/types.h>
#include <assert.h>
#include "mm_queue.h"

#define MM_MIME_LINELEN 998
#define MM_BASE64_LINELEN 76

TAILQ_HEAD(mm_mimeheaders, mm_mimeheader);
TAILQ_HEAD(mm_mimeparts, mm_mimepart);
TAILQ_HEAD(mm_params, mm_param);
SLIST_HEAD(mm_codecs, mm_codec);
SLIST_HEAD(mm_warnings, mm_warning);

/**
 * Available parser flags
 */
enum mm_parseflags
{
	MM_PARSE_NONE = (1L << 0),
	MM_PARSE_LOOSE = (1L << 1),
	MM_PARSE_STRIPCOMMENTS = (1L << 2),
	MM_PARSE_FASCIST = (1L << 2),
};

/**
 * Enumeration of MIME encodings
 */
enum mm_encoding
{
	MM_ENCODING_NONE = 0,
	MM_ENCODING_BASE64,
	MM_ENCODING_QUOTEDPRINTABLE,
	MM_ENCODING_UNKNOWN,
};

/**
 * Message type
 */
enum mm_messagetype
{
	/** Flat message */
	MM_MSGTYPE_FLAT = 0,
	/** Composite message */
	MM_MSGTYPE_MULTIPART,
};

/**
 * Enumeration of error categories
 */
enum mm_errors
{
	MM_ERROR_NONE = 0,
	MM_ERROR_UNDEF,
	MM_ERROR_ERRNO,	
	MM_ERROR_PARSE,		
	MM_ERROR_MIME,
	MM_ERROR_CODEC,
	MM_ERROR_PROGRAM
};

enum mm_warning_ids
{
	MM_WARN_NONE = 0,
	MM_WARN_PARSE,
	MM_WARN_MIME,
	MM_WARN_CODEC,
};

enum mm_addressfields {
	MM_ADDR_TO = 0,
	MM_ADDR_CC,
	MM_ADDR_BCC,
	MM_ADDR_FROM,
	MM_ADDR_SENDER,
	MM_ADDR_REPLY_TO
};

/**
 * More information about an error
 */
struct mm_error_data
{
	int error_id;
	int error_where;
	int lineno;
	char error_msg[128];
};
extern int mm_errno;
extern struct mm_error_data mm_error;

struct mm_warning
{
	char *message;
	int type;
	SLIST_ENTRY(mm_warning) next;
};

/**
 * Representation of a MiniMIME codec object
 */
struct mm_codec
{
	/** Encoding associated with this object */
	enum mm_encoding id;
	/** Textual representation of the encoding */
	char *encoding;

	/** Pointer to the encoder callback function */
	char *(*encoder)(char *, u_int32_t);
	/** Pointer to the decoder callback function */
	char *(*decoder)(char *);

	/** Pointer to the next codec */
	SLIST_ENTRY(mm_codec) next;
};

/**
 * Representation of a mail or MIME header field
 */
struct mm_mimeheader
{
	/** Name of the header field */
	char *name; 
	/** Value of the header field */
	char *value;

	/** Pointer to the next header field */
	TAILQ_ENTRY(mm_mimeheader) next;
};

/**
 * Representation of a MIME Content-Type parameter
 */
struct mm_param
{
	/** Name of the parameter */
	char *name; 
	/** Value of the parameter */
	char *value; 

	TAILQ_ENTRY(mm_param) next;
};

/**
 * Representation of a MIME Content-Type object
 */
struct mm_content
{
	/** Main type */
	char *maintype;
	/** Sub type */
	char *subtype;

	/** List of parameters attached to the Content-Type */
	struct mm_params params;

	/** String representing the encoding */
	char *encstring;
	/** MiniMIME representation of the encoding */
	enum mm_encoding encoding;
};

/**
 * Representation of a MIME part 
 */
struct mm_mimepart
{
	/** Mail headers of the MIME part */
	struct mm_mimeheaders headers;
	
	/** Length of the MIME part (with headers) */
	size_t opaque_length;
	/** Part body's opaque representation (with headers) */
	char *opaque_body;

	/** Length of the MIME part (without headers) */
	size_t length;
	/** Part body's representation (without headers) */
	char *body;

	/** Content-Type object of the part */
	struct mm_content *type;
	
	/** Content-Disposition type */
	char *disposition_type;
	/** Content-Disposition filename */
	char *filename;
	/** Content-Disposition creation-date */
	char *creation_date;
	/** Content-Disposition modification-date */
	char *modification_date;
	/** Content-Disposition read-date */
	char *read_date;
	/** Content-Disposition size */
	char *disposition_size;
	
	/** Pointer to the next MIME part */
	TAILQ_ENTRY(mm_mimepart) next;
};

/**
 * Represantation of a MiniMIME context
 */
struct mm_context
{
	/** Linked list of MIME parts in this context */
	struct mm_mimeparts parts;
	/** Type of the message */
	enum mm_messagetype messagetype;
	/** Linked list of warnings for this context */
	struct mm_warnings warnings;
	/** Linked list of registered codecs */
	struct mm_codecs codecs;
	/** Boundary string */
	char *boundary;
};
typedef struct mm_context MM_CTX;
typedef struct mm_context mm_ctx_t;

/**
 * @}
 */
 
/** @{
 * @name Utility functions
 */
char *mm_unquote(const char *);
char *mm_uncomment(const char *);
char *mm_stripchars(char *, char *);
char *mm_addchars(char *, char *, u_int16_t);
char *mm_gendate(void);
inline void mm_striptrailing(char **, const char *);

/**
 * @}
 * @{
 * @name Library initialization
 */
int mm_library_init(void);
int mm_library_isinitialized(void);
int PARSER_initialize(MM_CTX *);

/**
 * @}
 * @{
 * @name Manipulating MiniMIME contexts
 */
MM_CTX *mm_context_new(void);
void mm_context_free(MM_CTX *);
int mm_context_attachpart(MM_CTX *, struct mm_mimepart *);
int mm_context_deletepart(MM_CTX *, int, int);
int mm_context_countparts(MM_CTX *);
struct mm_mimepart *mm_context_getpart(MM_CTX *, int);
int mm_context_iscomposite(MM_CTX *);
int mm_context_haswarnings(MM_CTX *);

int mm_parse_mem(MM_CTX *, const char *, int, int);
int mm_parse_file(MM_CTX *, const char *, int, int);

/**
 * @}
 * @{
 * @name Manipulating MIME headers
 */
struct mm_mimeheader *mm_mimeheader_new(void);
void mm_mimeheader_free(struct mm_mimeheader *);
struct mm_mimeheader *mm_mimeheader_generate(const char *, const char *);
int mm_mimeheader_uncomment(struct mm_mimeheader *);
int mm_mimeheader_uncommentbyname(struct mm_mimepart *, const char *);
int mm_mimeheader_uncommentall(struct mm_mimepart *);

/**
 * @}
 * @{
 * @name Manipulating MIME parts
 */
struct mm_mimepart *mm_mimepart_new(void);
void mm_mimepart_free(struct mm_mimepart *);
int mm_mimepart_attachheader(struct mm_mimepart *, struct mm_mimeheader *);
int mm_mimepart_countheaders(struct mm_mimepart *part);
int mm_mimepart_countheaderbyname(struct mm_mimepart *, const char *);
struct mm_mimeheader *mm_mimepart_getheaderbyname(struct mm_mimepart *, const char *, int);
const char *mm_mimepart_getheadervalue(struct mm_mimepart *, const char *, int);
int mm_mimepart_headers_start(struct mm_mimepart *, struct mm_mimeheader **);
struct mm_mimeheader *mm_mimepart_headers_next(struct mm_mimepart *, struct mm_mimeheader **);
char *mm_mimepart_decode(struct mm_mimepart *);
struct mm_content *mm_mimepart_gettype(struct mm_mimepart *);
size_t mm_mimepart_getlength(struct mm_mimepart *);
char *mm_mimepart_getbody(struct mm_mimepart *, int);
void mm_mimepart_attachcontenttype(struct mm_mimepart *, struct mm_content *);
/** @} */

struct mm_content *mm_content_new(void);
void mm_content_free(struct mm_content *);
int mm_content_attachparam(struct mm_content *, struct mm_param *);
struct mm_content *mm_content_parse(const char *, int);
char *mm_content_getparambyname(struct mm_content *, const char *);
int mm_content_setmaintype(struct mm_content *, char *, int);
int mm_content_setsubtype(struct mm_content *, char *, int);
int mm_content_settype(struct mm_content *, const char *, ...);
char *mm_content_getmaintype(struct mm_content *);
char *mm_content_getsubtype(struct mm_content *);
char *mm_content_gettype(struct mm_content *);
int mm_content_iscomposite(struct mm_content *);
int mm_content_isvalidencoding(const char *);
int mm_content_setencoding(struct mm_content *, const char *);
char *mm_content_paramstostring(struct mm_content *);

struct mm_param *mm_param_new(void);
void mm_param_free(struct mm_param *);

char *mm_flatten_mimepart(struct mm_mimepart *);
char *mm_flatten_context(MM_CTX *);

int mm_codec_isregistered(const char *);
int mm_codec_hasdecoder(const char *);
int mm_codec_hasencoder(const char *);
int mm_codec_register(const char *, char *(*encoder)(char *, u_int32_t), char *(*decoder)(char *));
int mm_codec_unregister(const char *);
int mm_codec_unregisterall(void);
void mm_codec_registerdefaultcodecs(void);

/* CODECS */
char *mm_base64_decode(char *);
char *mm_base64_encode(char *, u_int32_t);

void mm_error_init(void);
void mm_error_setmsg(const char *, ...);
void mm_error_setlineno(int lineno);
char *mm_error_string(void);

void mm_warning_add(MM_CTX *, int, const char *, ...);
struct mm_warning *mm_warning_next(MM_CTX *, struct mm_warning **);

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif /* ! HAVE_STRLCPY */
#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif /* ! HAVE_STRLCAT */

#define MM_ISINIT() do { \
	assert(mm_library_isinitialized() == 1); \
} while (0);

#endif /* ! __MM_H */
