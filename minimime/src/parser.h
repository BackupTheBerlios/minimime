#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

/**
 * Prototypes for functions used by the parser routines
 */
int 	count_lines(char *);
void	mmyyerror(const char *);
int 	dprintf(const char *, ...);
int 	mm_yyparse(void);
int 	mm_yylex(void);
int	mm_yyerror(const char *);

struct s_position
{
	size_t opaque_start;
	size_t start;
	size_t end;
};

#endif /* ! PARSER_H_INCLUDED */
