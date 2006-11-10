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
 *
 * this is basically the freetype2 rendering example
 *
 * TODO: 
 * - init the library and load font file only once
 * - merge the draw_bitmap() with OSD_renderXXX() 
 * - xjadeo configurable Font size 
 * - proper error handling
 *
 */
#include "xjadeo.h"

#ifdef HAVE_FT

#include <ft2build.h>
#include FT_FREETYPE_H


/* origin is the upper left corner */
unsigned char ST_image[ST_HEIGHT][ST_WIDTH];
int ST_rightend=0;


void
draw_bitmap( FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y)
{
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;


  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
      if ( i >= ST_WIDTH || j >= ST_HEIGHT || i<0 || j<0)
        continue;

      ST_image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}

int render_font (char *fontfile, char *text)
{
  FT_Library    library;
  FT_Face       face;

  FT_GlyphSlot  slot;
  FT_Matrix     matrix;                 /* transformation matrix */
//FT_UInt       glyph_index;
  FT_Vector     pen;                    /* untransformed origin  */
  FT_Error      error;

  double        angle;
  int           target_height;
  int           n, num_chars;


  num_chars     = strlen( text );
  angle         = ( 0.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
  target_height = ST_HEIGHT;

  error = FT_Init_FreeType( &library );              /* initialize library */
  if ( error ) return(-1);

  error = FT_New_Face( library, fontfile, 0, &face ); /* create face object */
  if ( error ) return(-1);

  /* use 25 at 72 dpi */
  error = FT_Set_Char_Size( face, 25*64, 0, 72, 0 );  /* set character size */
  if ( error ) return(-1);

  slot = face->glyph;

  /* set up matrix */
  matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L );
  matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
  matrix.yx = (FT_Fixed)( sin( angle ) * 0x10000L );
  matrix.yy = (FT_Fixed)( cos( angle ) * 0x10000L );

  /* the pen position incartesian space coordinates; */
  pen.x = 0  * 64;
  pen.y = 10 * 64;

  memset(&(ST_image[0][0]),0,ST_WIDTH*ST_HEIGHT);
  ST_rightend=0;

  for ( n = 0; n < num_chars; n++ )
  {
    /* set transformation */
    FT_Set_Transform( face, &matrix, &pen );

    /* load glyph image into the slot (erase previous one) */
    error = FT_Load_Char( face, text[n], FT_LOAD_RENDER );
    if ( error ) continue;  /* ignore errors */

    /* now, draw to our target surface (convert position) */
    draw_bitmap( &slot->bitmap,
                 slot->bitmap_left,
                 target_height - slot->bitmap_top );

    if ((slot->bitmap_left + slot->bitmap.width) > ST_WIDTH) 
	    break;

    if ((slot->bitmap_left + slot->bitmap.width) > ST_rightend) 
	    ST_rightend=(slot->bitmap_left + slot->bitmap.width);

    /* increment pen position */
    pen.x += slot->advance.x;
    pen.y += slot->advance.y;
  }

  FT_Done_Face    ( face );
  FT_Done_FreeType( library );

  return 0;
}

#else  /* No freetype */

unsigned char ST_image[1][1];
int ST_rightend = 0;
int render_font (char *fontfile, char *text) {return -1;};

#endif /* HAVE_FT*/

/* vi:set ts=8 sts=2 sw=2: */
