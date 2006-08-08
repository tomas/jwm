/****************************************************************************
 * Functions for displaying popup windows.
 * Copyright (C) 2004 Joe Wingbermuehle
 ****************************************************************************/

#include "jwm.h"
#include "popup.h"
#include "main.h"
#include "color.h"
#include "font.h"
#include "screen.h"
#include "cursor.h"
#include "error.h"
#include "timing.h"
#include "misc.h"

#define DEFAULT_POPUP_DELAY 600

typedef struct PopupType {
	int isActive;
	int x, y;   /* The coordinates of the upper-left corner of the popup. */
	int mx, my; /* The mouse position when the popup was created. */
	int width, height;
	char *text;
	Window window;
} PopupType;

static PopupType popup;
static int popupEnabled;
int popupDelay;

static void DrawPopup();

/****************************************************************************
 ****************************************************************************/
void InitializePopup() {
	popupDelay = DEFAULT_POPUP_DELAY;
	popupEnabled = 1;
}

/****************************************************************************
 ****************************************************************************/
void StartupPopup() {
	popup.isActive = 0;
	popup.text = NULL;
	popup.window = None;
}

/****************************************************************************
 ****************************************************************************/
void ShutdownPopup() {
	if(popup.text) {
		Release(popup.text);
		popup.text = NULL;
	}
	if(popup.window != None) {
		JXDestroyWindow(display, popup.window);
		popup.window = None;
	}
}

/****************************************************************************
 ****************************************************************************/
void DestroyPopup() {
}

/****************************************************************************
 * Show a popup window.
 * x - The x coordinate of the popup window.
 * y - The y coordinate of the popup window.
 * text - The text to display in the popup.
 ****************************************************************************/
void ShowPopup(int x, int y, const char *text) {

	unsigned long attrMask;
	XSetWindowAttributes attr;
	const ScreenType *sp;

	Assert(text);

	if(!popupEnabled) {
		return;
	}

	if(popup.text) {
		Release(popup.text);
		popup.text = NULL;
	}

	if(!strlen(text)) {
		return;
	}

	popup.text = CopyString(text);

	popup.height = GetStringHeight(FONT_POPUP);
	popup.width = GetStringWidth(FONT_POPUP, popup.text);

	popup.height += 2;
	popup.width += 8; 

	sp = GetCurrentScreen(x, y);

	if(popup.width > sp->width) {
		popup.width = sp->width;
	}

	popup.x = x;
	popup.y = y - popup.height - 2;

	if(popup.width + popup.x >= sp->width) {
		popup.x = sp->width - popup.width - 2;
	}
	if(popup.height + popup.y >= sp->height) {
		popup.y = sp->height - popup.height - 2;
	}

	if(popup.window == None) {

		attrMask = 0;

		attrMask |= CWEventMask;
		attr.event_mask
			= ExposureMask
			| PointerMotionMask | PointerMotionHintMask;

		attrMask |= CWSaveUnder;
		attr.save_under = True;

		attrMask |= CWBackPixel;
		attr.background_pixel = colors[COLOR_POPUP_BG];

		attrMask |= CWBorderPixel;
		attr.border_pixel = colors[COLOR_POPUP_OUTLINE];

		attrMask |= CWDontPropagate;
		attr.do_not_propagate_mask
			= PointerMotionMask
			| ButtonPressMask
			| ButtonReleaseMask;

		popup.window = JXCreateWindow(display, rootWindow, popup.x, popup.y,
			popup.width, popup.height, 1, CopyFromParent,
			InputOutput, CopyFromParent, attrMask, &attr);

	} else {
		JXMoveResizeWindow(display, popup.window, popup.x, popup.y,
			popup.width, popup.height);
	}

	popup.mx = x;
	popup.my = y;

	if(!popup.isActive) {
		JXMapRaised(display, popup.window);
		popup.isActive = 1;
	} else {
		DrawPopup();
	}

}

/****************************************************************************
 ****************************************************************************/
void SetPopupEnabled(int e) {
	popupEnabled = e;
}

/****************************************************************************
 ****************************************************************************/
void SetPopupDelay(const char *str) {

	int temp;

	if(str == NULL) {
		return;
	}

	temp = atoi(str);

	if(temp < 0) {
		Warning("invalid popup delay specified: %s\n", str);
	} else {
		popupDelay = temp;
	}

}

/****************************************************************************
 ****************************************************************************/
void SignalPopup(const TimeType *now, int x, int y) {

	if(popup.isActive) {
		if(abs(popup.mx - x) > 2 || abs(popup.my - y) > 2) {
			JXUnmapWindow(display, popup.window);
			popup.isActive = 0;
		}
	}

}

/****************************************************************************
 ****************************************************************************/
int ProcessPopupEvent(const XEvent *event) {

	if(popup.isActive && event->type == Expose) {
		if(event->xexpose.window == popup.window) {
			DrawPopup();
			return 1;
		}
	}

	return 0;

}

/****************************************************************************
 ****************************************************************************/
void DrawPopup() {

	Assert(popup.isActive);

	JXClearWindow(display, popup.window);
	RenderString(popup.window, FONT_POPUP, COLOR_POPUP_FG, 4, 1,
		popup.width, NULL, popup.text);

}

