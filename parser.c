/***************************************************************

  Copyright (C) DSTC Pty Ltd (ACN 052 372 577) 1995.
  Unpublished work.  All Rights Reserved.

  The software contained on this media is the property of the
  DSTC Pty Ltd.  Use of this software is strictly in accordance
  with the license agreement in the accompanying LICENSE.DOC
  file.  If your distribution of this software does not contain
  a LICENSE.DOC file then you have no rights to use this
  software in any manner and should contact DSTC at the address
  below to determine an appropriate licensing arrangement.

     DSTC Pty Ltd
     Level 7, Gehrmann Labs
     University of Queensland
     St Lucia, 4072
     Australia
     Tel: +61 7 3365 4310
     Fax: +61 7 3365 4311
     Email: enquiries@dstc.edu.au

  This software is being provided "AS IS" without warranty of
  any kind.  In no event shall DSTC Pty Ltd be liable for
  damage of any kind arising out of or in connection with
  the use or performance of this software.

****************************************************************/

#ifndef lint
static const char cvsid[] = "$Id: parser.c,v 2.3 2000/07/05 07:59:28 phelps Exp $";
#endif /* lint */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <elvin/elvin.h>
#include <elvin/memory.h>

#define INITIAL_TOKEN_BUFFER_SIZE 64
#define INITIAL_STACK_DEPTH 10


/* Test ch to see if it's valid as a non-initial ID character */
static int is_id_char(int ch)
{
    static char table[] =
    {
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /* 0x00 */
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /* 0x10 */
	0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 1, 1,  0, 1, 1, 1, /* 0x20 */
	1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 0, 0,  1, 1, 1, 1, /* 0x30 */
	1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, /* 0x40 */
	1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1, /* 0x50 */
	1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, /* 0x60 */
	1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 0  /* 0x70 */
    };

    /* Use a table for quick lookup of those tricky symbolic chars */
    return (ch < 0 || ch > 127) ? 0 : table[ch];
}


/* FIX THIS: this goes in parser.h */
/* The parser type */
typedef struct parser *parser_t;

/* The lexer_state_t type */
typedef int (*lexer_state_t)(parser_t parser, int ch, elvin_error_t error);

/* The structure of the configuration file parser */
struct parser
{
    /* The tag for error messages */
    char *tag;

    /* The current line number */
    int line_num;

    /* The state stack */
    int *state_stack;

    /* The top of the state stack */
    int *state_top;

    /* The end of the state stack */
    int *state_end;

    /* The value stack */
    void **value_stack;

    /* The top of the value stack */
    void **value_top;

    /* The current lexical state */
    lexer_state_t state;

    /* The token construction buffer */
    char *token;

    /* The next character in the token buffer */
    char *point;

    /* The end of the token buffer */
    char *token_end;


    /* The callback for when we've completed a subscription entry */
/*    parser_callback_t callback;*/
    void *callback;
    
    /* The client-supplied arg for the callback */
    void *rock;
};


/* Reduction function declarations */
typedef void *(*reduction_t)(parser_t self, elvin_error_t error);
static void *function(parser_t self, elvin_error_t error);

/* Lexer state function headers */
static int lex_start(parser_t self, int ch, elvin_error_t error);
static int lex_comment(parser_t self, int ch, elvin_error_t error);
static int lex_bang(parser_t self, int ch, elvin_error_t error);
static int lex_ampersand(parser_t self, int ch, elvin_error_t error);
static int lex_eq(parser_t self, int ch, elvin_error_t error);
static int lex_lt(parser_t self, int ch, elvin_error_t error);
static int lex_gt(parser_t self, int ch, elvin_error_t error);
static int lex_vbar(parser_t self, int ch, elvin_error_t error);
static int lex_zero(parser_t self, int ch, elvin_error_t error);
static int lex_decimal(parser_t self, int ch, elvin_error_t error);
static int lex_float_first(parser_t self, int ch, elvin_error_t error);
static int lex_dq_string(parser_t self, int ch, elvin_error_t error);
static int lex_dq_string_esc(parser_t self, int ch, elvin_error_t error);
static int lex_sq_string(parser_t self, int ch, elvin_error_t error);
static int lex_sq_string_esc(parser_t self, int ch, elvin_error_t error);
static int lex_id(parser_t self, int ch, elvin_error_t error);
static int lex_id_esc(parser_t self, int ch, elvin_error_t error);

/* Include the parser tables */
#include "grammar.h"

/* Makes a deeper stack */
static int grow_stack(parser_t self, elvin_error_t error)
{
    fprintf(stderr, "grow_stack: not yet implemented\n");
    abort();
}

/* Pushes a state and value onto the stack */
static int push(parser_t self, int state, void *value, elvin_error_t error)
{
    /* Make sure there's enough room on the stack */
    if (! (self -> state_top + 1 < self -> state_end))
    {
	if (! grow_stack(self, error))
	{
	    return 0;
	}
    }

    /* The state stack is pre-increment */
    *(++self->state_top) = state;

    /* The value stack is post-increment */
    *(self->value_top++) = value;
    return 1;
}

/* Moves the top of the stack back `count' positions */
static void pop(parser_t self, int count, elvin_error_t error)
{
#ifdef DEBUG
    /* Sanity check */
    if (self -> state_stack > self -> state_top - count)
    {
	fprintf(stderr, "popped off the top of the stack\n");
	abort();
    }
#endif

    /* Adjust the appropriate pointers */
    self -> state_top -= count;
    self -> value_top -= count;
}

/* Puts n elements back onto the stack */
static void unpop(parser_t self, int count, elvin_error_t error)
{
    fprintf(stderr, "unpop: not yet implemented\n");
    abort();
}

/* Answers the top of the state stack */
static int top(parser_t self)
{
    return *(self->state_top);
}

/* Frees everything on the stack */
static void clean_stack(parser_t self, elvin_error_t error)
{
    fprintf(stderr, "clean_stack: not yet implemented\n");
    abort();
}

/* bogus function */
static void *function(parser_t self, elvin_error_t error)
{
    printf("function() called\n");
}


/* Accepts another token and performs as many parser transitions as
 * possible with the data it has seen so far */
static int shift_reduce(parser_t self, terminal_t terminal, void *value, elvin_error_t error)
{
    int action;
    struct production *production;
    int reduction;
    void *result;

    /* Reduce as many times as possible */
    while (IS_REDUCE(action = sr_table[top(self)][terminal]))
    {
	/* Locate the production rule to use */
	reduction = REDUCTION(action);
	production = productions + reduction;

	/* Point the stack at the beginning of the components */
	pop(self, production -> count, error);

	/* Reduce by calling the production's reduction function */
	if (! (result = production -> reduction(self, error)))
	{
	    /* Something went wrong -- put the args back on the stack */
	    unpop(self, production -> count, error);
	    return 0;
	}

	/* Push the result back onto the stack */
	if (! push(self, REDUCE_GOTO(top(self), production), result, error))
	{
	    return 0;
	}
    }

    /* See if we can shift */
    if (IS_SHIFT(action))
    {
	return push(self, SHIFT_GOTO(action), value, error);
    }

    /* See if we can accept */
    if (IS_ACCEPT(action))
    {
	return 1;
    }

    /* Everything else is an error */
    if (value == NULL)
    {
	fprintf(stderr, "Houston, we have a problem (1)\n");
	abort();
    }

    fprintf(stderr, "houston, we have a problem (2)\n");
    abort();
}

/* Accepts an AND token */
static int accept_and(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_AND, NULL, error);
}

/* Accepts an ASSIGN token */
static int accept_assign(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_ASSIGN, NULL, error);
}

/* Accepts a BANG token */
static int accept_bang(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_BANG, NULL, error);
}

/* Accepts a COLON token */
static int accept_colon(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_COLON, NULL, error);
}

/* Accepts an COMMA token */
static int accept_comma(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_COMMA, NULL, error);
}

/* Accepts an EOF token */
static int accept_eof(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_EOF, NULL, error);
}

/* Accepts an EQ token */
static int accept_eq(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_EQ, NULL, error);
}

/* Accepts an GE token */
static int accept_ge(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_GE, NULL, error);
}

/* Accepts an GT token */
static int accept_gt(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_GT, NULL, error);
}

/* Accepts an ID token */
static int accept_id(parser_t self, char *name, elvin_error_t error)
{
    return shift_reduce(self, TT_ID, NULL, error);
}

/* Accepts an INT32 token */
static int accept_int32(parser_t self, int32_t value, elvin_error_t error)
{
    return shift_reduce(self, TT_INT32, NULL, error);
}

/* Accepts a string as an int32 token */
static int accept_int32_string(parser_t self, char *string, elvin_error_t error)
{
    fprintf(stderr, "accept_int32_string(): not yet implemented\n");
    abort();
}

/* Accepts an INT64 token */
static int accept_int64(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_INT64, NULL, error);
}

/* Accepts a string as an int64 token */
static int accept_int64_string(parser_t self, char *string, elvin_error_t error)
{
    fprintf(stderr, "accept_int64_string(): not yet implemented\n");
    abort();
}

/* Accepts an LBRACE token */
static int accept_lbrace(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_LBRACE, NULL, error);
}

/* Accepts an LBRACKET token */
static int accept_lbracket(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_LBRACKET, NULL, error);
}

/* Accepts an LE token */
static int accept_le(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_LE, NULL, error);
}

/* Accepts an LT token */
static int accept_lt(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_LT, NULL, error);
}

/* Accepts an LPAREN token */
static int accept_lparen(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_LPAREN, NULL, error);
}

/* Accepts an NEQ token */
static int accept_neq(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_NEQ, NULL, error);
}

/* Accepts an OR token */
static int accept_or(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_OR, NULL, error);
}

/* Accepts an RBRACE token */
static int accept_rbrace(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_RBRACE, NULL, error);
}

/* Accepts an RBRACKET token */
static int accept_rbracket(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_RBRACKET, NULL, error);
}

/* Accepts an REAL64 token */
static int accept_real64(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_REAL64, NULL, error);
}

/* Accepts an RPAREN token */
static int accept_rparen(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_RPAREN, NULL, error);
}

/* Accepts a SEMI token */
static int accept_semi(parser_t self, elvin_error_t error)
{
    return shift_reduce(self, TT_SEMI, NULL, error);
}

/* Accepts an STRING token */
static int accept_string(parser_t self, char *string, elvin_error_t error)
{
    return shift_reduce(self, TT_STRING, NULL, error);
}



/* Expands the token buffer */
static int grow_buffer(parser_t self, elvin_error_t error)
{
    size_t length = (self -> token_end - self -> token) * 2;
    char *token;

    /* Make the buffer bigger */
    if (! (token = (char *)ELVIN_REALLOC(self -> token, length, error)))
    {
	return 0;
    }

    /* Update the pointers */
    self -> point = self -> point - self -> token + token;
    self -> token_end = token + length;
    self -> token = token;
    return 1;
}

/* Appends a character to the end of the token */
static int append_char(parser_t self, int ch, elvin_error_t error)
{
    /* Make sure there's room */
    if (! (self -> point < self -> token_end))
    {
	if (! grow_buffer(self, error))
	{
	    return 0;
	}
    }

    *(self -> point++) = ch;
    return 1;
}

/* Handle the first character of a new token */
static int lex_start(parser_t self, int ch, elvin_error_t error)
{
    switch (ch)
    {
	/* The end of the input file */
	case EOF:
	{
	    if (! accept_eof(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* BANG or NEQ */
	case '!':
	{
	    self -> state = lex_bang;
	    return 1;
	}

	/* COMMENT */
	case '#':
	{
	    self -> point = self -> token;
	    return lex_comment(self, ch, error);
	}
	

	/* Double-quoted string */
	case '"':
	{
	    self -> point = self -> token;
	    self -> state = lex_dq_string;
	    return 1;
	}

	/* AND */
	case '&':
	{
	    self -> state = lex_ampersand;
	    return 1;
	}

	/* Single-quoted string */
	case '\'':
	{
	    self -> point = self -> token;
	    self -> state = lex_sq_string;
	    return 1;
	}

	/* LPAREN */
	case '(':
	{
	    if (! accept_lparen(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* RPAREN */
	case ')':
	{
	    if (! accept_rparen(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* COMMA */
	case ',':
	{
	    if (! accept_comma(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* Zero can lead to many different kinds of numbers */
	case '0':
	{
	    self -> point = self -> token;
	    if (! append_char(self, ch, error))
	    {
		return 0;
	    }

	    self -> state = lex_zero;
	    return 1;
	}

	/* COLON */
	case ':':
	{
	    if (! accept_colon(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* SEMI */
	case ';':
	{
	    if (! accept_semi(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* LT or LE */
	case '<':
	{
	    self -> state = lex_lt;
	    return 1;
	}

	/* ASSIGN or EQ */
	case '=':
	{
	    self -> state = lex_eq;
	    return 1;
	}

	/* GT or GE */
	case '>':
	{
	    self -> state = lex_gt;
	    return 1;
	}

	/* LBRACKET */
	case '[':
	{
	    if (! accept_lbracket(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* ID with quoted first character */
	case '\\':
	{
	    self -> point = self -> token;
	    self -> state = lex_id_esc;
	    return 1;
	}

	/* RBRACKET */
	case ']':
	{
	    if (! accept_rbracket(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* ID */
	case '_':
	{
	    self -> point = self -> token;
	    if (! append_char(self, ch, error))
	    {
		return 0;
	    }

	    self -> state = lex_id;
	    return 1;
	}

	/* OR */
	case '|':
	{
	    self -> state = lex_vbar;
	    return 1;
	}

	/* LBRACE */
	case '{':
	{
	    if (! accept_lbrace(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* RBRACE */
	case '}':
	{
	    if (! accept_rbrace(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}
    }

    /* Ignore whitespace */
    if (isspace(ch))
    {
	self -> state = lex_start;
	return 1;
    }

    /* Watch for a number */
    if (isdigit(ch))
    {
	self -> point = self -> token;
	if (! append_char(self, ch, error))
	{
	    return 0;
	}

	self -> state = lex_decimal;
	return 1;
    }

    /* Watch for identifiers */
    if (isalpha(ch))
    {
	self -> point = self -> token;
	if (! append_char(self, ch, error))
	{
	    return 0;
	}

	self -> state = lex_id;
	return 1;
    }

    /* Anything else is bogus */
    printf("bad token character: `%c'\n", ch);
    return 0;
}


/* Reading a line comment */
static int lex_comment(parser_t self, int ch, elvin_error_t error)
{
    /* Watch for the end of file */
    if (ch == EOF)
    {
	return lex_start(self, ch, error);
    }

    /* Watch for the end of the line */
    if (ch == '\n')
    {
	self -> state = lex_start;
	return 1;
    }

    /* Ignore everything else */
    self -> state = lex_comment;
    return 1;
}

static int lex_bang(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_bang: not yet implemented\n");
    abort();
}

/* Reading the character after a `&' */
static int lex_ampersand(parser_t self, int ch, elvin_error_t error)
{
    /* Another `&' is an AND */
    if (ch == '&')
    {
	if (! accept_and(self, error))
	{
	    return 0;
	}

	self -> state = lex_start;
	return 1;
    }

    /* Otherwise we've got a problem */
    fprintf(stderr, "Bad token: `&'\n");
    abort();
}

/* Reading the character after an initial `=' */
static int lex_eq(parser_t self, int ch, elvin_error_t error)
{
    /* Is there an other `=' for an EQ token? */
    if (ch == '=')
    {
	if (! accept_eq(self, error))
	{
	    return 0;
	}

	self -> state = lex_start;
	return 1;
    }

    /* Nope.  This must be an ASSIGN. */
    if (! accept_assign(self, error))
    {
	return 0;
    }

    return lex_start(self, ch, error);
}

/* Reading the character after a `<' */
static int lex_lt(parser_t self, int ch, elvin_error_t error)
{
    switch (ch)
    {
	/* LE */
	case '=':
	{
	    if (! accept_le(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* Anything else is the next character */
	default:
	{
	    if (! accept_lt(self, error))
	    {
		return 0;
	    }

	    return lex_start(self, ch, error);
	}
    }
}

/* Reading the character after a `>' */
static int lex_gt(parser_t self, int ch, elvin_error_t error)
{
    switch (ch)
    {
	/* GE */
	case '=':
	{
	    if (! accept_ge(self, error))
	    {
		return 0;
	    }

	    self -> state = lex_start;
	    return 1;
	}

	/* GT */
	default:
	{
	    if (! accept_gt(self, error))
	    {
		return 0;
	    }

	    return lex_start(self, ch, error);
	}
    }
}

/* Reading the character after a `|' */
static int lex_vbar(parser_t self, int ch, elvin_error_t error)
{
    /* Another `|' is an OR token */
    if (ch == '|')
    {
	if (! accept_or(self, error))
	{
	    return 0;
	}

	self -> state = lex_start;
	return 1;
    }

    /* We're not doing simple math yet */
    fprintf(stderr, "bogus token: `|'\n");
    abort();
}

static int lex_zero(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_zero: not yet implemented\n");
    abort();
}

/* We've read one or more digits of a decimal number */
static int lex_decimal(parser_t self, int ch, elvin_error_t error)
{
    /* Watch for additional digits */
    if (isdigit(ch))
    {
	if (! append_char(self, ch, error))
	{
	    return 0;
	}

	self -> state = lex_decimal;
	return 1;
    }

    /* Watch for a decimal point */
    if (ch == '.')
    {
	if (! append_char(self, ch, error))
	{
	    return 0;
	}

	self -> state = lex_float_first;
	return 1;
    }

    /* Null-terminate the number */
    if (! append_char(self, 0, error))
    {
	return 0;
    }

    /* Watch for a trailing 'L' to indicate an int64 value */
    if (ch == 'l' || ch == 'L')
    {
	if (! accept_int64_string(self, self -> token, error))
	{
	    return 0;
	}

	self -> state = lex_start;
	return 1;
    }

    /* Otherwise accept the int32 token */
    if (! accept_int32_string(self, self -> token, error))
    {
	return 0;
    }

    return lex_start(self, ch, error);
}


/* Reading the first digit of the decimal portion of a floating point number */
static int lex_float_first(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_float_first: not yet implemented\n");
    abort();
}

/* Reading the characters of a double-quoted string */
static int lex_dq_string(parser_t self, int ch, elvin_error_t error)
{
    /* Watch for special characters */
    switch (ch)
    {
	/* Unterminated string constant */
	case EOF:
	{
	    fprintf(stderr, "Unterminated string constant\n");
	    abort();
	}

	/* An escaped character */
	case '\\':
	{
	    self -> state = lex_dq_string_esc;
	    return 1;
	}

	/* End of the string */
	case '"':
	{
	    /* Null-terminate the string */
	    if (! append_char(self, 0, error))
	    {
		return 0;
	    }

	    /* Accept the string */
	    if (! accept_string(self, self -> token, error))
	    {
		return 0;
	    }

	    /* Go back to the start state */
	    self -> state = lex_start;
	    return 1;
	}

	/* Normal string character */
	default:
	{
	    if (! append_char(self, ch, error))
	    {
		return 0;
	    }

	    self -> state = lex_dq_string;
	    return 1;
	}
    }
}

/* Reading an escaped character in a double-quoted string */
static int lex_dq_string_esc(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_dq_string_esc: not yet implemented\n");
    abort();
}

/* Reading the characters of a single-quoted string */
static int lex_sq_string(parser_t self, int ch, elvin_error_t error)
{
    /* Watch for special characters */
    switch (ch)
    {
	/* Unterminated string constant */
	case EOF:
	{
	    fprintf(stderr, "Unterminated string constant\n");
	    abort();
	}

	/* An escaped character */
	case '\\':
	{
	    self -> state = lex_sq_string_esc;
	    return 1;
	}

	/* End of the string */
	case '\'':
	{
	    /* Null-terminate the string */
	    if (! append_char(self, 0, error))
	    {
		return 0;
	    }

	    /* Accept the string */
	    if (! accept_string(self, self -> token, error))
	    {
		return 0;
	    }

	    /* Go back to the start state */
	    self -> state = lex_start;
	    return 1;
	}

	/* Normal string character */
	default:
	{
	    if (! append_char(self, ch, error))
	    {
		return 0;
	    }

	    self -> state = lex_sq_string;
	    return 1;
	}
    }
}

/* Reading an escaped character in a single-quoted string */
static int lex_sq_string_esc(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_sq_string_esc: not yet implemented\n");
    abort();
}

/* Reading an identifier (2nd character on) */
static int lex_id(parser_t self, int ch, elvin_error_t error)
{
    /* Watch for an escaped id char */
    if (ch == '\\')
    {
	self -> state = lex_id_esc;
	return 1;
    }

    /* Watch for additional id characters */
    if (is_id_char(ch))
    {
	if (! append_char(self, ch, error))
	{
	    return 0;
	}

	self -> state = lex_id;
	return 1;
    }

    /* This is the end of the id */
    if (! append_char(self, 0, error))
    {
	return 0;
    }

    /* Accept it */
    if (! accept_id(self, self -> token, error))
    {
	return 0;
    }

    /* Run ch through the start state */
    return lex_start(self, ch, error);
}

static int lex_id_esc(parser_t self, int ch, elvin_error_t error)
{
    fprintf(stderr, "lex_id_esc: not yet implemented\n");
    abort();
}

void parser_free(parser_t self, elvin_error_t error);

/* Allocate space for a parser and initialize its contents */
parser_t parser_alloc(elvin_error_t error)
{
    parser_t self;

    /* Allocate some memory for the new parser */
    if (! (self = (parser_t)ELVIN_MALLOC(sizeof(struct parser), error)))
    {
	return NULL;
    }

    /* Initialize all of the fields to sane values */
    memset(self, 0, sizeof(struct parser));

    /* Allocate room for the state stack */
    if (! (self -> state_stack = (int *)ELVIN_CALLOC(INITIAL_STACK_DEPTH, sizeof(int), error)))
    {
	parser_free(self, error);
	return NULL;
    }

    /* Allocate room for the value stack */
    if (! (self -> value_stack = (void *)ELVIN_CALLOC(INITIAL_STACK_DEPTH, sizeof(void *), error)))
    {
	parser_free(self, error);
	return NULL;
    }

    /* Allocate room for the token buffer */
    if (! (self -> token = (char *)ELVIN_MALLOC(INITIAL_TOKEN_BUFFER_SIZE, error)))
    {
	parser_free(self, error);
	return NULL;
    }

    /* Initialize the rest of the values */
    self -> state_end = self -> state_stack + INITIAL_STACK_DEPTH;
    self -> state_top = self -> state_stack;
    *(self -> state_top) = 0;

    self -> value_top = self -> value_stack;
    self -> token_end = self -> token + INITIAL_TOKEN_BUFFER_SIZE;
    self -> line_num = 1;
    self -> state = lex_start;
    return self;
}

/* Frees the resources consumed by the parser */
void parser_free(parser_t self, elvin_error_t error)
{
    /* Free the token buffer */
    if (self -> token)
    {
	free(self -> token);
    }

    free(self);
}


/* Parses a single character, counting LFs */
static int parse_char(parser_t self, int ch, elvin_error_t error)
{
    if (! self -> state(self, ch, error))
    {
	return 0;
    }

    if (ch == '\n')
    {
	self -> line_num++;
    }

    return 1;
}

/* Runs the characters in the buffer through the parser */
int parser_parse(parser_t self, char *buffer, size_t length, elvin_error_t error)
{
    char *pointer;
    int ch;

    /* Length of 0 is an EOF */
    if (length == 0)
    {
	return parse_char(self, EOF, error);
    }

    for (pointer = buffer; pointer < buffer + length; pointer++)
    {
	if (! parse_char(self, *pointer, error))
	{
	    return 0;
	}
    }

    return 1;
}


/* For testing purposes */
int main(int argc, char *argv[])
{
    elvin_error_t error = NULL;
    parser_t parser;
    char buffer[2048];
    size_t length;
    int fd;

    if ((parser = parser_alloc(error)) == NULL)
    {
	fprintf(stderr, "parser_alloc(): failed\n");
	abort();
    }

    /* Read stdin and put it through the parser */
    fd = STDIN_FILENO;
    while (1)
    {
	if ((length = read(fd, buffer, 2048)) < 0)
	{
	    perror("read(): failed");
	    exit(1);
	}

	if (! parser_parse(parser, buffer, length, error))
	{
	    exit(1);
	}

	if (length < 1)
	{
	    printf("-- end of input --\n");
	    exit(0);
	}
    }
}
