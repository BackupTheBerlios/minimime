/*
 * $Id: mm_parse.c,v 1.3 2004/06/01 06:51:47 jfi Exp $
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

#include "parser.h"
#include "y.tab.h"

extern FILE *mm_yyin;

/**
 * Parses a NUL-terminated string into a MiniMIME context
 *
 * @param ctx A valid MiniMIME context object
 * @param text The NUL-terminated string to parse
 * @param parseflags A valid combination of parser flags
 * @returns 0 on success or -1 on failure
 * @note Sets mm_errno if an error occurs
 *
 * This function parses a MIME message, stored in the memory region pointed to
 * by text (must be NUL-terminated) according to the parseflags and stores the
 * results in the MiniMIME context specified by ctx.
 *
 * The context needs to be initialized before using mm_context_new() and may
 * be freed using mm_context_free().
 */
int
mm_parse_mem(MM_CTX *ctx, const char *text, int parseflages, int foo)
{
	PARSER_initialize(ctx);
	PARSER_setbuffer(text);
	mm_yyin = NULL;
	return mm_yyparse();
}

int
mm_parse_file(MM_CTX *ctx, const char *filename, int parseflags, int foo)
{
	FILE *fp;

	if ((fp = fopen(filename, "r")) == NULL) {
		mm_errno = MM_ERROR_ERRNO;
		return -1;
	}
	
	mm_yyin = fp;
	PARSER_initialize(ctx);

	return mm_yyparse();
}
