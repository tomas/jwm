/****************************************************************************
 * Functions to handle swallowing client windows in the tray.
 ****************************************************************************/

#include "jwm.h"
#include "swallow.h"
#include "main.h"
#include "tray.h"
#include "error.h"
#include "root.h"
#include "color.h"
#include "client.h"
#include "event.h"

/* Spend 15 seconds looking. */
#define FIND_SECONDS 15

/* Check 30 times. */
#define FIND_RETRY_COUNT 30

#define FIND_USLEEP ((FIND_SECONDS * 1000 * 1000) / FIND_RETRY_COUNT)

typedef struct SwallowNode {

	TrayComponentType *cp;

	char *name;
	char *command;
	int started;
	int border;
	int queued;

	struct SwallowNode *next;

} SwallowNode;

static SwallowNode *swallowNodes;

static void QueueStartup(TrayComponentType *cp);
static void AwaitStartup(TrayComponentType *cp);
static Window FindSwallowedClient(const char *name);

static void Create(TrayComponentType *cp);
static void Destroy(TrayComponentType *cp);
static void Resize(TrayComponentType *cp);

/****************************************************************************
 ****************************************************************************/
void InitializeSwallow() {
	swallowNodes = NULL;
}

/****************************************************************************
 ****************************************************************************/
void StartupSwallow() {

	SwallowNode *np;

	for(np = swallowNodes; np; np = np->next) {
		QueueStartup(np->cp);
	}
	for(np = swallowNodes; np; np = np->next) {
		AwaitStartup(np->cp);
	}

}

/****************************************************************************
 ****************************************************************************/
void ShutdownSwallow() {
}

/****************************************************************************
 ****************************************************************************/
void DestroySwallow() {

	SwallowNode *np;

	while(swallowNodes) {

		np = swallowNodes->next;

		Assert(swallowNodes->name);
		Release(swallowNodes->name);

		if(swallowNodes->command) {
			Release(swallowNodes->command);
		}

		Release(swallowNodes);
		swallowNodes = np;

	}

}

/****************************************************************************
 ****************************************************************************/
TrayComponentType *CreateSwallow(const char *name, const char *command,
	int width, int height) {

	TrayComponentType *cp;
	SwallowNode *np;

	if(!name) {
		Warning("cannot swallow a client with no name");
		return NULL;
	}

	/* Make sure this name isn't already used. */
	for(np = swallowNodes; np; np = np->next) {
		if(!strcmp(np->name, name)) {
			Warning("cannot swallow the same client multiple times");
			return NULL;
		}
	}

	np = Allocate(sizeof(SwallowNode));
	np->name = Allocate(strlen(name) + 1);
	strcpy(np->name, name);
	if(command) {
		np->command = Allocate(strlen(command) + 1);
		strcpy(np->command, command);
	} else {
		np->command = NULL;
	}
	np->started = 0;
	np->queued = 0;

	np->next = swallowNodes;
	swallowNodes = np;

	cp = CreateTrayComponent();
	np->cp = cp;
	cp->object = np;
	cp->Create = Create;
	cp->Destroy = Destroy;
	cp->Resize = Resize;

	cp->requestedWidth = width;
	cp->requestedHeight = height;

	return cp;

}

/****************************************************************************
 ****************************************************************************/
int ProcessSwallowEvent(const XEvent *event) {

	SwallowNode *np;

	for(np = swallowNodes; np; np = np->next) {
		if(event->xany.window == np->cp->window) {
			switch(event->type) {
			case DestroyNotify:
				np->cp->window = None;
				break;
			case ConfigureNotify:
				np->cp->requestedWidth = event->xconfigure.width;
				np->cp->requestedHeight = event->xconfigure.height;
				ResizeTray(np->cp->tray);
				break;
			default:
				break;
			}
			return 1;
		}
	}

	return 0;

}

/****************************************************************************
 ****************************************************************************/
void Create(TrayComponentType *cp) {

	int width, height;

	SwallowNode *np = (SwallowNode*)cp->object;

	if(cp->window != None) {
		width = cp->width - np->border * 2;
		height = cp->height - np->border * 2;
		JXResizeWindow(display, cp->window, width, height);
	}

}

/****************************************************************************
 ****************************************************************************/
void Resize(TrayComponentType *cp) {

	int width, height;

	SwallowNode *np = (SwallowNode*)cp->object;

	if(cp->window != None) {
		width = cp->width - np->border * 2;
		height = cp->height - np->border * 2;
		JXResizeWindow(display, cp->window, width, height);
	}

}

/****************************************************************************
 ****************************************************************************/
void Destroy(TrayComponentType *cp) {

	if(cp->window) {
		JXReparentWindow(display, cp->window, rootWindow, 0, 0);
		JXRemoveFromSaveSet(display, cp->window);
		SendClientMessage(cp->window, ATOM_WM_PROTOCOLS, ATOM_WM_DELETE_WINDOW);
	}

}

/****************************************************************************
 ****************************************************************************/
void QueueStartup(TrayComponentType *cp) {

	SwallowNode *np;

	np = (SwallowNode*)cp->object;

	Assert(np);

	cp->window = FindSwallowedClient(np->name);
	if(cp->window == None) {

		Debug("starting %s", np->name);

		if(!np->command) {
			Warning("client to be swallowed (%s) not found and no command given",
				np->name);
			cp->requestedWidth = 1;
			cp->requestedHeight = 1;
		} else {
			RunCommand(np->command);
			np->queued = 1;
		}

	}

}

/****************************************************************************
 ****************************************************************************/
void AwaitStartup(TrayComponentType *cp) {

	SwallowNode *np;
	XWindowAttributes attributes;
	int x;

	np = (SwallowNode*)cp->object;

	Assert(np);

	if(cp->window == None) {

		Debug("waiting for %s", np->name);

		for(x = 0; x < FIND_RETRY_COUNT; x++) {
			cp->window = FindSwallowedClient(np->name);
			if(cp->window != None) {
				break;
			}
			usleep(FIND_USLEEP);
		}

		if(cp->window == None) {
			Warning("%s not found after running %s", np->name, np->command);
			cp->requestedWidth = 1;
			cp->requestedHeight = 1;
			return;
		}

	}

	if(!JXGetWindowAttributes(display, cp->window, &attributes)) {
		attributes.width = 0;
		attributes.height = 0;
		attributes.border_width = 0;
	}
	np->border = attributes.border_width;

	if(cp->requestedWidth < 0 || cp->requestedWidth > rootWidth) {
		Warning("invalid width for swallow: %d", cp->requestedWidth);
		cp->requestedWidth = 0;
	}
	if(cp->requestedHeight < 0 || cp->requestedHeight > rootHeight) {
		Warning("invalid height for swallow: %d", cp->requestedHeight);
		cp->requestedHeight = 0;
	}

	if(cp->requestedWidth == 0) {
		cp->requestedWidth = attributes.width + 2 * np->border;
	}
	if(cp->requestedHeight == 0) {
		cp->requestedHeight = attributes.height + 2 * np->border;
	}

	Debug("%s swallowed", np->name);

}

/****************************************************************************
 ****************************************************************************/
Window FindSwallowedClient(const char *name) {

	XClassHint hint;
	XWindowAttributes attr;
	Window rootReturn, parentReturn, *childrenReturn;
	Window result;
	unsigned int childrenCount;
	unsigned int x;

	Assert(name);

	result = None;

	/* Process any pending events before searching. */
	HandleStartupEvents();

	JXFlush(display);

	JXQueryTree(display, rootWindow, &rootReturn, &parentReturn,
			&childrenReturn, &childrenCount);

	for(x = 0; x < childrenCount; x++) {

		if(!JXGetWindowAttributes(display, childrenReturn[x], &attr)) {
			continue;
		}

		if(attr.map_state != IsViewable || attr.override_redirect == True) {
			continue;
		}

		if(JXGetClassHint(display, childrenReturn[x], &hint)) {
			if(!strcmp(hint.res_name, name)) {
				result = childrenReturn[x];
				JXAddToSaveSet(display, result);
				JXSetWindowBorder(display, result, colors[COLOR_TRAY_BG]);
				JXMapRaised(display, result);
				JXSelectInput(display, result, StructureNotifyMask);
				JXFree(hint.res_name);
				JXFree(hint.res_class);
				break;
			}
			JXFree(hint.res_name);
			JXFree(hint.res_class);
		}
	}

	JXFree(childrenReturn);

	return result;

}


