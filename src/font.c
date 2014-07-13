/**
 * @file font.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions to handle fonts.
 *
 */

#include "jwm.h"
#include "font.h"
#include "main.h"
#include "error.h"
#include "color.h"
#include "misc.h"

#ifdef HAVE_LANGINFO_H
#  include <langinfo.h>
#endif
#ifdef HAVE_ICONV_H
#  include <iconv.h>
#endif

#ifdef USE_XFT
static const char *DEFAULT_FONT = "FreeSans-9";
#else
static const char *DEFAULT_FONT = "fixed";
#endif

static char *GetUTF8String(const char *str);
static void ReleaseUTF8String(char *utf8String);

static char *fontNames[FONT_COUNT];

#ifdef USE_ICONV
static iconv_t conversionDescriptor = (iconv_t)-1;
#endif

#ifdef USE_XFT
static XftFont *fonts[FONT_COUNT];
static XftDraw *xd;
#else
static XFontStruct *fonts[FONT_COUNT];
static GC fontGC;
#endif

/** Initialize font data. */
void InitializeFonts(void)
{
#ifdef USE_ICONV
   const char *codeset;
#endif
   unsigned int x;

   for(x = 0; x < FONT_COUNT; x++) {
      fonts[x] = NULL;
      fontNames[x] = NULL;
   }

   /* Allocate a conversion descriptor if we're not using UTF-8. */
#ifdef USE_ICONV
   codeset = nl_langinfo(CODESET);
   if(strcmp(codeset, "UTF-8")) {
      conversionDescriptor = iconv_open("UTF-8", codeset);
   } else {
      conversionDescriptor = (iconv_t)-1;
   }
#endif

}

/** Startup font support. */
void StartupFonts(void)
{

#ifndef USE_XFT
   XGCValues gcValues;
   unsigned long gcMask;
#endif
   unsigned int x;

   /* Inherit unset fonts from the tray for tray items. */
   if(!fontNames[FONT_TASK]) {
      fontNames[FONT_TASK] = CopyString(fontNames[FONT_TRAY]);
   }
   if(!fontNames[FONT_TRAYBUTTON]) {
      fontNames[FONT_TRAYBUTTON] = CopyString(fontNames[FONT_TRAY]);
   }
   if(!fontNames[FONT_CLOCK]) {
      fontNames[FONT_CLOCK] = CopyString(fontNames[FONT_TRAY]);
   }
   if(!fontNames[FONT_PAGER]) {
      fontNames[FONT_PAGER] = CopyString(fontNames[FONT_TRAY]);
   }

#ifdef USE_XFT

   for(x = 0; x < FONT_COUNT; x++) {
      if(fontNames[x]) {
         fonts[x] = JXftFontOpenName(display, rootScreen, fontNames[x]);
         if(!fonts[x]) {
            fonts[x] = JXftFontOpenXlfd(display, rootScreen, fontNames[x]);
         }
         if(JUNLIKELY(!fonts[x])) {
            Warning(_("could not load font: %s"), fontNames[x]);
         }
      }
      if(!fonts[x]) {
         fonts[x] = JXftFontOpenName(display, rootScreen, DEFAULT_FONT);
      }
      if(JUNLIKELY(!fonts[x])) {
         FatalError(_("could not load the default font: %s"), DEFAULT_FONT);
      }
   }

   xd = XftDrawCreate(display, rootWindow, rootVisual, rootColormap);

#else /* USE_XFT */

   for(x = 0; x < FONT_COUNT; x++) {
      if(fontNames[x]) {
         fonts[x] = JXLoadQueryFont(display, fontNames[x]);
         if(JUNLIKELY(!fonts[x] && fontNames[x])) {
            Warning(_("could not load font: %s"), fontNames[x]);
         }
      }
      if(!fonts[x]) {
         fonts[x] = JXLoadQueryFont(display, DEFAULT_FONT);
      }
      if(JUNLIKELY(!fonts[x])) {
         FatalError(_("could not load the default font: %s"), DEFAULT_FONT);
      }
   }

   gcMask = GCGraphicsExposures;
   gcValues.graphics_exposures = False;
   fontGC = JXCreateGC(display, rootWindow, gcMask, &gcValues);

#endif /* USE_XFT */

}

/** Shutdown font support. */
void ShutdownFonts(void)
{
   unsigned int x;
   for(x = 0; x < FONT_COUNT; x++) {
      if(fonts[x]) {
#ifdef USE_XFT
         JXftFontClose(display, fonts[x]);
#else
         JXFreeFont(display, fonts[x]);
#endif
         fonts[x] = NULL;
      }
   }

#ifdef USE_XFT
   XftDrawDestroy(xd);
#else
   JXFreeGC(display, fontGC);
#endif
}

/** Destroy font data. */
void DestroyFonts(void)
{
   unsigned int x;
   for(x = 0; x < FONT_COUNT; x++) {
      if(fontNames[x]) {
         Release(fontNames[x]);
         fontNames[x] = NULL;
      }
   }
#ifdef USE_ICONV
   if(conversionDescriptor != (iconv_t)-1) {
      iconv_close(conversionDescriptor);
   }
#endif
}

/** Convert a string to UTF-8. */
char *GetUTF8String(const char *str)
{
   char *utf8String;
#ifdef USE_ICONV
   if(conversionDescriptor == (iconv_t)-1) {
      utf8String = (char*)str;
   } else {
      char *inBuf = (char*)str;
      char *outBuf;
      size_t inLeft = strlen(str);
      size_t outLeft = 4 * inLeft;
      size_t rc;
      utf8String = Allocate(outLeft + 1);
      outBuf = utf8String;
      rc = iconv(conversionDescriptor, &inBuf, &inLeft, &outBuf, &outLeft);
      if(rc == (size_t)-1) {
         Warning("iconv failed");
         iconv_close(conversionDescriptor);
         conversionDescriptor = (iconv_t)-1;
         utf8String = (char*)str;
      } else {
         *outBuf = 0;
      }
   }
#else
   utf8String = (char*)str;
#endif
   return utf8String;
}

/** Release a UTF-8 string. */
void ReleaseUTF8String(char *utf8String)
{
#ifdef USE_ICONV
   if(conversionDescriptor != (iconv_t)-1) {
      Release(utf8String);
   }
#endif
}

/** Get the width of a string. */
int GetStringWidth(FontType ft, const char *str)
{
#ifdef USE_XFT
   XGlyphInfo extents;
#endif
#ifdef USE_FRIBIDI
   FriBidiChar *temp_i;
   FriBidiChar *temp_o;
   FriBidiParType type = FRIBIDI_PAR_ON;
   int unicodeLength;
#endif
   int len;
   char *output;
   int result;
   char *utf8String;

   /* Convert to UTF-8 if necessary. */
   utf8String = GetUTF8String(str);

   /* Length of the UTF-8 string. */
   len = strlen(utf8String);

   /* Apply the bidi algorithm if requested. */
#ifdef USE_FRIBIDI
   temp_i = AllocateStack((len + 1) * sizeof(FriBidiChar));
   temp_o = AllocateStack((len + 1) * sizeof(FriBidiChar));
   unicodeLength = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8,
                                              utf8String, len, temp_i);
   fribidi_log2vis(temp_i, unicodeLength, &type, temp_o, NULL, NULL, NULL);
   output = AllocateStack(4 * len + 1);
   fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, temp_o, unicodeLength,
                              (char*)output);
   len = strlen(output);
#else
   output = utf8String;
#endif

   /* Get the width of the string. */
#ifdef USE_XFT
   JXftTextExtentsUtf8(display, fonts[ft], (const unsigned char*)output,
                       len, &extents);
   result = extents.xOff;
#else
   result = XTextWidth(fonts[ft], output, len);
#endif

   /* Clean up. */
#ifdef USE_FRIBIDI
   ReleaseStack(temp_i);
   ReleaseStack(temp_o);
   ReleaseStack(output);
#endif
   ReleaseUTF8String(utf8String);

   return result;
}

/** Get the height of a string. */
int GetStringHeight(FontType ft)
{
   Assert(fonts[ft]);
   return fonts[ft]->ascent + fonts[ft]->descent;
}

/** Set the font to use for a component. */
void SetFont(FontType type, const char *value)
{
   if(JUNLIKELY(!value)) {
      Warning(_("empty Font tag"));
      return;
   }
   if(fontNames[type]) {
      Release(fontNames[type]);
   }
   fontNames[type] = CopyString(value);
}

/** Display a string. */
void RenderString(Drawable d, FontType font, ColorType color,
                  int x, int y, int width, const char *str)
{

#ifdef USE_ICONV
   static char isUTF8 = -1;
#endif
   XRectangle rect;
   Region renderRegion;
   int len;
   char *output;
#ifdef USE_FRIBIDI
   FriBidiChar *temp_i;
   FriBidiChar *temp_o;
   FriBidiParType type = FRIBIDI_PAR_ON;
   int unicodeLength;
#endif
#ifdef USE_XFT
   XGlyphInfo extents;
#endif
   char *utf8String;

   /* Early return for empty strings. */
   if(!str || !str[0]) {
      return;
   }

   /* Convert to UTF-8 if necessary. */
   utf8String = GetUTF8String(str);

   /* Get the length of the UTF-8 string. */
   len = strlen(utf8String);

   /* Apply the bidi algorithm if requested. */
#ifdef USE_FRIBIDI
   temp_i = AllocateStack((len + 1) * sizeof(FriBidiChar));
   temp_o = AllocateStack((len + 1) * sizeof(FriBidiChar));
   unicodeLength = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8,
                                              utf8String, len, temp_i);
   fribidi_log2vis(temp_i, unicodeLength, &type, temp_o, NULL, NULL, NULL);
   output = AllocateStack(4 * len + 1);
   fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, temp_o, unicodeLength,
                              (char*)output);
   len = strlen(output);
#else
   output = utf8String;
#endif

   /* Get the bounds for the string based on the specified width. */
   rect.x = x;
   rect.y = y;
   rect.height = GetStringHeight(font);
#ifdef USE_XFT
   JXftTextExtentsUtf8(display, fonts[font], (const unsigned char*)output,
                       len, &extents);
   rect.width = extents.xOff;
#else
   rect.width = XTextWidth(fonts[font], output, len);
#endif
   rect.width = Min(rect.width, width) + 2;

   /* Combine the width bounds with the region to use. */
   renderRegion = XCreateRegion();
   XUnionRectWithRegion(&rect, renderRegion, renderRegion);

   /* Display the string. */
#ifdef USE_XFT
   JXftDrawChange(xd, d);
   JXftDrawSetClip(xd, renderRegion);
   JXftDrawStringUtf8(xd, GetXftColor(color), fonts[font],
                      x, y + fonts[font]->ascent,
                      (const unsigned char*)output, len);
   JXftDrawChange(xd, rootWindow);
#else
   JXSetForeground(display, fontGC, colors[color]);
   JXSetRegion(display, fontGC, renderRegion);
   JXSetFont(display, fontGC, fonts[font]->fid);
   JXDrawString(display, d, fontGC, x, y + fonts[font]->ascent, output, len);
#endif

   /* Free any memory used for UTF conversion. */
#ifdef USE_FRIBIDI
   ReleaseStack(temp_i);
   ReleaseStack(temp_o);
   ReleaseStack(output);
#endif
   ReleaseUTF8String(utf8String);

   XDestroyRegion(renderRegion);

}
