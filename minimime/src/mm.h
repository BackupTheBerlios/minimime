/*
 * $Id: mm.h,v 1.4 2004/06/01 09:39:48 jfi Exp $
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

#include <assert.h>
#include "mm_queue.h"

#define MM_MIME_LINELEN 998
#define MM_BASE64_LINELEN 76

SLIST_HEAD(mm_mimeheaders, mm_mimeheader);
SLIST_HEAD(mm_mimeparts, mm_mimepart);
SLIST_HEAD(mm_params, mm_param);
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

enum mm_messagetype
{
	MM_MSGTYPE_FLAT = 0,
	MM_MSGTYPE_MULTIPART = 1,
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
 * MiniMIME codec object
 */
struct mm_codec
{
	enum mm_encoding id;
	char *encoding;
	char *(*encoder)(char *, u_int32_t);
	char *(*decoder)(char *);
	SLIST_ENTRY(mm_codec) next;
};

struct mm_mimeheader
{
	char *name; 
	char *value;
	char *opaque;
	SLIST_ENTRY(mm_mimeheader) next;
};

/**
 * Represents a MIME Content-Type parameter
 */
struct mm_param
{
	char *name; 
	char *value; 
	SLIST_ENTRY(mm_param) next;
};

/**
 * Represents a MIME Content-Type object
 */
struct mm_content
{
	char *maintype;
	char *subtype;
	struct mm_params params;
	char *encstring;
	enum mm_encoding encoding;
};

/**
 * Represents a MIME part
 */
struct mm_mimepart
{
	struct mm_mimeheaders headers;
	char *copy;
	char *body;
	size_t length;
	struct mm_content *type;
	SLIST_ENTRY(mm_mimepart) next;
};

/**
 * The MiniMIME context.
 */
struct mm_context
{
	unsigned char initialized;
	unsigned char finalized;
	struct mm_mimeparts parts;
	enum mm_messagetype messagetype;
	int has_contenttype;
	struct mm_warnings warnings;
	struct mm_codecs codecs;
};
typedef struct mm_context MM_CTX;
typedef struct mm_context mm_ctx_t;

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
int mm_context_addaddress(MM_CTX *, int, const char *, const char *);
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
struct mm_mimeheader *mm_mimeheader_parse(const char *, int, struct mm_mimeheader **);
struct mm_mimeheader *mm_mimeheader_parsefmt(int, const char *, ...);
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
struct mm_mimeheader *mm_mimepart_getheaderbyname(struct mm_mimepart *, const char *);
const char *mm_mimepart_getheadervalue(struct mm_mimepart *, const char *);
int mm_mimepart_headers_start(struct mm_mimepart *, struct mm_mimeheader **);
struct mm_mimeheader *mm_mimepart_headers_next(struct mm_mimepart *, struct mm_mimeheader **);
char *mm_mimepart_decode(struct mm_mimepart *);
struct mm_content *mm_mimepart_gettype(struct mm_mimepart *);
size_t mm_mimepart_getlength(struct mm_mimepart *);
char *mm_mimepart_getbody(struct mm_mimepart *);
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
