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
static const char cvsid[] = "$Id: gap.c,v 1.1 1999/06/20 12:36:00 phelps Exp $";
#endif /* lint */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include "glyph.h"
#include "ScrollerP.h"

typedef struct gap *gap_t;
struct gap
{
    GLYPH_PREFIX

    /* The ScrollerWidget in which the receiver is displayed */
    ScrollerWidget widget;
};

/* The receiver will never contain a Message */
static Message get_message(gap_t self)
{
    return NULL;
}

/* The receiver's width is based on the ScrollerWidget */
static unsigned int get_width(gap_t self)
{
    return ScGapWidth(self -> widget);
}

/* Simply erase the relevant bit of the widget */
static void do_paint(
    gap_t self, Display *display, Drawable drawable, int offset,
    int x, int y, unsigned int width, unsigned int height)
{
    XFillRectangle(
	display, drawable, 
	ScGCForBackground(self -> widget),
	offset, y, get_width(self), height);
}

/* The gap will never expire */
static int get_is_expired(gap_t self)
{
    return False;
}

/* Nothing to do here... */
void do_expire(gap_t self)
{
}


/* Allocates and initializes a new gap glyph */
glyph_t gap_alloc(ScrollerWidget widget)
{
    gap_t self;

    /* Allocate memory for a new gap glyph */
    if ((self = (gap_t) malloc(sizeof(struct gap))) == NULL)
    {
	return NULL;
    }

    /* Initialize its fields to sane values */
    self -> previous = (glyph_t)self;
    self -> next = (glyph_t)self;
    self -> free = (free_method_t)free;
    self -> get_message = (message_method_t)get_message;
    self -> get_width = (width_method_t)get_width;
    self -> paint = (paint_method_t)do_paint;
    self -> is_expired = (is_expired_method_t)get_is_expired;
    self -> expire = (expire_method_t)do_expire;

    self -> widget = widget;

    return (glyph_t) self;
}