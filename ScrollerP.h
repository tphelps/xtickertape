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

   Description: 
             Private Scroller widget definitions

****************************************************************/

#ifndef SCROLLERP_H
#define SCROLLERP_H

#ifndef lint
static const char cvs_SCROLLERP_H[] = "$Id: ScrollerP.h,v 1.11 1999/06/21 14:27:11 phelps Exp $";
#endif /* lint */

#include <X11/CoreP.h>

#include "Scroller.h"
#include "glyph.h"
#include "List.h"


/* New fields for the Scroller widget record */
typedef struct
{
    int foo;
} ScrollerClassPart;


/* Full class record declaration */
typedef struct _ScrollerClassRec
{
    CoreClassPart core_class;
    ScrollerClassPart scroller_class;
} ScrollerClassRec;


/* New fields for the Scroller widget record */
typedef struct
{
    /* Resources */
    XtCallbackList callbacks;
    XFontStruct *font;
    Pixel groupPixel;
    Pixel userPixel;
    Pixel stringPixel;
    Pixel separatorPixel;
    Dimension fadeLevels;
    Dimension frequency;
    Position step;

    /* Private state */
    int isStopped;

    /* The number of pixels of the leftmost glyph beyond the left edge of the scroller */
    int left_offset;

    /* The number of pixels of the rightmost glyph beyond the edge of the scroller */
    int right_offset;

    /* The circular queue containing all glyphs */
    glyph_t glyphs;

    /* The leftmost visible glyph */
    glyph_t left_glyph;

    /* The rightmost visible glyph */
    glyph_t right_glyph;

    /* The width of the `effective' last glyph on the queue */
    unsigned long last_width;

    /* The queue containing glyphs which need to be added */
    glyph_t pending;


    /* The off-screen pixmap */
    Pixmap pixmap;

    /* The GC used to draw the Scroller's background */
    GC backgroundGC;

    /* The GC used to draw various glyphs */
    GC gc;

    /* The array of Pixels used to display the separator portion of a
     * Message at varying degrees of fading */
    Pixel *separatorPixels;

    /* The array of Pixels used to display the group portion of a
     * Message at varying degrees of fading */
    Pixel *groupPixels;

    /* The array of Pixels used to display the user portion of a
     * Message at varying degrees of fading */
    Pixel *userPixels;

    /* The array of Pixels used to display the string portion of a
     * Message at varying degrees of fading */
    Pixel *stringPixels;
} ScrollerPart;


/* Full instance record declaration */
typedef struct _ScrollerRec
{
    CorePart core;
    ScrollerPart scroller;
} ScrollerRec;



/* Semi-private methods */

/* Answers the GC for the background */
GC ScGCForBackground(ScrollerWidget self);

/* Answers a GC to be used for displaying the group */
GC ScGCForGroup(ScrollerWidget self, int level);

/* Answers a GC to be used for displaying the user */
GC ScGCForUser(ScrollerWidget self, int level);

/* Answers a GC to be used for displaying the string */
GC ScGCForString(ScrollerWidget self, int level);

/* Answers a GC to be used for displaying the separators */
GC ScGCForSeparator(ScrollerWidget self, int level);

/* Answers the font to use for displaying the group */
XFontStruct *ScFontForGroup(ScrollerWidget self);

/* Answers the font to use for displaying the user */
XFontStruct *ScFontForUser(ScrollerWidget self);

/* Answers the font to use for displaying the string */
XFontStruct *ScFontForString(ScrollerWidget self);

/* Answers the font to use for displaying the separators */
XFontStruct *ScFontForSeparator(ScrollerWidget self);


/* Answers the number of fade levels messages should go through */
Dimension ScGetFadeLevels(ScrollerWidget self);

/* Sets a timer to go off in interval milliseconds */
XtIntervalId ScStartTimer(ScrollerWidget self, unsigned long interval,
			  XtTimerCallbackProc proc, XtPointer client_data);

/* Stops a timer from going off */
void ScStopTimer(ScrollerWidget self, XtIntervalId timer);

/* Repaints the given glyph (if visible) */
void ScRepaintGlyph(ScrollerWidget self, glyph_t glyph);

/* Answers the width of the gap glyph */
int ScGapWidth(ScrollerWidget self);

#endif /* SCROLLERP_H */
