/* xjadeo - jack video monitor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * (c) 2006
 *  Robin Gareus <robin@gareus.org>
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 */
#include "xjadeo.h"

#ifdef HAVE_FT

#include <ft2build.h>
#include FT_FREETYPE_H

/* origin is the upper left corner */
unsigned char ST_image[ST_HEIGHT][ST_WIDTH];
int ST_rightend=0;
int ST_height=0;

static void draw_bitmap(FT_Bitmap*  bitmap,
    FT_Int x,
    FT_Int y)
{
  FT_Int i, j, p, q;
  FT_Int x_max = x + bitmap->width;
  FT_Int y_max = y + bitmap->rows;


  for (i = x, p = 0; i < x_max; i++, p++)
  {
    for (j = y, q = 0; j < y_max; j++, q++)
    {
      if (i >= ST_WIDTH || j >= ST_HEIGHT || i < 0 || j < 0)
	continue;

      ST_image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}

static char *ff = NULL;
static int   initialized = 0;
static FT_Library    library;
static FT_Face       face;

int render_font (char *fontfile, char *text, int px)
{
  static int pxx = 0;
  FT_GlyphSlot  slot;
  FT_Matrix     matrix;                 /* transformation matrix */
  FT_Vector     pen;                    /* untransformed origin  */
  FT_Error      error;
  int           target_height;
  int           n, num_chars;

  /* set up matrix */
  matrix.xx = (FT_Fixed)(0x10000L);
  matrix.xy = (FT_Fixed)(0x0L);
  matrix.yx = (FT_Fixed)(0x0L);
  matrix.yy = (FT_Fixed)(0x10000L);

  if (!ff || strcmp(fontfile, ff) || pxx != px || !initialized) {
    pxx = px;
    free(ff);
    ff = strdup(fontfile);
    if (initialized) {
      FT_Done_Face    (face);
      FT_Done_FreeType(library);
      initialized = 0;
    }

    error = FT_Init_FreeType(&library);              /* initialize library */
    if (error) return(-1);

    error = FT_New_Face(library, fontfile, 0, &face); /* create face object */
    if (error) {
      FT_Done_FreeType(library);
      return(-1);
    }

    error = FT_Set_Char_Size(face, 0, px * 64, 0, 72);  /* set character size */
    if (error) {
      FT_Done_Face    (face);
      FT_Done_FreeType(library);
      return(-1);
    }

    initialized = 1;
  }

  /* the pen position incartesian space coordinates; */
  pen.x = 1  * 64;
  pen.y = 10 * 64;

  num_chars     = strlen(text);
  target_height = ST_HEIGHT;
  slot = face->glyph;

  memset(&(ST_image[0][0]),0,ST_WIDTH*ST_HEIGHT);
  ST_rightend=0;
  ST_height = 0;

  for (n = 0; n < num_chars; n++)
  {
    /* set transformation */
    FT_Set_Transform(face, &matrix, &pen);

    /* load glyph image into the slot (erase previous one) */
    error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
    if (error) continue;  /* ignore errors */

    /* now, draw to our target surface (convert position) */
    draw_bitmap(&slot->bitmap,
	slot->bitmap_left,
	target_height - slot->bitmap_top);

    const int height = slot->bitmap_top + (slot->bitmap_top - slot->bitmap.rows)/2.0;
    if (height > ST_height) {
      ST_height = height;
      if (ST_height > ST_HEIGHT) ST_height = ST_HEIGHT;
    }
    if ((slot->bitmap_left + slot->bitmap.width) > ST_WIDTH)
      break;

    /* increment pen position */
    pen.x += slot->advance.x;
    pen.y += slot->advance.y;

    ST_rightend=pen.x/64;
  }

  ST_rightend+=1;

  return 0;
}

void free_freetype () {
  free(ff);
  if (initialized) {
    FT_Done_Face    (face);
    FT_Done_FreeType(library);
  }
  initialized = 0;
}

#else  /* No freetype */

unsigned char ST_image[1][1];
int ST_rightend = 0;
int ST_height = 0;
int render_font (char *fontfile, char *text, int px) {return -1;}
void free_freetype () { ; }

#endif /* HAVE_FT*/
