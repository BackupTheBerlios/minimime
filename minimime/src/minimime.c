/*
 * MiniMIME test program
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mm.h"

int
main(int argc, char **argv)
{
	MM_CTX *ctx;
	struct mm_mimeheader *header, *lastheader;
	struct mm_warning *warning, *lastwarning;
	struct mm_mimepart *part;
	struct mm_content *ct;
	int parts, i;

	lastheader = NULL;

	if (argc < 2) {
		printf("USAGE: %s <filename>\n", argv[0]);
		exit(1);
	}
	
#ifdef __HAVE_LEAK_DETECTION
	/* Initialize memory leak detection if compiled in */
	MM_leakd_init();
#endif

	/* Initialize MiniMIME library */
	mm_library_init();

	/* Register all default codecs (base64/qp) */
	mm_codec_registerdefaultcodecs();

	do {
		/* Create a new context */
		ctx = mm_context_new();

		/* Parse a file into our context */
		if (mm_parse_file(ctx, argv[1], MM_PARSE_LOOSE, 0) == -1) {
			printf("ERROR: %s\n", mm_error_string());
			exit(1);
		}

		/* Get the number of MIME parts */
		parts = mm_context_countparts(ctx);
		if (parts == 0) {
			printf("ERROR: got zero MIME parts, huh\n");
			exit(1);
		} else {
			if (mm_context_iscomposite(ctx)) {
				printf("Got %d MIME parts\n", parts - 1);
			} else {
				printf("Flat message (not multipart)\n");
			}
		}

		/* Get the main MIME part */
		part = mm_context_getpart(ctx, 0);
		if (part == NULL) {
			fprintf(stderr, "Could not get envelope part\n");
			exit(1);
		}

		printf("Printing envelope headers:\n");
		/* Print all headers */
		if (mm_mimepart_headers_start(part, &lastheader) == -1) {
			fprintf(stderr, "No headers in envelope\n");
			exit(1);
		}
		while ((header = mm_mimepart_headers_next(part, &lastheader)) != NULL) {
			printf("%s: %s\n", header->name, header->value);
		}

		printf("\n");
		
		ct = part->type;
		assert(ct != NULL);

		/* Loop through all MIME parts beginning with 1 */
		for (i = 1; i < mm_context_countparts(ctx); i++) {
			char *decoded;

			printf("Printing headers for MIME part %d\n", i);

			/* Get the current MIME entity */
			part = mm_context_getpart(ctx, i);
			if (part == NULL) {
				fprintf(stderr, "Should have %d parts but "
				    "couldn't retrieve part %d",
				    mm_context_countparts(ctx), i);
				exit(1);
			}

			/* Print all headers */
			if (mm_mimepart_headers_start(part, &lastheader) == -1) {
				printf("Ups no headers\n");
			}
			while ((header = mm_mimepart_headers_next(part, &lastheader)) != NULL) {
				printf("%s: %s\n", header->name, header->value);
			}

			/* Print MIME part body */
			printf("\nPrinting message BODY:\n%s\n", (char *)part->body);
			decoded = mm_mimepart_decode(part);
			if (decoded != NULL) {
				printf("DECODED:\n%s\n", decoded);
				xfree(decoded);
			}
		}
		
		/* Print out all warnings that we might have received */
		if (mm_context_haswarnings(ctx) > 0) {
			lastwarning = NULL;
			fprintf(stderr, "WARNINGS:\n");
			while ((warning = mm_warning_next(ctx, &lastwarning)) 
			    != NULL) {
				fprintf(stderr, " -> %s\n", warning->message);
			}
		}

		mm_context_free(ctx);
		ctx = NULL;

#ifdef __HAVE_LEAK_DETECTION
		MM_leakd_printallocated();
#endif

	} while (0);

	return 0;
}
