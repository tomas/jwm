/****************************************************************************
 * Header for client window functions.
 * Copyright (C) 2004 Joe Wingbermuehle
 ****************************************************************************/

#ifndef CLIENT_H
#define CLIENT_H

#include "border.h"
#include "hint.h"

typedef enum {
	BORDER_NONE    = 0,
	BORDER_OUTLINE = 1,
	BORDER_TITLE   = 2,
	BORDER_MIN     = 4,
	BORDER_MAX     = 8,
	BORDER_CLOSE   = 16,
	BORDER_RESIZE  = 32,
	BORDER_MOVE    = 64
} BorderFlags;

#define BORDER_DEFAULT ( \
		  BORDER_OUTLINE \
		| BORDER_TITLE   \
		| BORDER_MIN     \
		| BORDER_MAX     \
		| BORDER_CLOSE   \
		| BORDER_RESIZE  \
		| BORDER_MOVE    )

typedef enum {
	STAT_NONE      = 0,
	STAT_ACTIVE    = 1 << 0,  /* This client has focus. */
	STAT_MAPPED    = 1 << 1,  /* This client is shown (on some desktop). */
	STAT_MAXIMIZED = 1 << 2,  /* This client is maximized. */
	STAT_HIDDEN    = 1 << 3,  /* This client is not on the current desktop. */
	STAT_STICKY    = 1 << 4,  /* This client is on all desktops. */
	STAT_NOLIST    = 1 << 5,  /* Skip this client in the task list. */
	STAT_MINIMIZED = 1 << 6,  /* This client is minimized. */
	STAT_SHADED    = 1 << 7,  /* This client is shaded. */
	STAT_WMDIALOG  = 1 << 8,  /* This is a JWM dialog window. */
	STAT_PIGNORE   = 1 << 9,  /* Ignore the program-specified position. */
	STAT_SHAPE     = 1 << 10, /* This client uses the shape extension. */
	STAT_SDESKTOP  = 1 << 11  /* This client was minimized to show desktop. */
} StatusFlags;

typedef struct ColormapNode {
	Window window;
	struct ColormapNode *next;
} ColormapNode;

typedef struct AspectRatio {
	int minx, maxx;
	int miny, maxy;
} AspectRatio;

typedef struct ClientNode {

	Window window;
	Window parent;

	Window owner;

	int x, y, width, height;
	int oldx, oldy, oldWidth, oldHeight;

	long sizeFlags;
	int baseWidth, baseHeight;
	int minWidth, minHeight;
	int maxWidth, maxHeight;
	int xinc, yinc;
	AspectRatio aspect;
	int gravity;

	Colormap cmap;
	ColormapNode *colormaps;

	char *name;
	char *instanceName;
	char *className;

	ClientState state;

	BorderActionType borderAction;

	struct IconNode *icon;

	void (*controller)(int wasDestroyed);

	struct ClientNode *prev;
	struct ClientNode *next;

} ClientNode;

extern ClientNode *nodes[LAYER_COUNT];
extern ClientNode *nodeTail[LAYER_COUNT];

ClientNode *FindClientByWindow(Window w);
ClientNode *FindClientByParent(Window p);
ClientNode *GetActiveClient();

void InitializeClients();
void StartupClients();
void ShutdownClients();
void DestroyClients();

ClientNode *AddClientWindow(Window w, int alreadyMapped, int notOwner);
void RemoveClient(ClientNode *np);

void MinimizeClient(ClientNode *np);
void ShadeClient(ClientNode *np);
void UnshadeClient(ClientNode *np);
void SetClientWithdrawn(ClientNode *np);
void RestoreClient(ClientNode *np, int raise);
void MaximizeClient(ClientNode *np);
void FocusClient(ClientNode *np);
void FocusNextStacked(ClientNode *np);
void RefocusClient();
void DeleteClient(ClientNode *np);
void KillClient(ClientNode *np);
void RaiseClient(ClientNode *np);
void RestackClients();
void SetClientLayer(ClientNode *np, unsigned int layer);
void SetClientDesktop(ClientNode *np, unsigned int desktop);
void SetClientSticky(ClientNode *np, int isSticky);

void HideClient(ClientNode *np);
void ShowClient(ClientNode *np);

void UpdateClientColormap(ClientNode *np);

void SetShape(ClientNode *np);

void SendConfigureEvent(ClientNode *np);

void SendClientMessage(Window w, AtomType type, AtomType message);

#endif

