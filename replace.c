/***************************************************************

  Copyright (C) 2002, 2004 by Mantara Software (ABN 17 105 665 594).
  All Rights Reserved.

  This software is the property of the Mantara Software. All use
  of this software is permitted only under the terms of a
  license agreement with Mantara Software. If you do not have
  such a license, then you have no rights to use this software
  in any manner and should contact Mantara at the address below
  to determine an appropriate licensing arrangement.
  
     Mantara Software
     PO Box 1820
     Toowong QLD 4066
     Australia
     Tel: +61 7 3876 8844
     Fax: +61 7 3876 8843
     Email: licensing@mantara.com
  
     Web: http://www.mantara.com
  
  This software is being provided "AS IS" without warranty of
  any kind. In no event shall Mantara Software be liable for
  damage of any kind arising out of or in connection with the
  use or performance of this software.

****************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h> /* snprintf, vsprintf */
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* NULL */
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h> /* toupper */
#endif
#ifdef HAVE_STRING_H
#include <string.h> /* memcpy */
#endif
#include "replace.h"

/* Replacements for standard functions */


#ifndef HAVE_MEMSET
/* A slow but correct implementation of memset */
void *memset(void *s, int c, size_t n)
{
    char *point = (char *)s;
    char *end = point + n;

    while (point < end)
    {
	*(point++) = c;
    }

    return s;
}
#endif

#ifndef HAVE_SNPRINTF
/* A dodgy hack to replace a real snprintf implementation */
#include <stdarg.h>

int snprintf(char *s, size_t n, const char *format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    return result;
}

#endif

#ifndef HAVE_STRCASECMP
/* A slow but correct implementation of strcasecmp */
int strcasecmp(const char *s1, const char *s2)
{
    size_t i = 0;
    int c1, c2;

    while (1)
    {
	c1 = toupper(s1[i]);
	c2 = toupper(s2[i]);

	if (c1 == 0)
	{
	    if (c2 == 0)
	    {
		return 0;
	    }
	    else
	    {
		return -1;
	    }
	}

	if (c2 == 0)
	{
	    return 1;
	}
	
	if (c1 < c2)
	{
	    return -1;
	}

	if (c1 > c2)
	{
	    return 1;
	}

	i++;
    }
}
#endif


#ifndef HAVE_STRCHR
/* A slow but correct implementation of strchr */
char *strchr(const char *s, int c)
{
    size_t i = 0;
    int ch;

    while ((ch = s[i]) != 0)
    {
	if (ch == c)
	{
	    return s + i;
	}

	i++;
    }

    return NULL;
}
#endif

#ifndef HAVE_STRDUP
/* A slow but correct implementation of strdup */
char *strdup(const char *s)
{
    size_t length;
    char *result;

    length = strlen(s) + 1;
    if ((result = (char *)malloc(length)) == NULL)
    {
	return NULL;
    }

    memcpy(result, s, length);
    return result;
}
#endif

#ifndef HAVE_STRRCHR
/* A slow but correct implementation of strrchr */
char *strrchr(const char *s, int c)
{
    char *result = NULL;
    int ch;

    while ((ch = *s) != 0)
    {
	if (ch == c)
	{
	    result = (char *)s;
	}

	s++;
    }

    return result;
}
#endif

#ifndef HAVE_STRERROR
#define BUFFER_SIZE 32
static char buffer[BUFFER_SIZE];
char *strerror(int errnum)
{
    snprintf(buffer, BUFFER_SIZE, "errno=%d\n", errnum);
    return buffer;
}
#endif
