/***************************************************************

  Copyright (C) DSTC Pty Ltd (ACN 052 372 577) 2001.
  Unpublished work.  All Rights Reserved.

  The software contained on this media is the property of the
  DSTC Pty Ltd.  Use of this software is strictly in accordance
  with the license agreement in the accompanying LICENSE.DOC
  file.  If your distribution of this software does not contain
  a LICENSE.DOC file then you have no rights to use this
  software in any manner and should contact DSTC at the address
  below to determine an appropriate licensing arrangement.

     DSTC Pty Ltd
     Level 7, GP South
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
static const char cvsid[] = "$Id: History.c,v 1.48 2002/04/04 15:26:05 phelps Exp $";
#endif /* lint */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <X11/Xresource.h>
#include <X11/StringDefs.h>
#include <X11/CoreP.h>
#include <X11/Xaw/SimpleP.h>
#include <Xm/XmAll.h>
#include <Xm/PrimitiveP.h>
#include "message.h"
#include "message_view.h"
#include "History.h"
#include "HistoryP.h"


#if 1
#define dprintf(x) printf x
#else
#define dprintf(x) ;
#endif

#if !defined(MIN)
# define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#if !defined(MAX)
# define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif


/*
 *
 * Resources
 *
 */

#define offset(field) XtOffsetOf(HistoryRec, field)
static XtResource resources[] =
{
    /* XtCallbackProc callback */
    {
	XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(history.callbacks), XtRCallback, (XtPointer)NULL
    },

    /* XtCallbackList attachment_callbacks */
    {
	XtNattachmentCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(history.attachment_callbacks), XtRCallback, (XtPointer)NULL
    },

    /* XFontStruct *font */
    {
	XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(history.font), XtRString, XtDefaultFont
    },

    /* Pixel timestamp_pixel */
    {
	XtNtimestampPixel, XtCTimestampPixel, XtRPixel, sizeof(Pixel),
	offset(history.timestamp_pixel), XtRString, "Black"
    },

    /* Pixel group_pixel */
    {
	XtNgroupPixel, XtCGroupPixel, XtRPixel, sizeof(Pixel),
	offset(history.group_pixel), XtRString, "Blue"
    },

    /* Pixel user_pixel */
    {
	XtNuserPixel, XtCUserPixel, XtRPixel, sizeof(Pixel),
	offset(history.user_pixel), XtRString, "Green"
    },

    /* Pixel string_pixel */
    {
	XtNstringPixel, XtCStringPixel, XtRPixel, sizeof(Pixel),
	offset(history.string_pixel), XtRString, "Red"
    },

    /* Pixel separator_pixel */
    {
	XtNseparatorPixel, XtCSeparatorPixel, XtRPixel, sizeof(Pixel),
	offset(history.separator_pixel), XtRString, XtDefaultForeground
    },

    /* Dimension margin_width */
    {
	XtNmarginWidth, XtCMarginWidth, XtRDimension, sizeof(Dimension),
	offset(history.margin_width), XtRImmediate, (XtPointer)5
    },

    /* Dimension margin_height */
    {
	XtNmarginHeight, XtCMarginHeight, XtRDimension, sizeof(Dimension),
	offset(history.margin_height), XtRImmediate, (XtPointer)5
    },

    /* unsigned int message_capacity */
    {
	XtNmessageCapacity, XtCMessageCapacity, XtRDimension, sizeof(unsigned int),
	offset(history.message_capacity), XtRImmediate, (XtPointer)32
    },

    /* Pixel selection_pixel */
    {
	XtNselectionPixel, XtCSelectionPixel, XtRPixel, sizeof(Pixel),
	offset(history.selection_pixel), XtRString, XtDefaultForeground
    },

    /* int drag_delay */
    {
	XtNdragDelay, XtCDragDelay, XtRInt, sizeof(int),
	offset(history.drag_delay), XtRImmediate, (XtPointer)100
    }
};
#undef offset


/* Action declarations */
static void drag(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void drag_done(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void do_select(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void toggle_selection(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void show_attachment(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void select_previous(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void select_next(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void scroll_left(Widget widget, XEvent *event, String *params, Cardinal *nparams);
static void scroll_right(Widget widget, XEvent *event, String *params, Cardinal *nparams);

static XtActionsRec actions[] =
{
    { "drag", drag },
    { "drag-done", drag_done },
    { "select", do_select },
    { "toggle-selection", toggle_selection },
    { "show-attachment", show_attachment },
    { "select-previous", select_previous },
    { "select-next", select_next },
    { "scroll-left", scroll_left },
    { "scroll-right", scroll_right }
};


/* We move the History window around by calling XCopyArea to move the
 * visible portion of the screen and rely on GraphicsExpose events to
 * tell us what to repaint.  It occasionally happens that we want to
 * move the window again before the X server has processed our
 * XCopyArea request.  When this happens we need to tweak the
 * coordinates of the Expose events generated by the first XCopyArea
 * because the exposed areas will have moved by the time the packets
 * which redraw them are processed by the server.  Since there's no
 * upper limit on the number of XCopyAreas that haven't yet been
 * processed we end up using a linked list to represent all them */
struct delta_queue
{
    /* The next item in the queue */
    delta_queue_t next;

    /* The sequence number of the XCopyArea request */
    unsigned long request_id;

    /* The displacement in the X direction */
    long dx;

    /* The displacement in the Y direction */
    long dy;
};


/* We store the history of messages as a tree, according to
 * message-ids and in-reply-to ids */
struct node
{
    /* The message recorded by the node */
    message_t message;

    /* The node's youngest child */
    node_t child;

    /* The node's youngest elder sibling */
    node_t sibling;
};

/* Allocates and returns a new node */
static node_t node_alloc(message_t message)
{
    node_t self;

    /* Allocate memory for the node */
    if ((self = malloc(sizeof(struct node))) == NULL)
    {
	return NULL;
    }

    /* Initialize its fields to sane values */
    memset(self, 0, sizeof(struct node));

    /* Record the message */
    self -> message = message_alloc_reference(message);
    return self;
}

/* Frees a node */
static void node_free(node_t self)
{
    /* Make sure we decrement the message's reference count */
    if (self -> message != NULL)
    {
	message_free(self -> message);
	self -> message = NULL;
    }

    dprintf(("node_free(): %p\n", self));
    free(self);
}

/* Returns the id of the node's message */
static char *node_get_id(node_t self)
{
    return message_get_id(self -> message);
}


/* Add a node to the tree.  WARNING: this is not a friendly function.
 * To avoid having to traverse the tree more than once when adding a
 * node, this function both finds a parent and adds a child to it and
 * also discards any nodes which no longer have visible children.
 *
 * self
 *    The pointer to the root node of the tree.  This will be updated,
 *    so it should really be the official pointer to the tree.
 *
 * parent_id
 *    The Message-Id of the parent of the node to add.
 *
 * child
 *    The child node to be added.  This will be set to NULL, so don't
 *    use a pointer you want to use later.
 *
 * index
 *    The display index of the root node.  This should initially be
 *    one less than the number of visible nodes.  Note that this value
 *    will be overwritten, so don't point to a value you wish to keep.
 *
 * index_out
 *    Will be set to the index of the newly added node.
 *
 * depth
 *    The depth (indentation level) of self.  Should initially be 0.
 *
 * depth_out
 *    Will be set to the depth of the newly added node.
 */
static void node_add1(node_t *self,
		      char *parent_id, node_t *child,
		      int *index, int *index_out,
		      int depth, int *depth_out)
{
    /* Traverse all of our siblings */
    while (*self != NULL)
    {
	char *id;

	/* If no match yet, then check for one here */
	if (parent_id && *child != NULL &&
	    (id = node_get_id(*self)) != NULL &&
	    strcmp(id, parent_id) == 0)
	{
	    /* Match!  Add the child to this node */
	    (*child) -> sibling = (*self) -> child;
	    (*self) -> child = *child;
	    *child = NULL;

	    /* Record output information */
	    *index_out = *index;
	    *depth_out = depth + 1;
	}

	/* Check with the descendents */
	if ((*self) -> child)
	{
	    node_add1(&(*self) -> child,
		     parent_id, child,
		     index, index_out,
		     depth + 1, depth_out);
	}

	/* If this node has scrolled off the top of the history and
	 * has no children then we can safely remove it from the tree.
	 * Note: this check is simplistic, but it will work since the
	 * new node must always be added after any nodes that have
	 * scrolled off the top. */
	if (*index < 0 && (*self) -> child == NULL)
	{
	    /* If the above statement is true, then it follows that
	     * the node may not have any elder siblings.  Hence this
	     * is a good sanity check. */
	    assert((*self) -> sibling == NULL);

	    /* Free the node */
	    node_free(*self);

	    /* End the loop */
	    *self = NULL;
	}
	else
	{
	    /* The pointer we want to update if this node disappears */
	    self = &(*self) -> sibling;
	}

	/* We've actually checked a node */
	(*index)--;
    }
}

/* Add a node to the tree.  This is just a friendly wrapper around
 * node_add1() above.
 *
 * self
 *    The pointer to the variable holding the root of the tree.  This
 *    will commonly be modified, so use the actual pointer.
 *
 * parent_id
 *    The Message-Id of the parent node; the In-Reply-To field.
 *
 * child
 *    The node to be added to the tree.
 *
 * count
 *    The number of nodes which should be visible in the tree view
 *    once the child is added to the tree.
 *
 * index_out
 *    Will be set to the index of the newly added node.
 *
 * depth_out
 *    Will be set to the depth of the newly added node.
 */
static void node_add(node_t *self,
		     char *parent_id,
		     node_t child,
		     int count,
		     int *index_out,
		     int *depth_out)
{
    int index;

    /* The last index will be count - 1 */
    index = count - 1;

    /* Trim the older nodes in the tree, and add the node if its
     * parent is in the tree. */
    node_add1(self, parent_id, &child, &index, index_out, 0, depth_out);

    /* Add the child now if it didn't get added already */
    if (child != NULL)
    {
	/* Add the node to the tree */
	child -> sibling = *self;
	(*self) = child;
	*index_out = count - 1;
	*depth_out = 0;
    }
}

#ifdef DEBUG
static void node_dump(node_t self, int depth)
{
    int i;

    /* Nothing to print... */
    if (self == NULL)
    {
	return;
    }

    /* Print our siblings */
    node_dump(self -> sibling, depth);

    /* Indent */
    for (i = 0; i < depth; i++)
    {
	printf("  ");
    }

    /* Print this node's message */
    printf("%p: %s\n", self, message_get_string(self -> message));

    /* Print our children */
    node_dump(self -> child, depth + 1);
}
#endif


/* Populate an array with message views */
static void node_populate(node_t self,
			  XFontStruct *font,
			  message_view_t *array,
			  int depth, int *index,
			  message_t selection,
			  unsigned int *selection_index_out)
{
    /* Go through each of our siblings */
    while (self != NULL)
    {
	/* Add the children first */
	if (self -> child)
	{
	    node_populate(self -> child, font, array, depth + 1, index, selection, selection_index_out);
	}

	/* Bail if the index goes negative */
	if (index < 0)
	{
	    printf("bailing out!\n");
	    return;
	}

	/* Is this the selection? */
	if (self -> message == selection)
	{
	    *selection_index_out = *index;
	}

	/* Wrap the message in a message view */
	array[(*index)--] =  message_view_alloc(self -> message, font, depth);

	/* Move on to the next node */
	self = self -> sibling;
    }
}
		     


/*
 *
 * Method declarations
 *
 */

static void init(Widget request, Widget widget, ArgList args, Cardinal *num_args);
static void realize(
    Widget widget,
    XtValueMask *value_mask,
    XSetWindowAttributes *attributes);
static void redisplay(Widget self, XEvent *event, Region region);
static void gexpose(Widget widget, XtPointer rock, XEvent *event, Boolean *ignored);
static void destroy(Widget self);
static void resize(Widget self);
static Boolean set_values(
    Widget current,
    Widget request,
    Widget new,
    ArgList args,
    Cardinal *num_args);
static XtGeometryResult query_geometry(
    Widget self,
    XtWidgetGeometry *intended,
    XtWidgetGeometry *preferred);
static void border_highlight(Widget widget);
static void border_unhighlight(Widget widget);

/*
 *
 * Class record initializations
 *
 */

HistoryClassRec historyClassRec =
{
    /* core_class fields */
    {
	(WidgetClass)&xmPrimitiveClassRec, /* superclass */
	"History", /* class_name */
	sizeof(HistoryRec), /* widget_size */
	NULL, /* class_initialize */
	NULL, /* class_part_initialize */
	False, /* class_inited */
	init, /* initialize */
	NULL, /* initialize_hook */
	realize, /* realize */
	actions, /* actions */
	XtNumber(actions), /* num_actions */
	resources, /* resources */
	XtNumber(resources), /* num_resources */
	NULLQUARK, /* xrm_class */
	True, /* compress_motion */
	XtExposeCompressMultiple, /* compress_exposure */
	True, /* compress_enterleave */
	False, /* visible_interest */
	destroy, /* destroy */
	resize, /* resize */
	redisplay, /* expose */
	set_values, /* set_values */
	NULL, /* set_values_hook */
	XtInheritSetValuesAlmost, /* set_values_almost */
	NULL, /* get_values_hook */
	NULL, /* accept_focus */
	XtVersion, /* version */
	NULL, /* callback_private */
	NULL, /* tm_table */
	query_geometry, /* query_geometry */
	XtInheritDisplayAccelerator, /* display_accelerator */
	NULL /* extension */
    },

    /* Primitive class fields initialization */
    {
	border_highlight, /* border_highlight */
	border_unhighlight, /* border_unhighlight */
	NULL, /* translations */
	NULL, /* arm_and_activate_proc */
	NULL, /* synthetic resources */
	0, /* num syn res */
	NULL, /* extension */
    },

    /* History class fields initialization */
    {
	0 /* foo */
    }
};

WidgetClass historyWidgetClass = (WidgetClass) &historyClassRec;


/*
 *
 * Private static variables
 *
 */

/*
 *
 * Private Methods
 *
 */

/* Sets the origin of the visible portion of the widget */
static void set_origin(HistoryWidget self, long x, long y, int update_scrollbars)
{
    Display *display = XtDisplay((Widget)self);
    Window window = XtWindow((Widget)self);
    GC gc = self -> history.gc;
    XGCValues values;
    delta_queue_t item;

    /* Skip this part if we're not visible */
    if (gc != None)
    {
	/* Allocate a new queue item to represent this movement */
	item = malloc(sizeof(struct delta_queue));
	item -> next = self -> history.dqueue;
	item -> request_id = NextRequest(display);
	item -> dx = x - self -> history.x;
	item -> dy = y - self -> history.y;
	self -> history.dqueue = item;

	/* Remove our clip mask */
	values.clip_mask = None;
	XChangeGC(display, gc, GCClipMask, &values);

	/* Copy to the new location */
	XCopyArea(
	    display, window, window, gc,
	    item -> dx, item -> dy,
	    self -> core.width, self -> core.height,
	    0, 0);
    }

    /* Update the scrollbars */
    if (update_scrollbars)
    {
	/* Have we moved horizontally? */
	if (self -> history.x != x)
	{
	    XtVaSetValues(
		self -> history.hscrollbar,
		XmNvalue, x,
		NULL);
	}

	/* Have we moved vertically? */
	if (self -> history.y != y)
	{
	    XtVaSetValues(
		self -> history.vscrollbar,
		XmNvalue, y,
		NULL);
	}
    }

    /* Record our new location */
    self -> history.x = x;
    self -> history.y = y;

}

/* Callback for the vertical scrollbar */
static void vert_scrollbar_cb(Widget widget, XtPointer client_data, XtPointer call_data)
{
    HistoryWidget self = (HistoryWidget)client_data;
    XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)call_data;

    /* Drag to the new location */
    set_origin(self, self -> history.x, cbs -> value, 0);
}


/* Callback for the horizontal scrollbar */
static void horiz_scrollbar_cb(Widget widget, XtPointer client_data, XtPointer call_data)
{
    HistoryWidget self = (HistoryWidget)client_data;
    XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)call_data;

    /* Drag to the new location */
    set_origin(self, cbs -> value, self -> history.y, 0);
}


/* Initializes the History-related stuff */
/* ARGSUSED */
static void init(Widget request, Widget widget, ArgList args, Cardinal *num_args)
{
    HistoryWidget self = (HistoryWidget)widget;
    Widget scrollbar;

    dprintf(("History.init()\n"));

    /* Set the initial width/height if none are supplied */
    self -> core.width = 400;
    self -> core.height = 80;

    /* Set our initial dimensions to be the margin width */
    self -> history.width = (long)self -> history.margin_width * 2;
    self -> history.height = (long)self -> history.margin_height * 2;

    /* Compute the line height */
    self -> history.line_height =
	(long)self -> history.font -> ascent +
	(long)self -> history.font -> descent + 1;

    /* Start with an empty delta queue */
    self -> history.dqueue = NULL;

    /* Allocate enough room for all of our message views */
    self -> history.message_capacity = MAX(self -> history.message_capacity, 1);
    self -> history.messages = calloc(self -> history.message_capacity, sizeof(message_t));
    self -> history.message_count = 0;
    self -> history.message_index = 0;
    self -> history.message_views =
	calloc(self -> history.message_capacity, sizeof(message_view_t));

    /* Nothing is selected yet */
    self -> history.selection = NULL;
    self -> history.selection_index = (unsigned int)-1;
    self -> history.drag_timeout = None;
    self -> history.drag_direction = DRAG_NONE;
    self -> history.show_timestamps = False;

    /* Create the horizontal scrollbar */
    scrollbar = XtVaCreateManagedWidget(
	"HorizScrollBar", xmScrollBarWidgetClass,
	XtParent(self),
	XmNincrement, self -> history.line_height,
	XmNmaximum, self -> history.width,
	XmNminimum, 0,
	XmNorientation, XmHORIZONTAL,
	XmNpageIncrement, self -> core.width,
	XmNsliderSize, self -> history.width,
	NULL);

    /* Add a bunch of callbacks */
    XtAddCallback(scrollbar, XmNdragCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNdecrementCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNincrementCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNpageDecrementCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNpageIncrementCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNtoTopCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNtoBottomCallback, horiz_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNvalueChangedCallback, horiz_scrollbar_cb, self);
    self -> history.hscrollbar = scrollbar;
    
    /* Create the vertical scrollbar */
    scrollbar = XtVaCreateManagedWidget(
	"VertScrollBar", xmScrollBarWidgetClass,
	XtParent(self),
	XmNincrement, self -> history.line_height,
	XmNmaximum, self -> history.height,
	XmNminimum, 0,
	XmNorientation, XmVERTICAL,
	XmNpageIncrement, self -> core.height,
	XmNsliderSize, self -> history.height,
	NULL);

    /* Add a bunch of callbacks */
    XtAddCallback(scrollbar, XmNdecrementCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNdragCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNincrementCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNpageDecrementCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNpageIncrementCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNtoTopCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNtoBottomCallback, vert_scrollbar_cb, self);
    XtAddCallback(scrollbar, XmNvalueChangedCallback, vert_scrollbar_cb, self);
    self -> history.vscrollbar = scrollbar;
}

/* Updates the scrollbars after either the widget or the data it is
 * displaying is resized */
static void update_scrollbars(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;
    Dimension width, height;
    Position x, y;

    /* Figure out how big the widget is now */
    XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, NULL);

    /* The slider can't be wider than the total */
    if (self -> history.width < width)
    {
	width = self -> history.width;
    }

    /* Keep the right edge sane */
    x = self -> history.x;
    if (self -> history.width - width < x)
    {
	x = self -> history.width - width;
    }


    /* The slider can't be taller than the total */
    if (self -> history.height < height)
    {
	height = self -> history.height;
    }

    /* Keep the bottom edge sane */
    y = self -> history.y;
    if (self -> history.height - height < y)
    {
	y = self -> history.height - height;
    }

    /* Move the origin */
    set_origin(self, x, y, 0);

    /* Update the horizontal scrollbar */
    XtVaSetValues(
	self -> history.hscrollbar,
	XmNvalue, x,
	XmNsliderSize, width,
	XmNmaximum, self -> history.width,
	NULL);

    /* Update the vertical scrollbar */
    XtVaSetValues(
	self -> history.vscrollbar,
	XmNvalue, y,
	XmNsliderSize, height,
	XmNmaximum, self -> history.height,
	NULL);
}

/* Realize the widget by creating a window in which to display it */
static void realize(
    Widget widget,
    XtValueMask *value_mask,
    XSetWindowAttributes *attributes)
{
    HistoryWidget self = (HistoryWidget)widget;
    Display *display = XtDisplay(self);
    XGCValues values;

    dprintf(("History.realize()\n"));

    /* Create our window */
    XtCreateWindow(widget, InputOutput, CopyFromParent, *value_mask, attributes);

    /* Create a GC for our own nefarious purposes */
    values.background = self -> core.background_pixel;
    values.font = self -> history.font -> fid;
    self -> history.gc = XCreateGC(display, XtWindow(widget), GCBackground | GCFont, &values);

    /* Register to receive GraphicsExpose events */
    XtAddEventHandler(widget, 0, True, gexpose, NULL);
}

/* Translate the bounding box to compensate for unprocessed CopyArea
 * requests */
static void compensate_bbox(
    HistoryWidget self,
    unsigned long request_id,
    XRectangle *bbox)
{
    delta_queue_t previous = NULL;
    delta_queue_t item = self -> history.dqueue;

    /* Go through each outstanding delta item */
    while (item != NULL)
    {
	/* Has the request for this item been processed? */
	if (item -> request_id < request_id)
	{
	    /* Yes.  Throw it away. */
	    if (previous == NULL)
	    {
		self -> history.dqueue = item -> next;
		free(item);
		item = self -> history.dqueue;
	    }
	    else
	    {
		previous -> next = item -> next;
		free(item);
		item = previous -> next;
	    }
	}
	else
	{
	    /* No.  Translate the bounding box accordingly. */
	    bbox -> x -= item -> dx;
	    bbox -> y -= item -> dy;
	    previous = item;
	    item = item -> next;
	}
    }
}

/* Repaint the widget */
static void paint(HistoryWidget self, XRectangle *bbox)
{
    Display *display = XtDisplay((Widget)self);
    Window window = XtWindow((Widget)self);
    GC gc = self -> history.gc;
    long xmargin = (long)self -> history.margin_width;
    long ymargin = (long)self -> history.margin_height;
    int show_timestamps = self -> history.show_timestamps;
    message_view_t view;
    XGCValues values;
    unsigned int index;
    long x, y;

    /* Set that as our bounding box */
    XSetClipRectangles(display, gc, 0, 0, bbox, 1, YXSorted);

    /* Compute our X coordinate */
    x = xmargin - self -> history.x;

    /* Is the top margin visible? */
    if (self -> history.y < self -> history.margin_height)
    {
	/* Yes.  Start drawing at index 0 */
	index = 0;
	y = ymargin - self -> history.y;
    }
    else
    {
	/* No.  Compute the first visible index */
	index = (self -> history.y - self -> history.margin_height) / self -> history.line_height;
	y = (ymargin - self -> history.y) % self -> history.line_height;
    }

    /* Draw all visible message views */
    while (index < self -> history.message_count)
    {
	/* Skip NULL message views */
	if ((view = self -> history.message_views[index++]) == NULL)
	{
	    return;
	}

	/* Is this the selected message? */
	if (message_view_get_message(view) == self -> history.selection)
	{
	    /* Yes, draw a background for it */
	    values.foreground = self -> history.selection_pixel;
	    XChangeGC(display, gc, GCForeground, &values);

	    /* FIX THIS: should we respect the margin? */
	    XFillRectangle(
		display, window, gc,
		0, y, self -> core.width,
		self -> history.line_height);

	    /* Is the widget highlighted? */
	    if (self -> primitive.highlighted)
	    {
		values.foreground = self -> primitive.highlight_color;
		XChangeGC(display, gc, GCForeground, &values);

		/* Draw a rectangle around the selection */
		XDrawRectangle(
		    display, window, gc,
		    -self -> history.x, y,
		    self -> history.width - 1, self -> history.line_height - 1);
	    }
	}

	/* Draw the view */
	message_view_paint(
	    view, display, window, gc,
	    show_timestamps, self -> history.timestamp_pixel,
	    self -> history.group_pixel, self -> history.user_pixel,
	    self -> history.string_pixel, self -> history.separator_pixel,
	    x, y + self -> history.font -> ascent, bbox);

	/* Get ready to draw the next one */
	y += self -> history.line_height;

	/* Bail out if the next line is past the end of the screen */
	if (! (y < self -> core.height))
	{
	    return;
	}
    }
}

/* Redisplay the given region */
static void redisplay(Widget widget, XEvent *event, Region region)
{
    HistoryWidget self = (HistoryWidget)widget;
    XExposeEvent *x_event = (XExposeEvent *)event;
    XRectangle bbox;

    /* Find the smallest rectangle which contains the region */
    XClipBox(region, &bbox);

    /* Compensate for unprocessed CopyArea requests */
    compensate_bbox(self, x_event -> serial, &bbox);

    /* Repaint the region */
    paint(self, &bbox);
}


/* Redraw the item at the given index */
static void redisplay_index(HistoryWidget self, unsigned int index)
{
    XRectangle bbox;
    long y;
    
    /* Bail if the index is out of range */
    if (! (index < self -> history.message_count))
    {
	return;
    }

    /* Figure out where the message is */
    y = index * self -> history.line_height -
	self -> history.y + self -> history.margin_height;

    /* Create a bounding box for the message */
    bbox.x = 0;
    bbox.y = y;
    bbox.width = self -> core.width;
    bbox.height = self -> history.line_height;

    /* Paint it */
    paint(self, &bbox);
}

/* Repaint the bits of the widget that didn't get copied */
static void gexpose(Widget widget, XtPointer rock, XEvent *event, Boolean *ignored)
{
    HistoryWidget self = (HistoryWidget)widget;
    Display *display = XtDisplay(widget);
    XEvent event_buffer;
    XRectangle bbox;

    /* Process all of the GraphicsExpose events in one hit */
    while (1)
    {
	XGraphicsExposeEvent *g_event;

	/* Stop drawing stuff if the widget is obscured */
	if (event -> type == NoExpose)
	{
	    return;
	}

	/* Sanity check */
	assert(event -> type == GraphicsExpose || event -> type == NoExpose);

	/* Coerce the event */
	g_event = (XGraphicsExposeEvent *)event;

	/* Get the bounding box of the event */
	bbox.x = g_event -> x;
	bbox.y = g_event -> y;
	bbox.width = g_event -> width;
	bbox.height = g_event -> height;

	/* Compensate for unprocessed CopyArea requests */
	compensate_bbox(self, g_event -> serial, &bbox);

	/* Update this portion of the history */
	paint(self,  &bbox);

	/* Bail out if this is the last GraphicsExpose event */
	if (g_event -> count < 1)
	{
	    return;
	}

	/* Otherwise grab the next one */
	XNextEvent(display, &event_buffer);
	event = &event_buffer;
    }
}

/* Recompute the dimensions of the widget and update the scrollbars */
static void recompute_dimensions(HistoryWidget self)
{
    int show_timestamps = self -> history.show_timestamps;
    long width = 0;
    long height = 0;
    unsigned int i;

    /* Measure each message */
    for (i = 0; i < self -> history.message_count; i++)
    {
	struct string_sizes sizes;

	message_view_get_sizes(self -> history.message_views[i], show_timestamps, &sizes);
	width = MAX(width, sizes.width);
	height += self -> history.line_height;
    }

    /* Update our dimensions */
    self -> history.width = width + (long)self -> history.margin_width * 2;
    self -> history.height = height + (long)self -> history.margin_height * 2;

    /* And update the scrollbars */
    update_scrollbars((Widget)self);
}

/* Insert a message before the given index */
static void insert_message(HistoryWidget self,
			   unsigned int index,
			   unsigned int indent,
			   message_t message)
{
    Display *display = XtDisplay((Widget)self);
    Window window = XtWindow((Widget)self);
    message_view_t view;
    struct string_sizes sizes;
    long y = 0;
    long delta_y;
    long width = 0;
    long height = 0;
    XRectangle bbox;
    unsigned int i;
    XGCValues values;
    GC gc = self -> history.gc;
    int show_timestamps = self -> history.show_timestamps;

    /* Sanity check */
    assert(index <= self -> history.message_count);

    /* Cancel our clip mask */
    if (gc != None)
    {
	values.clip_mask = None;
 	values.foreground = self -> core.background_pixel;
	XChangeGC(display, gc, GCClipMask | GCForeground, &values);
    }

    /* If there's still room then we'll have to move stuff down */
    if (self -> history.message_count < self -> history.message_capacity)
    {
	/* Measure the nodes before the index */
	for (i = 0; i < index; i++)
	{
	    view = self -> history.message_views[i];

	    /* Measure the view */
	    message_view_get_sizes(view, show_timestamps, &sizes);
	    width = MAX(width, sizes.width);
	    y += self -> history.line_height;
	    height += self -> history.line_height;
	}

	/* Measure the nodes after the index, moving them down to make
	 * room for the new message. */
	for (i = self -> history.message_count; i > index; i--)
	{
	    view = self -> history.message_views[i - 1];

	    /* Measure the view */
	    message_view_get_sizes(view, show_timestamps, &sizes);
	    width = MAX(width, sizes.width);
	    height += self -> history.line_height;

	    /* Move it */
	    self -> history.message_views[i] = view;
	    if (self -> history.selection_index == i - 1)
	    {
		self -> history.selection_index = i;
	    }
	}

	/* Move stuff down to make room */
	if (gc != None)
	{
	    XCopyArea(
		display, window, window, gc,
		0, self -> history.margin_height - self -> history.y + y,
		self -> core.width, height - y,
		0, self -> history.margin_height - self -> history.y + y + self -> history.line_height);
	}

	/* We've got another node */
	self -> history.message_count++;
    }
    else
    {
	/* Discard the first message view */
	message_view_free(self -> history.message_views[0]);
	if (self -> history.selection_index == 0)
	{
	    self -> history.selection_index = (unsigned int)-1;
	}

	/* Measure the nodes before the index, moving them up to make
	 * room for the new message. */
	for (i = 0; i < index; i++)
	{
	    view = self -> history.message_views[i + 1];

	    /* Measure the view */
	    message_view_get_sizes(view, show_timestamps, &sizes);
	    width = MAX(width, sizes.width);
	    y += self -> history.line_height;
	    height += self -> history.line_height;

	    /* Move it up */
	    self -> history.message_views[i] = view;

	    /* Update the selection index */
	    if (self -> history.selection_index == i + 1)
	    {
		self -> history.selection_index = i;
	    }
	}

	/* Measure the nodes after the index */
	for (i = index + 1; i < self -> history.message_count; i++)
	{
	    view = self -> history.message_views[i];

	    /* Measure the view */
	    message_view_get_sizes(view, show_timestamps, &sizes);
	    width = MAX(width, sizes.width);
	    height += self -> history.line_height;
	}

	/* Move stuff up to make room */
	if (gc != None)
	{
	    XCopyArea(
		display, window, window, gc,
		0, self -> history.margin_height - self -> history.y + self -> history.line_height,
		self -> core.width, y,
		0, self -> history.margin_height - self -> history.y);
	}
    }

    /* Create a new message view */
    view = message_view_alloc(message, self -> history.font, indent);
    self -> history.message_views[index] = view;

    /* Measure it */
    message_view_get_sizes(view, show_timestamps, &sizes);
    width = MAX(width, sizes.width);
    height += self -> history.line_height;

    /* Paint it */
    if (gc != None)
    {
	/* Remove any remains of the previous message */
	XFillRectangle(
	    display, window, gc,
	    0, self -> history.margin_height - self -> history.y + y,
	    self -> core.width, self -> history.line_height);

	/* Make a bounding box */
	bbox.x = 0;
	bbox.y = self -> history.margin_height - self -> history.y + y;
	bbox.width = self -> core.width;
	bbox.height = self -> history.line_height;

	/* Use it to draw the new message */
	message_view_paint(
	    view, display, window, gc,
	    self -> history.show_timestamps,
	    self -> history.timestamp_pixel,
	    self -> history.group_pixel, self -> history.user_pixel,
	    self -> history.string_pixel, self -> history.separator_pixel,
	    self -> history.margin_width - self -> history.x,
	    self -> history.margin_height - self -> history.y + y + self -> history.font -> ascent,
	    &bbox);
    }

    /* Add the margins to our height and width */
    width += (long)self -> history.margin_width * 2;
    height += (long)self -> history.margin_height * 2;

    /* Has the width changed? */
    if (self -> history.width != width)
    {
	self -> history.width = width;

	/* Repaint the selection so that the right edge is drawn properly */
	redisplay_index(self, self -> history.selection_index);
    }

    /* Update our height */
    if (self -> core.height < height)
    {
	delta_y = MAX(0, height - MAX(self -> history.height, self -> core.height));
    }
    else
    {
	delta_y = 0;
    }

    self -> history.height = height;

    /* Update the scrollbars */
    update_scrollbars((Widget)self);
    set_origin(self, self -> history.x, self -> history.y + delta_y , True);
}

/* Make sure the given index is visible */
static void make_index_visible(HistoryWidget self, unsigned int index)
{
    long y;

    /* Sanity check */
    assert(index < self -> history.message_count);

    /* Figure out where the index would appear */
    y = self -> history.selection_index * self -> history.line_height;

    /* If it's above then scroll up to it */
    if (y + self -> history.margin_height < self -> history.y)
    {
	set_origin(self, self -> history.x, y + self -> history.margin_height, 1);
	return;
    }

    /* If it's below then scroll down to it */
    if (self -> history.y + self -> core.height - self -> history.margin_height <
	y + self -> history.line_height)
    {
	set_origin(
	    self, self -> history.x,
	    y + self -> history.line_height - self -> core.height +
	    self -> history.margin_height, 1);
	return;
    }
}

/* Selects the given message at the given index */
static void set_selection(HistoryWidget self, unsigned int index, message_t message)
{
    Display *display = XtDisplay((Widget)self);
    Window window = XtWindow((Widget)self);
    GC gc = self -> history.gc;
    XGCValues values;
    XRectangle bbox;
    long y;

    /* Bail if the selection is unchanged */
    if (self -> history.selection == message)
    {
	return;
    }

    /* Set up a bounding box */
    bbox.x = 0;
    bbox.y = 0;
    bbox.width = self -> core.width;
    bbox.height = self -> core.height;

    /* Drop our extra reference to the old selection */
    if (self -> history.selection != NULL)
    {
	message_free(self -> history.selection);
    }

    /* Redraw the old selection if appropriate */
    if (gc != None && self -> history.selection_index != (unsigned int)-1)
    {
	/* Determine the location of the old selection */
	y = self -> history.selection_index * self -> history.line_height -
	    self -> history.y + self -> history.margin_height;

	/* Is it visible? */
	if (0 <= y + self -> history.line_height && y < (long)self -> history.height)
	{
	    /* Clear the clip mask */
	    values.clip_mask = None;
	    values.foreground = self -> core.background_pixel;
	    XChangeGC(display, gc, GCClipMask | GCForeground, &values);

	    /* Erase the rectangle previously occupied by the message */
	    XFillRectangle(
		display, window, gc,
		0, y, self -> core.width, self -> history.line_height);

	    /* And then draw it again */
	    message_view_paint(
		self -> history.message_views[self -> history.selection_index],
		display, window, gc,
		self -> history.show_timestamps,
		self -> history.timestamp_pixel,
		self -> history.group_pixel, self -> history.user_pixel,
		self -> history.string_pixel, self -> history.separator_pixel,
		self -> history.margin_width - self -> history.x,
		y + self -> history.font -> ascent,
		&bbox);
	}
    }

    /* Record the new selection */
    if (message == NULL) {
	self -> history.selection = NULL;
    } else {
	self -> history.selection = message_alloc_reference(message);
    }

    /* And record its index */
    self -> history.selection_index = index;

    /* Draw the new selection if appropriate */
    if (gc != None && self -> history.selection_index != (unsigned int)-1)
    {
	/* Determine its location */
	y = self -> history.selection_index * self -> history.line_height -
	    self -> history.y + self -> history.margin_height;

	/* Is it visible? */
	if (0 <= y + self -> history.line_height && y < (long)self -> history.height)
	{
	    /* Clear the clip mask */
	    values.clip_mask = None;
	    values.foreground = self -> history.selection_pixel;
	    XChangeGC(display, gc, GCClipMask | GCForeground, &values);

	    /* Draw the selection rectangle */
	    XFillRectangle(
		display, window, gc,
		0, y, self -> core.width, self -> history.line_height);

	    /* Is the widget highlighted? */
	    if (self -> primitive.highlighted)
	    {
		values.foreground = self -> primitive.highlight_color;
		XChangeGC(display, gc, GCForeground, &values);

		/* Draw a rectangle around the selection */
		XDrawRectangle(
		    display, window, gc,
		    -self -> history.x, y,
		    self -> history.width - 1, self -> history.line_height - 1);
	    }

	    /* And then draw the message view on top of it */
	    message_view_paint(
		self -> history.message_views[self -> history.selection_index],
		display, window, gc,
		self -> history.show_timestamps,
		self -> history.timestamp_pixel,
		self -> history.group_pixel, self -> history.user_pixel,
		self -> history.string_pixel, self -> history.separator_pixel,
		self -> history.margin_width - self -> history.x,
		y + self -> history.font -> ascent,
		&bbox);
	}

	/* Try to make the entire selection is visible */
	make_index_visible(self, index);
    }

    /* Call the callback with our new selection */
    XtCallCallbackList((Widget)self, self -> history.callbacks, (XtPointer)message);
}

/* Selects the message at the given index */
static void set_selection_index(HistoryWidget self, unsigned int index)
{
    message_view_t view;

    /* Locate the message view at that index */
    if (index < self -> history.message_count)
    {
	view = self -> history.message_views[index];
    }
    else
    {
	index = (unsigned int)-1;
	view = NULL;
    }

    /* Select it */
    set_selection(self, index, view ? message_view_get_message(view) : NULL);
}

/* Destroy the widget */
static void destroy(Widget self)
{
    dprintf(("History.destroy()\n"));
}


/* Resize the widget */
static void resize(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;
    int page_inc;

    dprintf(("History.resize(w=%d, h=%d)\n", self -> core.width, self -> core.height));

    /* Update the page increment of the horizontal scrollbar */
    page_inc = self -> core.height;
    XtVaSetValues(
	self -> history.hscrollbar,
	XmNpageIncrement, self -> core.width,
	NULL);

    /* Update the page increment of the vertical scrollbar */
    page_inc = self -> core.height;
    XtVaSetValues(
	self -> history.vscrollbar,
	XmNpageIncrement, page_inc,
	NULL);

    /* Update the scrollbar sizes */
    update_scrollbars(widget);
}


/* What should this do? */
static Boolean set_values(
    Widget current,
    Widget request,
    Widget new,
    ArgList args,
    Cardinal *num_args)
{
    dprintf(("History.set_values()\n"));
    return False;
}


/* We're always happy */
static XtGeometryResult query_geometry(
    Widget widget,
    XtWidgetGeometry *intended,
    XtWidgetGeometry *preferred)
{
    dprintf(("History.query_geometry()\n"));
    return XtGeometryYes;
}

/* Hightlight the border */
static void border_highlight(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;

    dprintf(("History.border_highlight()\n"));

    /* Skip out if we're already highlighted */
    if (self -> primitive.highlighted)
    {
	return;
    }

    /* We're now highlighted */
    self -> primitive.highlighted = True;

    /* Repaint the selection */
    redisplay_index(self, self -> history.selection_index);
}

/* Unhighlight the border */
static void border_unhighlight(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;

    dprintf(("History.border_unhighlight()\n"));

    /* No longer highlighting */
    self -> primitive.highlighted = False;

    /* Repaint the selection */
    redisplay_index(self, self -> history.selection_index);
}

/*
 *
 *  Action definitions
 *
 */

/* Returns the message index corresponding to the given y coordinate */
static unsigned int index_of_y(HistoryWidget self, long y)
{
    unsigned int index;

    /* Is y in the top margin? */
    if (self -> history.y + y < self -> history.margin_height)
    {
	/* Pretend its in the first message */
	index = 0;
    }
    else
    {
	/* Otherwise compute the position */
	index = (self -> history.y + y - self -> history.margin_height) / self -> history.line_height;
    }

    /* Anything outside of bounds becomes the last item (or -1 if we have no last item) */
    if (! (index < self -> history.message_count))
    {
	index = self -> history.message_count - 1;
    }

    return index;
}

/* Scroll up or down, wait and repeat */
static void drag_timeout_cb(XtPointer closure, XtIntervalId *id)
{
    HistoryWidget self = (HistoryWidget)closure;
    Window root, child;
    int root_x, root_y, x, y;
    unsigned int mask;
    unsigned int index;

    /* Sanity check */
    assert(self -> history.drag_timeout == *id);

    /* Clear our status */
    self -> history.drag_direction = DRAG_NONE;
    self -> history.drag_timeout = None;

    /* Determine the current position of the pointer */
    if (XQueryPointer(
	    XtDisplay((Widget)self), XtWindow((Widget)self),
	    &root, &child,
	    &root_x, &root_y, &x, &y, &mask))
    {
	/* Figure out what's happening */
	if (y < 0)
	{
	    /* We're being dragged up. */
	    self -> history.drag_direction = DRAG_UP;

	    /* If we've already selected at the first message then make
	     * the top margin visible */
	    if (self -> history.selection_index == 0)
	    {
		set_origin(self, self -> history.x, 0, 1);
	    }
	    else
	    {
 		/* Otherwise select the item before the first completely visible one */
		index = index_of_y(self, -1);
		set_selection_index(self, index);
	    }
	}
	else if (self -> core.height < y)
	{
	    /* We're being dragged down */
	    self -> history.drag_direction = DRAG_DOWN;

	    /* If we've already select the last message then make the
	     * bottom margin visible */
	    if (self -> history.selection_index == self -> history.message_count - 1)
	    {
		if (! (self -> history.height < self -> core.height))
		{
		    set_origin(self, self -> history.x, self -> history.height - self -> core.height, 1);
		}
	    }
	    else
	    {
		/* Otherwise select the item below the last completely visible one */
		index = index_of_y(self, self -> core.height + 1);
		set_selection_index(self, index);
	    }
	}
	else
	{
	    /* We're within the widget's bounds */
	    index = index_of_y(self, y);
	    set_selection_index(self, index);
	}
    }

    /* Arrange to scroll again after a judicious pause */
    self -> history.drag_timeout = XtAppAddTimeOut(
	XtWidgetToApplicationContext((Widget)self), self -> history.drag_delay,
	drag_timeout_cb, (XtPointer)self);
}

/* Dragging selects */
static void drag(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    XButtonEvent *button_event = (XButtonEvent *)event;

    /* This widget now gets keyboard input */
    XmProcessTraversal(widget, XmTRAVERSE_CURRENT);

    /* FIgure out what's going on */
    if (button_event -> y < 0)
    {
	/* We should be dragging up.  Is a timeout already registered? */
	if (self -> history.drag_timeout != None)
	{
	    /* Bail if we're already scrolling up */
	    if (self -> history.drag_direction == DRAG_UP)
	    {
		return;
	    }

	    /* Otherwise we're scrolling the wrong way! */
	    XtRemoveTimeOut(self -> history.drag_timeout);
	    self -> history.drag_timeout = None;
	}

	/* We're dragging up */
	self -> history.drag_direction = DRAG_UP;
    }
    else if (self -> core.height < button_event -> y)
    {
	/* We should be dragging down.  Is a timeout already registered? */
	if (self -> history.drag_timeout != None)
	{
	    /* Bail if we're already scrolling down */
	    if (self -> history.drag_direction == DRAG_DOWN)
	    {
		return;
	    }

	    /* Otherwise we're scrolling the wrong way! */
	    XtRemoveTimeOut(self -> history.drag_timeout);
	    self -> history.drag_timeout = None;
	}

	/* Call the timeout callback to scroll down and set a timeout */
	self -> history.drag_direction = DRAG_DOWN;
    }
    else
    {
	/* We're within the bounds of the widget.  Is a timeout registered? */
	if (self -> history.drag_timeout != None)
	{
	    /* Cancel the timeout */
	    XtRemoveTimeOut(self -> history.drag_timeout);
	    self -> history.drag_timeout = None;
	}

	/* We're not dragging the window around */
	self -> history.drag_direction = DRAG_NONE;
    }

    /* Call the drag timeout callback to update the selection and set a timeout */
    drag_timeout_cb((XtPointer)self, &self -> history.drag_timeout);
}

/* End of the dragging */
static void drag_done(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;

    /* Cancel any pending timeout */
    if (self -> history.drag_timeout != None)
    {
	XtRemoveTimeOut(self -> history.drag_timeout);
	self -> history.drag_timeout = None;
    }
}

/* Select the item under the mouse */
static void do_select(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    XButtonEvent *button_event = (XButtonEvent *)event;
    unsigned int index;

    /* This widget now gets keyboard input */
    XmProcessTraversal(widget, XmTRAVERSE_CURRENT);

    /* Convert the y coordinate into an index and select it */
    index = index_of_y(self, button_event -> y);
    set_selection_index(self, index);
}

/* Select or deselect the item under the pointer */
static void toggle_selection(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    XButtonEvent *button_event = (XButtonEvent *)event;
    unsigned int index;

    /* This widget now gets keyboard input */
    XmProcessTraversal(widget, XmTRAVERSE_CURRENT);

    /* Convert the y coordinate into an index */
    index = (self -> history.y + button_event -> y - self -> history.margin_height) /
	self -> history.line_height;

    /* Are we selecting past the end of the list? */
    if (! (index < self -> history.message_count))
    {
	index = self -> history.message_count - 1;
    }

    /* Is this our current selection? */
    if (self -> history.selection_index == index)
    {
	/* Yes.  Toggle the selection off */
	index = (unsigned int)-1;
    }

    /* Set the selection */
    set_selection_index(self, index);
}


/* Call the attachment callbacks */
static void show_attachment(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;

    /* This widget now gets keyboard input */
    XmProcessTraversal(widget, XmTRAVERSE_CURRENT);

    /* Finish the dragging */
    if (self -> history.drag_timeout != None)
    {
	XtRemoveTimeOut(self -> history.drag_timeout);
	self -> history.drag_timeout = None;
    }

    /* Call the attachment callbacks */
    if (self -> history.selection)
    {
	XtCallCallbackList(
	    widget,
	    self -> history.attachment_callbacks,
	    (XtPointer)self -> history.selection);
    }
}


/* Select the previous item in the history */
static void select_previous(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    unsigned int index;

    /* Is anything selected? */
    if (self -> history.selection_index == (unsigned int)-1)
    {
	/* No.  Prepare to select the last item in the list */
	index = self -> history.message_count;
    }
    else
    {
	/* Yes.  Prepare to select the one before it */
	index = self -> history.selection_index;
    }

    /* Bail if there is no previous item */
    if (index == 0)
    {
	return;
    }

    /* Select the previous item */
    set_selection_index(self, index - 1);
}

/* Select the next item in the history */
static void select_next(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    unsigned int index;

    /* Prepare to select the next item */
    index = self -> history.selection_index + 1;

    /* Bail if there is no next item */
    if (! (index < self -> history.message_count))
    {
	return;
    }

    /* Select the next item */
    set_selection_index(self, index);
}

/* Scroll the window left if we can */
static void scroll_left(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    int increment;

    /* Get the horizontal scrollbar's increment */
    XtVaGetValues(self -> history.hscrollbar, XmNincrement, &increment, NULL);

    /* Move that far to the left */
    set_origin(
	self,
	MAX(self -> history.x - increment, 0),
	self -> history.y,
	1);
}

/* Scroll the window right if we can */
static void scroll_right(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    HistoryWidget self = (HistoryWidget)widget;
    int increment;

    /* Get the scrollbar's increment */
    XtVaGetValues(self -> history.hscrollbar, XmNincrement, &increment, NULL);

    /* Move that far to the right */
    set_origin(
	self,
	MIN(self -> history.x + increment, (self -> history.width - self -> core.width)),
	self -> history.y,
	1);
}


/* Redraw the widget */
static void redraw_all(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;
    Display *display = XtDisplay(widget);
    Window window = XtWindow(widget);
    GC gc = self -> history.gc;
    XGCValues values;
    XRectangle bbox;

    /* Bail if we don't have a valid GC */
    if (gc == None)
    {
	return;
    }

    /* Set up the graphics context */
    values.clip_mask = None;
    values.foreground = self -> core.background_pixel;
    XChangeGC(display, gc, GCClipMask | GCForeground, &values);

    /* And erase the contents of the widget */
    XFillRectangle(
	display, window, gc,
	0, 0,
	self -> core.width, self -> core.height);

    /* Get a bounding box that contains the entire visible portion of the widget */
    bbox.x = 0;
    bbox.y = 0;
    bbox.width = self -> core.width;
    bbox.height = self -> core.height;

    /* And repaint it */
    paint(self, &bbox);
}

/*
 *
 * Method definitions
 *
 */

/* Set the widget's display to be threaded or not */
void HistorySetThreaded(Widget widget, Boolean is_threaded)
{
    HistoryWidget self = (HistoryWidget)widget;
    unsigned int i, index;
    message_t message;

    /* Don't do anything if there's no change */
    if (self -> history.is_threaded == is_threaded)
    {
	return;
    }

    /* Change threaded status */
    self -> history.is_threaded = is_threaded;

    /* Get rid of all of the old message views */
    for (i = 0; i < self -> history.message_count; i++)
    {
	message_view_free(self -> history.message_views[i]);
    }

    /* Create a bunch of new message views accordingly */
    if (is_threaded)
    {
	index = self -> history.message_count - 1;

	/* Clear the selection in case it doesn't show up */
	self -> history.selection_index = (unsigned int)-1;

	/* Traverse the history tree */
	node_populate(
	    self -> history.nodes,
	    self -> history.font,
	    self -> history.message_views,
	    0, &index,
	    self -> history.selection,
	    &self -> history.selection_index);
    }
    else
    {
	/* Create a bunch of new ones */
	index = self -> history.message_index;
	for (i = 0; i < self -> history.message_count; i++)
	{
	    /* Look up the message */
	    message = self -> history.messages[index];

	    /* Wrap it in a message view */
	    self -> history.message_views[i] = message_view_alloc(message, self -> history.font, 0);

	    /* Update the selection index */
	    if (self -> history.selection == message)
	    {
		self -> history.selection_index = i;
	    }

	    index = (index + 1) % self -> history.message_count;
	}
    }

    /* Recompute the dimensions of the widget */
    recompute_dimensions(self);

    /* Redraw the contents of the widget */
    redraw_all(widget);
}

/* Returns whether or not the history is threaded */
Boolean HistoryIsThreaded(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;
    return self -> history.is_threaded;
}

/* Set the widget's display to show timestamps or not */
void HistorySetShowTimestamps(Widget widget, Boolean show_timestamps)
{
    HistoryWidget self = (HistoryWidget)widget;

    /* Update the flag */
    self -> history.show_timestamps = show_timestamps;

    /* Recompute the dimensions of the widget */
    recompute_dimensions(self);

    /* Redraw the whole thing */
    redraw_all(widget);
}

/* Returns whether or not the timestamps are visible */
Boolean HistoryIsShowingTimestamps(Widget widget)
{
    HistoryWidget self = (HistoryWidget)widget;
    return self -> history.show_timestamps;
}


/* Adds a new message to the History */
void HistoryAddMessage(Widget widget, message_t message)
{
    HistoryWidget self = (HistoryWidget)widget;
    node_t node;
    int index;
    int depth;

    /* Wrap the message in a node */
    if ((node = node_alloc(message)) == NULL)
    {
	return;
    }

    /* Add the node to the threaded history tree */
    node_add(&self -> history.nodes,
	     message_get_reply_id(message),
	     node,
	     MIN(self -> history.message_count + 1, self -> history.message_capacity),
	     &index,
	     &depth);

    /* If we're not at capacity then just add a reference to the end
     * of the array of messages */
    if (self -> history.message_count < self -> history.message_capacity)
    {
	self -> history.messages[self -> history.message_count] =
	    message_alloc_reference(message);
    }
    else
    {
	/* Free the old message */
	message_free(self -> history.messages[self -> history.message_index]);

	/* Replace it with this message */
	self -> history.messages[self -> history.message_index] =
	    message_alloc_reference(message);

	self -> history.message_index = (self -> history.message_index + 1) %
	    self -> history.message_capacity;
    }

    /* Add the node according to our threadedness */
    if (HistoryIsThreaded(widget))
    {
	insert_message(self, index, depth, message);
    }
    else
    {
	insert_message(self, self -> history.message_count, 0, message);
    }
}

/* Kills the thread of the given message */
void HistoryKillThread(Widget widget, message_t message)
{
    dprintf(("HistoryKillThread(): not yet implemented\n"));
}

/* Returns whether or not a given message is in a killed thread */
Boolean HistoryIsMessageKilled(Widget widget, message_t message)
{
    dprintf(("HistoryIsMessageKilled(): not yet implemented\n"));
    return False;
}

/* Selects a message in the history */
void HistorySelect(Widget widget, message_t message)
{
    HistoryWidget self = (HistoryWidget)widget;
    unsigned int i;

    /* Save the effort if we're just cancelling a selection */
    if (message != NULL)
    {
	/* Find the index of the message */
	for (i = 0; i < self -> history.message_count; i++)
	{
	    if (message_view_get_message(self -> history.message_views[i]) == message)
	    {
		set_selection(self, i, message);
		return;
	    }
	}
    }

    /* Not found -- select at -1 */
    set_selection(self, (unsigned int)-1, message);
}

/* Returns the selected message or NULL if none is selected */
message_t HistoryGetSelection(Widget widget)
{
    dprintf(("HistoryGetSelection(): not yet implemented\n"));
    return NULL;
}
