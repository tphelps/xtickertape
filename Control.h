/* $Id: Control.h,v 1.2 1997/02/15 02:32:16 phelps Exp $ */

#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "Message.h"

typedef void (*ControlPanelCallback)(Message message, void *context);
typedef struct ControlPanel_t *ControlPanel;

/* Constructs the Tickertape Control Panel */
ControlPanel ControlPanel_alloc(Widget parent, ControlPanelCallback callback, void *context);

/* Releases the resources used by the receiver */
void ControlPanel_free(ControlPanel self);

/* Makes the ControlPanel window visible */
void ControlPanel_show(ControlPanel self);

/* Answers the receiver's values as a Message */
Message ControlPanel_createMessage(ControlPanel self);

/* Handle notifications */
void ControlPanel_handleNotify(ControlPanel self, Widget widget);

#endif /* CONTROLPANEL_H */
