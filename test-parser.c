#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <elvin/elvin.h>
#include <elvin/memory.h>
#include <elvin/errors/elvin.h>
#include "errors.h"
#include "sexp.h"
#include "parser.h"

env_t root_env = NULL;

static int prim_lambda(env_t env, sexp_t args, sexp_t *result, elvin_error_t error);

/* Parser callback */
int parsed(void *rock, parser_t parser, sexp_t sexp, elvin_error_t error)
{
    sexp_t result;

    /* Evaluate the s-expression */
    if (sexp_eval(sexp, root_env, &result, error) == 0)
    {
	elvin_error_fprintf(stderr, error);
    }
    else
    {
	/* Print it */
	sexp_print(result); printf("\n");

	/* Free it */
	if (sexp_free(result, error) == 0)
	{
	    return 0;
	}
    }

    printf("> "); fflush(stdout);
    return 1;
}

/* Parse a file */
static int parse_file(parser_t parser, int fd, char *filename, elvin_error_t error)
{
    char buffer[4096];
    ssize_t length;

    /* Keep reading until we run dry */
    while (1)
    {
	/* Read some of the file */
	if ((length = read(fd, buffer, 4096)) < 0)
	{
	    perror("read(): failed");
	    exit(1);
	}

	/* Run it through the parser */
	if (parser_read_buffer(parser, buffer, length, error) == 0)
	{
	    return 0;
	}

	/* See if that was the end of the file */
	if (length == 0)
	{
	    return 1;
	}
    }
}


/* Evaluate a list of args and put the results into an array */
static int eval_args(
    env_t env,
    sexp_t args,
    sexp_t *values,
    uint32_t count,
    elvin_error_t error)
{
    uint32_t i;

    /* Go through the args */
    for (i = 0; i < count; i++)
    {
	sexp_t arg;

	/* Make sure we have an arg */
	if (sexp_get_type(args) != SEXP_CONS)
	{
	    ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	    while (i > 0)
	    {
		sexp_free(values[i - 1], NULL);
		i--;
	    }

	    return 0;
	}

	/* Extract the arg, evaluate it and move on to the next */
	if ((arg = cons_car(args, error)) == NULL ||
	    sexp_eval(arg, env, values + i, error) == 0 ||
	    (args = cons_cdr(args, error)) == 0)
	{
	    while (i > 0)
	    {
		sexp_free(values[i - 1], NULL);
		i--;
	    }

	    return 0;
	}
    }

    /* Make sure there are no more args */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too many args");
	while (i > 0)
	{
	    sexp_free(values[i - 1], NULL);
	    i--;
	}

	return 0;
    }

    return 1;
}

/* Extract the arguments to a function without evaluating them */
static int extract_args(
    env_t env,
    sexp_t args,
    sexp_t *args_out,
    uint32_t count,
    elvin_error_t error)
{
    uint32_t i;

    /* Go through the args */
    for (i = 0; i < count; i++)
    {
	/* Make sure we have an arg */
	if (sexp_get_type(args) != SEXP_CONS)
	{
	    ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	    return 0;
	}

	/* Extract the arg and move on to the next */
	if ((args_out[i] = cons_car(args, error)) == NULL ||
	    (args = cons_cdr(args, error)) == NULL)
	{
	    return 0;
	}
    }

    /* Make sure there are no more args */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too many args");
	return 0;
    }

    return 1;
}

/* The `and' primitive function */
static int prim_and(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    /* Assume true */
    *result = symbol_alloc("t", error);

    /* Evaluate each arg until we get a nil or run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	/* Free the old value */
	sexp_free(*result, error);

	/* Evaluate the arg */
	if (sexp_eval(cons_car(args, error), env, result, error) == 0)
	{
	    return 0;
	}

	/* If it's nil then return nil */
	if (sexp_get_type(*result) == SEXP_NIL)
	{
	    return 1;
	}

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure the arg list ends nicely */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	sexp_free(*result, NULL);
	return 0;
    }

    /* We're done */
    return 1;
}

/* The `car' primitive function */
static int prim_car(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t value;

    /* Evaluate the arg */
    if (! eval_args(env, args, &value, 1, error))
    {
	return 0;
    }

    /* Make sure the cons really is a cons cell */
    if (sexp_get_type(value) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad cons");
	return 0;
    }

    /* Extract the car of the cons cell */
    if ((*result = cons_car(value, error)) == NULL)
    {
	return 0;
    }

    /* Free our reference to the cons cell */
    if (sexp_free(value, error) == 0)
    {
	return 0;
    }

    /* Grab a reference to it */
    return sexp_alloc_ref(*result, error);
}

/* The `car' primitive function */
static int prim_cdr(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t value;

    /* Evaluate the arg */
    if (! eval_args(env, args, &value, 1, error))
    {
	return 0;
    }

    /* Make sure the cons really is a cons cell */
    if (sexp_get_type(value) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad cons");
	return 0;
    }

    /* Extract the car of the cons cell */
    if ((*result = cons_cdr(value, error)) == NULL)
    {
	return 0;
    }

    /* Free our reference to the cons cell */
    if (sexp_free(value, error) == 0)
    {
	return 0;
    }

    /* Grab a reference to it */
    return sexp_alloc_ref(*result, error);
}

/* The `cons' primitive function */
static int prim_cons(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t values[2];

    /* Evaluate the args */
    if (! eval_args(env, args, values, 2, error))
    {
	return 0;
    }

    /* Construct a cons cell */
    if ((*result = cons_alloc(values[0], values[1], error)) == NULL)
    {
	return 0;
    }

    /* Retain our reference to the car and cdr */
    return 1;
}

/* The `defun' primitive function */
static int prim_defun(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t symbol;

    /* Make sure we've got a cons */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	return 0;
    }

    /* Extract the function name */
    if ((symbol = cons_car(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure it's a symbol */
    if (sexp_get_type(symbol) != SEXP_SYMBOL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad symbol");
	return 0;
    }

    /* Treat the rest of the expression as a lambda */
    if (prim_lambda(env, cons_cdr(args, error), result, error) == 0)
    {
	return 0;
    }

    /* Assign the lambda to the function name */
    return env_assign(env, symbol, *result, error);
}

/* The `eq' primitive function */
static int prim_eq(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t values[2];
    int match = 0;

    /* Evaluate the args */
    if (! eval_args(env, args, values, 2, error))
    {
	return 0;
    }

    /* If the values are identical then return true */
    if (values[0] == values[1])
    {
	match = 1;
    }
    else
    {
	/* Numbers are more difficult */
	switch (sexp_get_type(values[0]))
	{
	    case SEXP_INT32:
	    {
		switch (sexp_get_type(values[1]))
		{
		    case SEXP_INT32:
		    {
			match = int32_value(values[0], error) == int32_value(values[1], error);
			break;
		    }

		    case SEXP_INT64:
		    {
			match = int32_value(values[0], error) == int64_value(values[1], error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			match = int32_value(values[0], error) == float_value(values[1], error);
			break;
		    }

		    default:
		    {
			match = 0;
		    }
		}

		break;
	    }

	    case SEXP_INT64:
	    {
		switch (sexp_get_type(values[1]))
		{
		    case SEXP_INT32:
		    {
			match = int64_value(values[0], error) == int32_value(values[1], error);
			break;
		    }

		    case SEXP_INT64:
		    {
			match = int64_value(values[0], error) == int64_value(values[1], error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			match = int64_value(values[0], error) == float_value(values[1], error);
			break;
		    }

		    default:
		    {
			match = 0;
		    }
		}

		break;
	    }

	    case SEXP_FLOAT:
	    {
		switch (sexp_get_type(values[1]))
		{
		    case SEXP_INT32:
		    {
			match = float_value(values[0], error) == int32_value(values[1], error);
			break;
		    }

		    case SEXP_INT64:
		    {
			match = float_value(values[0], error) == int64_value(values[1], error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			match = float_value(values[0], error) == float_value(values[1], error);
			break;
		    }

		    default:
		    {
			match = 0;
		    }
		}

		break;
	    }

	    default:
	    {
		match = 0;
	    }
	}
    }

    /* Construct our result */
    if ((*result = match ? symbol_alloc("t", error) : nil_alloc(error)) == NULL)
    {
	return 0;
    }

    /* Free the arg values */
    if (sexp_free(values[0], error) == 0)
    {
	return 0;
    }

    return sexp_free(values[1], error);
}

/* The `if' primitive function */
static int prim_if(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t values[3];
    sexp_t value;

    /* Extract the args */
    if (extract_args(env, args, values, 3, error) == 0)
    {
	return 0;
    }


    /* Evaluate the test */
    if (sexp_eval(values[0], env, &value, error) == 0)
    {
	return 0;
    }

    /* If it's non-nil then evaluate the true branch */
    if (sexp_get_type(value) != SEXP_NIL)
    {
	if (sexp_eval(values[1], env, result, error) == 0)
	{
	    sexp_free(value, NULL);
	    return 0;
	}
    }
    else
    {
	if (sexp_eval(values[2], env, result, error) == 0)
	{
	    sexp_free(value, NULL);
	    return 0;
	}
    }

    /* Free the test's result */
    return sexp_free(value, error);
}

/* The `lambda' primitive function */
static int prim_lambda(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t arg_list;
    sexp_t body;

    /* Make sure we have at least one arg */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	return 0;
    }

    /* Extract it */
    if ((arg_list = cons_car(args, error)) == NULL)
    {
	return 0;
    }

    /* Move on to the next arg */
    if ((body = cons_cdr(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure that we have a valid body */
    if (sexp_get_type(body) != SEXP_NIL && sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad thingo");
	return 0;
    }

    /* Grab references to the env, arg list and body */
    if (env_alloc_ref(env, error) == 0 ||
	sexp_alloc_ref(arg_list, error) == 0 ||
	sexp_alloc_ref(body, error) == 0)
    {
	return 0;
    }

    /* Create a lambda node from the args and body */
    if ((*result = lambda_alloc(env, arg_list, body, error)) == NULL)
    {
	return 0;
    }

    return 1;
}

/* The `and' primitive function */
static int prim_or(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    /* Assume false */
    *result = nil_alloc(error);

    /* Evaluate each arg until we get a non-nil one or run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	/* Free the old result */
	sexp_free(*result, error);

	/* Evaluate the arg */
	if (sexp_eval(cons_car(args, error), env, result, error) == 0)
	{
	    return 0;
	}

	/* If it's non-nil then we're done */
	if (sexp_get_type(*result) != SEXP_NIL)
	{
	    return 1;
	}

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure the list ends nicely */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	sexp_free(*result, NULL);
	return 0;
    }

    return 1;
}

/* The `+' primitive function */
static int prim_plus(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t sum;

    /* Start with a sum of zero */
    if ((sum = int32_alloc(0, error)) == NULL)
    {
	return 0;
    }

    /* Keep going until we run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	sexp_t value;

	/* Evaluate the car */
	if (sexp_eval(cons_car(args, error), env, &value, error) == 0)
	{
	    sexp_free(value, NULL);
	    return 0;
	}

	/* Figure out what to do based on the type of the first arg */
	switch (sexp_get_type(sum))
	{
	    case SEXP_INT32:
	    {
		int32_t lhs = int32_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int32_alloc(lhs + int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs + int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs + float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_INT64:
	    {
		int64_t lhs = int64_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int64_alloc(lhs + int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs + int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs + float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_FLOAT:
	    {
		double lhs = float_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = float_alloc(lhs + int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = float_alloc(lhs + int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs + float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    default:
	    {
		ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
		*result = NULL;
		break;
	    }
	}

	/* Free the value */
	if (sexp_free(value, error) == 0)
	{
	    return 0;
	}

	/* Check for an error */
	if (*result == NULL)
	{
	    return 0;
	}

	/* The result becomes the sum */
	sum = *result;

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure we end cleanly */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	return 0;
    }

    *result = sum;
    return 1;
}

/* The `-' primitive function */
static int prim_minus(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t sum;

    /* Start with a sum of zero */
    if ((sum = int32_alloc(0, error)) == NULL)
    {
	return 0;
    }

    /* Keep going until we run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	sexp_t value;

	/* Evaluate the car */
	if (sexp_eval(cons_car(args, error), env, &value, error) == 0)
	{
	    sexp_free(value, NULL);
	    return 0;
	}

	/* Figure out what to do based on the type of the first arg */
	switch (sexp_get_type(sum))
	{
	    case SEXP_INT32:
	    {
		int32_t lhs = int32_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int32_alloc(lhs - int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs - int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs - float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_INT64:
	    {
		int64_t lhs = int64_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int64_alloc(lhs - int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs - int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs - float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_FLOAT:
	    {
		double lhs = float_value(sum, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = float_alloc(lhs - int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = float_alloc(lhs - int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs - float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    default:
	    {
		ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
		*result = NULL;
		break;
	    }
	}

	/* Free the value */
	if (sexp_free(value, error) == 0)
	{
	    return 0;
	}

	/* Check for an error */
	if (*result == NULL)
	{
	    return 0;
	}

	/* The result becomes the sum */
	sum = *result;

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure we end cleanly */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	return 0;
    }

    *result = sum;
    return 1;
}

/* The `*' primitive function */
static int prim_times(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t product;

    /* Start with a product of one */
    if ((product = int32_alloc(1, error)) == NULL)
    {
	return 0;
    }

    /* Keep going until we run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	sexp_t value;

	/* Evaluate the car */
	if (sexp_eval(cons_car(args, error), env, &value, error) == 0)
	{
	    sexp_free(product, NULL);
	    return 0;
	}

	/* Figure out what to do based on the type of the first arg */
	switch (sexp_get_type(product))
	{
	    case SEXP_INT32:
	    {
		int32_t lhs = int32_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int32_alloc(lhs * int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs * int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs * float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_INT64:
	    {
		int64_t lhs = int64_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int64_alloc(lhs * int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs * int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs * float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_FLOAT:
	    {
		double lhs = float_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = float_alloc(lhs * int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = float_alloc(lhs * int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs * float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    default:
	    {
		ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
		*result = NULL;
		break;
	    }
	}

	/* Free the value */
	if (sexp_free(value, error) == 0)
	{
	    return 0;
	}

	/* Check for an error */
	if (*result == NULL)
	{
	    return 0;
	}

	/* The result becomes the product */
	product = *result;

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure we end cleanly */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	return 0;
    }

    *result = product;
    return 1;
}

/* The `/' primitive function */
static int prim_div(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t product;

    /* Make sure we have at least one arg */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "not a number");
	return 0;
    }

    /* Use the first arg as our divisor */
    if (sexp_eval(cons_car(args, error), env, &product, error) == 0)
    {
	return 0;
    }

    /* Start with the remaining args */
    args = cons_cdr(args, error);

    /* Keep going until we run out of args */
    while (sexp_get_type(args) == SEXP_CONS)
    {
	sexp_t value;

	/* Evaluate the car */
	if (sexp_eval(cons_car(args, error), env, &value, error) == 0)
	{
	    sexp_free(value, NULL);
	    return 0;
	}

	/* Figure out what to do based on the type of the first arg */
	switch (sexp_get_type(product))
	{
	    case SEXP_INT32:
	    {
		int32_t lhs = int32_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int32_alloc(lhs / int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs / int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs / float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_INT64:
	    {
		int64_t lhs = int64_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = int64_alloc(lhs / int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = int64_alloc(lhs / int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs / float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    case SEXP_FLOAT:
	    {
		double lhs = float_value(product, error);

		switch (sexp_get_type(value))
		{
		    case SEXP_INT32:
		    {
			*result = float_alloc(lhs / int32_value(value, error), error);
			break;
		    }

		    case SEXP_INT64:
		    {
			*result = float_alloc(lhs / int64_value(value, error), error);
			break;
		    }

		    case SEXP_FLOAT:
		    {
			*result = float_alloc(lhs / float_value(value, error), error);
			break;
		    }

		    default:
		    {
			ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
			*result = NULL;
		    }
		}

		break;
	    }

	    default:
	    {
		ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad type");
		*result = NULL;
		break;
	    }
	}

	/* Free the value */
	if (sexp_free(value, error) == 0)
	{
	    return 0;
	}

	/* Check for an error */
	if (*result == NULL)
	{
	    return 0;
	}

	/* The result becomes the product */
	product = *result;

	/* Move on to the next arg */
	args = cons_cdr(args, error);
    }

    /* Make sure we end cleanly */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad arg list");
	return 0;
    }

    *result = product;
    return 1;
}


/* The `quote' primitive function */
static int prim_quote(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    /* Make sure we have at least one arg */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	return 0;
    }

    /* Extract it */
    if ((*result = cons_car(args, error)) == NULL)
    {
	return 0;
    }

    /* Go to the next arg */
    if ((args = cons_cdr(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure that there are no more */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too many args");
	return 0;
    }

    /* Increase the reference count */
    return sexp_alloc_ref(*result, error);
}


/* The `setq' primitive function */
static int prim_setq(env_t env, sexp_t args, sexp_t *result, elvin_error_t error)
{
    sexp_t symbol;
    sexp_t value;

    /* Make sure we have at least one arg */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	return 0;
    }

    /* Extract it */
    if ((symbol = cons_car(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure it's a symbol */
    if (sexp_get_type(symbol) != SEXP_SYMBOL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "bad symbol");
	return 0;
    }

    /* Move on to the next arg */
    if ((args = cons_cdr(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure we have at least one more arg left */
    if (sexp_get_type(args) != SEXP_CONS)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too few args");
	return 0;
    }

    /* Extract it */
    if ((value = cons_car(args, error)) == NULL)
    {
	return 0;
    }

    /* Move on to the next arg */
    if ((args = cons_cdr(args, error)) == NULL)
    {
	return 0;
    }

    /* Make sure we're now out of args */
    if (sexp_get_type(args) != SEXP_NIL)
    {
	ELVIN_ERROR_ELVIN_NOT_YET_IMPLEMENTED(error, "too many args");
	return 0;
    }

    /* Evaluate the value */
    if (! sexp_eval(value, env, result, error))
    {
	return 0;
    }

    /* Assign it */
    if (! env_assign(env, symbol, *result, error))
    {
	return 0;
    }

    return 1;
}


/* Initializes the Lisp evaluation engine */
static env_t root_env_alloc(elvin_error_t error)
{
    env_t env;

    if ((env = env_alloc(40, NULL, error)) == NULL)
    {
	return NULL;
    }

    /* Register some constants */
    if (env_set_symbol(env, "t", "t", error) == 0 ||
	env_set_float(env, "pi", M_PI, error) == 0)
    {
	env_free(env, NULL);
	return NULL;
    }

    /* Register the built-in functions */
    if (env_set_builtin(env, "and", prim_and, error) == 0 ||
	env_set_builtin(env, "car", prim_car, error) == 0 ||
	env_set_builtin(env, "cdr", prim_cdr, error) == 0 ||
	env_set_builtin(env, "cons", prim_cons, error) == 0 ||
	env_set_builtin(env, "defun", prim_defun, error) == 0 ||
	env_set_builtin(env, "/", prim_div, error) == 0 ||
	env_set_builtin(env, "eq", prim_eq, error) == 0 ||
	env_set_builtin(env, "if", prim_if, error) == 0 ||
	env_set_builtin(env, "lambda", prim_lambda, error) == 0 ||
	env_set_builtin(env, "-", prim_minus, error) == 0 ||
	env_set_builtin(env, "or", prim_or, error) == 0 ||
	env_set_builtin(env, "+", prim_plus, error) == 0 ||
	env_set_builtin(env, "quote", prim_quote, error) == 0 ||
	env_set_builtin(env, "setq", prim_setq, error) == 0 ||
	env_set_builtin(env, "*", prim_times, error) == 0)
    {
	env_free(env, NULL);
	return NULL;
    }

    /* FIX THIS: define some built-in functions */
    return env;
}

/* For testing purposes */
int main(int argc, char *argv[])
{
    elvin_error_t error;
    parser_t parser;
    int i;

    /* Grab an error context */
    if (! (error = elvin_error_alloc()))
    {
	fprintf(stderr, "elvin_error_alloc(): failed\n");
	exit(1);
    }

    /* Initialize the interpreter */
    if (! interp_init(error))
    {
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Allocate the root environment */
    if ((root_env = root_env_alloc(error)) == NULL)
    {
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Allocate a new parser */
    if ((parser = parser_alloc(parsed, NULL, error)) == NULL)
    {
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* If we have no args, then read from stdin */
    if (argc < 2)
    {
	/* Print the prompt */
	printf("> "); fflush(stdout);

	if (! parse_file(parser, STDIN_FILENO, "[stdin]", error))
	{
	    fprintf(stderr, "parse_file(): failed\n");
	    elvin_error_fprintf(stderr, error);
	}
    }
    else
    {
	for (i = 1; i < argc; i++)
	{
	    char *filename = argv[i];
	    int fd;

	    fprintf(stderr, "--- parsing %s ---\n", filename);

	    /* Open the file */
	    if ((fd = open(filename, O_RDONLY)) < 0)
	    {
		perror("open(): failed");
		exit(1);
	    }

	    /* Parse its contents */
	    if (! parse_file(parser, fd, filename, error))
	    {
		fprintf(stderr, "parse_file(): failed\n");
		elvin_error_fprintf(stderr, error);
	    }

	    /* Close the file */
	    if (close(fd) < 0)
	    {
		perror("close(): failed");
		exit(1);
	    }
	}
    }

    /* Get rid of the parser */
    if (parser_free(parser, error) == 0)
    {
	fprintf(stderr, "parser_free(): failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Free the root environment */
    if (env_free(root_env, error) == 0)
    {
	fprintf(stderr, "env_free(): failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Clean up the interpreter */
    if (! interp_close(error))
    {
	fprintf(stderr, "interp_close(): failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    elvin_error_free(error);

    /* Report on memory usage */
    elvin_memory_report();
    exit(0);
}
