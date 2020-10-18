#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <sys/mman.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library library;

typedef int ETextAlign;

typedef struct {
  uint8_t pad_0x0[0x18];
  uint16_t m_nWidth;            // 0x18
  uint16_t m_nHeight;           // 0x1A
  uint32_t m_nBitsPerComponent; // 0x1C
  void *m_pData;                // 0x20
  uint8_t m_bHasAlpha;          // 0x28 or 0x29
  uint8_t m_bPreMulti;          // 0x29 or 0x28
} CCImage;

// Functions within Pro Pinball 1.2.3 for macOS
static void* CCImage_initWithString = 0x10013f48a;
static void *(*cpp_new)(unsigned long size) = 0x10019f2f4; // operator new[](unsigned long)
static void (*cpp_delete)(void *ptr) = 0x10019f2e8; // operator delete[](void*)

unsigned int pot(unsigned int v) {
  assert(v != 0);
  unsigned int r = 1;
  while (r < v) {
    r *= 2;
  }
  return r;
}

unsigned int max(unsigned int a, unsigned int b) {
  return (a > b) ? a : b;
}

// Reimplementation of cocos2dx 2.6.6 CCImage::initWithString (platform/CCImage.h) MacOS backend
static bool CCImage_initWithString_hook(CCImage *_this, const char *pText,
                                   int nWidth, int nHeight,
                                   ETextAlign eAlignMask, const char *pFontName,
                                   int nSize) {
  int error;

  assert(!strcmp(pFontName, "Oswald-Regular"));
  // assert(nWidth == 0);
  // assert(nHeight == 0);
  assert(nSize > 0);
  assert((eAlignMask == (0x10 | 0x2 | 0x1))    // kAlignTop
      || (eAlignMask == (0x20 | 0x10 | 0x2))); // kAlignRight

  printf("Trying to create texture for '%s', in font '%s', size %d, image %dx%d; align: %d\n",
         pText, pFontName, nSize, nWidth, nHeight, eAlignMask);

  unsigned int POTWide = 0;
  unsigned int POTHigh = 0;

#if 0
	// Allocate a buffer for the texture
	unsigned char* dataNew = cpp_new(textureSize); //  new unsigned char[texture_size]
	if (dataNew) {
	    return false;
	}
#endif

  FT_Face face;

  error = FT_New_Face(library, "/Applications/Pro Pinball.app/Contents/Resources/fonts/Oswald-Regular.ttf", 0, &face);
  if (error == FT_Err_Unknown_File_Format) {
    // The font file could be opened and read, but it appears that its font format is unsupported
    assert(false);
  } else if (error) {
    // Another error code means that the font file could not be opened or read, or that it is broken...
    assert(false);
  }

  FT_Select_Charmap(face, ft_encoding_unicode);

  FT_GlyphSlot slot = face->glyph;

  unsigned int line_height = nSize; // FIXME: Can we retrieve line_height elsewhere?

#if 0
error = FT_Set_Char_Size(
          face,    /* handle to face object           */
          0,       /* char_width in 1/64th of points  */
          16*64,   /* char_height in 1/64th of points */
          300,     /* horizontal device resolution    */
          300 );   /* vertical device resolution      */
#else
  error = FT_Set_Pixel_Sizes(face,         /* handle to face object */
                             0,            /* pixel_width           */
                             line_height); /* pixel_height          */
#endif
  if (error) {
    assert(false);
  }

  // Used for finding the buffer size
  // FIXME: Shouldn't be necessary, we can use the real vars directly
  int bound_w = 0;
  int bound_h = 0;

  // Offset for alignment
  unsigned int dx = 0;
  unsigned int dy = 0;

  // This will be the pointer to the pixel data
  uint8_t *data = NULL;

  // Get number of glyphs
  unsigned int num_chars = strlen(pText);

  // Do 2 passes:
  //   1. Find the size of the buffer
  //   2. Draw to the buffer
  for (int pass = 0; pass < 2; pass++) {

    if (pass == 1) {
      // Pro Pinball assumes RGBA
      size_t textureSize = POTWide * POTHigh * 4;

      // Allocate buffer and fill it with transparent pixels
      data = cpp_new(textureSize);
      memset(data, 0x00, textureSize);
    }

    // Offset for the current glyph
    unsigned int pen_x = 0;
    unsigned int pen_y = 0;

    // Loop over all glyphs
    for (unsigned int n = 0; n < num_chars; n++) {

      // Handle linebreaks
      if (pText[n] == '\n') {
        pen_x = 0;
        pen_y += line_height;
        continue;
      }

      // Load glyph image into the slot (erase previous one)
      error = FT_Load_Char(face, pText[n], FT_LOAD_RENDER);
      if (error) {
        //FIXME: Handle errors somehow?
        continue;
      }

      // Now, draw to our target surface
      FT_Bitmap *bm = &slot->bitmap;
      assert(bm->pixel_mode == FT_PIXEL_MODE_GRAY);
      assert(bm->num_grays == 256);
      uint8_t *src_data = bm->buffer;
      printf("glyph is %dx%d at %d, %d + %d, %d\n", bm->width, bm->rows, pen_x, pen_y, slot->bitmap_left, slot->bitmap_top);
      for (unsigned int y = 0; y < bm->rows; y++) {

        int ay = dy + pen_y + y + line_height - slot->bitmap_top;
        if ((ay < 0) || (ay >= POTHigh)) {
          bound_h = max(ay, bound_h);
          if (pass == 1) {
            continue;
          }
        }

        for (unsigned int x = 0; x < bm->width; x++) {

          int ax = dx + pen_x + x + slot->bitmap_left;
          if (pass == 0) {
            if (ax < 0) {
              ax = 0;
            }
          }
          
          // FIXME: This is a dirty hack for negative bitmap_left; we should move the entire space
          if ((ax < 0) || (ax >= POTWide)) {
            bound_w = max(ax, bound_w);
            continue;
          }

          // Copy glyph to our output
          if (pass == 1) {
            uint8_t gray = src_data[y * bm->pitch + x];
            for (unsigned int channel = 0; channel < 4; channel++) {
              data[(ay * POTWide + ax) * 4 + channel] = gray; // FIXME: Mix?
            }
          }
        }
      }

      // Increment pen position
      pen_x += slot->advance.x >> 6;
    }

    // In the first pass, we have to generate the size of the buffer and alignment offsets
    POTWide = bound_w; // pot(bound_w);
    POTHigh = bound_h; // pot(bound_h);
    if (eAlignMask == 0x13) {
      dx = (POTWide - bound_w) / 2;
      dy = POTHigh - bound_h;
    } else if (eAlignMask == 0x32) {
      dx = POTWide - bound_w;
      dy = (POTHigh - bound_h) / 2;
    } else {
      assert(false);
    }

    printf("Allocated %dx%d surface for %dx%d content\n", POTWide, POTHigh,
           bound_w, bound_h);
  }

  FT_Done_Face(face);

  _this->m_nHeight = (short)POTHigh;
  _this->m_nWidth = (short)POTWide;
  _this->m_nBitsPerComponent = 8; // info.bitsPerComponent;
  _this->m_bHasAlpha = true;      // info.hasAlpha;
  _this->m_bPreMulti = true;      // info.isPremultipliedAlpha;
  if (_this->m_pData) {
    cpp_delete(_this->m_pData);
  }
  _this->m_pData = data;

  return true;
}

static void install_detour(void* at, void* function) {
  uint64_t detour_address = (uint64_t)at;
  mprotect(detour_address & ~0xFFF, 0x2000, PROT_WRITE | PROT_EXEC | PROT_READ);
  *(uint8_t *)(detour_address + 0) = 0x48;
  *(uint8_t *)(detour_address + 1) = 0xB8;
  *(uint64_t *)(detour_address + 2) = (uint64_t)function;
  *(uint8_t *)(detour_address + 10) = 0xFF;
  *(uint8_t *)(detour_address + 11) = 0xE0;
}

__attribute__((constructor)) void inject(void) {
  if (*(unsigned long long *)0x10013fbb6 != 0x4589480005f739e8) {
    printf("Couldn't find Pro Pinball?!\n");
    return;
  }

  int error;
  error = FT_Init_FreeType(&library);
  if (error) {
    assert(false);
  }
  // FIXME: Do FT_Done_FreeType( library ); in destructor?
  
  install_detour(CCImage_initWithString, CCImage_initWithString_hook);
}
