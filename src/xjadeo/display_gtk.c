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
 */

#include "xjadeo.h"
#include "display.h"


/*******************************************************************************
 * GTK
 */

#if HAVE_MYGTK

  GtkWidget *gwindow,*gimage;
  GdkGeometry *ggeometry;

/* gtk callback function */
void on_mygtk_destroy (GtkObject *object, gpointer user_data) {
	if (!remote_en)
		loop_flag = 0;
	gwindow=NULL;
}

void on_mygtk_expose (GtkObject *object, gpointer user_data) {
	if(buffer) render_gtk (buffer);
}

void on_mygtk_key( GtkWidget      *widget, GdkEventKey *event ){
	if(event->keyval == GDK_Escape) loop_flag=0;
//	return TRUE;
}


void on_mygtk_clicked( GtkWidget      *widget, GdkEventButton *event )
{
	if (event->button == 1 ) {
		gdk_window_resize(gwindow->window,movie_width,movie_height);
	}  else { 
		unsigned int my_Width,my_Height;
		getsize_gtk(&my_Width,&my_Height);

		if(event->button == 4) {
			float step=sqrt((float)my_Height);
			my_Width-=floor(step*((float)movie_width/(float)movie_height));
			my_Height-=step;
		}
		if(event->button == 5) {
			float step=sqrt((float)my_Height);
			my_Width+=floor(step*((float)movie_width/(float)movie_height));
			my_Height+=step;
		} 
		// resize to match movie aspect ratio
		if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
			my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
		else my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);

		resize_gtk(my_Width,my_Height);
	}

#if 0 // To be continued...	
	else if (event->button == 4 ) {
		gdk_window_iconify (GTK_WINDOW(gwindow));
		gtk_window_set_default_size(GTK_WINDOW(gwindow),100,100);
	}
	else printf("other: %i\n",event->button);
#endif

//	return TRUE;
}

void resize_gtk (unsigned int x, unsigned int y) { 
	gdk_window_resize(gwindow->window,x,y);
}

void getsize_gtk (unsigned int *x, unsigned int *y) {
#if 0 // what was that ??
	int sx,sy;
	sx=(int) *x;
	sy=(int) *y;
	gdk_window_resize(gwindow->window,sx,sy);
#endif
//	gint d0,d1,d2;
	gint d3,d4;
	gdk_window_get_size(gwindow->window,&d3,&d4);
//	gdk_window_get_geometry (gwindow->window, &d0,&d1,&d3,&d4,&d2);
	if(x) *x=(unsigned int) d3;
	if(y) *y=(unsigned int) d4;
}

void position_gtk (int x, int y) { 
//  gdk_window_get_origin()
//  gdk_window_get_position(gwindow,&x,&y)
//  gdk_window_set_geometry_hints (GdkWindow *window, GdkGeometry *geometry, GdkWindowHints geom_mask);
	;
}


int open_window_gtk(int *argc, char ***argv) {
	gtk_init(argc, argv);
	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	gwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gimage = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(gwindow),gimage);
//	gtk_drawing_area_size(GTK_DRAWING_AREA(gimage), movie_width, movie_height);
//	gtk_widget_set_usize(GTK_WIDGET(gimage), movie_width, movie_height);
//	gtk_window_set_default_size(GTK_WINDOW(gwindow),movie_width,movie_height);
	gtk_signal_connect ((gpointer) gwindow, "destroy", on_mygtk_destroy, NULL);
//	gtk_signal_connect ((gpointer) gwindow, "destroy", gtk_widget_destroy, NULL);

//
	gtk_widget_add_events (gimage,
			GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK | 
			GDK_BUTTON_PRESS_MASK| GDK_BUTTON_RELEASE_MASK);

	gtk_widget_add_events (gwindow, GDK_KEY_PRESS_MASK);

	gtk_signal_connect ((gpointer) gimage, "expose_event", on_mygtk_expose, NULL);
 	gtk_signal_connect ((gpointer) gimage, "button_release_event", on_mygtk_clicked, NULL);
 	gtk_signal_connect ((gpointer) gwindow, "key_press_event", on_mygtk_key, NULL);


	gtk_widget_show(gimage);
	gtk_widget_show(gwindow);
	gdk_flush();
	gdk_window_resize(gwindow->window,movie_width,movie_height);
	while(gtk_events_pending()) gtk_main_iteration();
	gdk_flush();
		
	return 0;
}

void close_window_gtk(void) {
	if (gwindow) {
		// needed if closing the window via remote.
		gtk_widget_hide(gwindow);
		gtk_widget_destroy(gwindow);
	}
	gdk_flush();
	while(gtk_events_pending()) gtk_main_iteration();
	gdk_flush();
	gtk_main_quit();
}

void render_gtk (uint8_t *mybuffer) {
	int width=movie_width;
	int height=movie_height;
	unsigned int dest_width,dest_height; //get gtk window size

	getsize_gtk(&dest_width,&dest_height);

	if (dest_width<8 ) dest_width=8;
	if (dest_height<8 ) dest_height=8;

	if (dest_width==width && dest_height == height) { // no scaling 
		gdk_draw_rgb_image(gimage->window, gimage->style->fg_gc[gimage->state],
				0, 0, width, height, GDK_RGB_DITHER_MAX, (guchar*) mybuffer, 
				3*width);
	} else {
		GdkPixbuf *pixbuf, *scaled;

		pixbuf=  gdk_pixbuf_new_from_data  (mybuffer, GDK_COLORSPACE_RGB, 0, 8, width, height, width*3, NULL, NULL);
		scaled = gdk_pixbuf_scale_simple (pixbuf, dest_width, dest_height, GDK_INTERP_NEAREST);

#if 1 // the old way  gtk-1.2
		gdk_pixbuf_render_to_drawable (scaled, gimage->window, gimage->style->fg_gc[gimage->state],
				0,0,0,0,dest_width, dest_height, GDK_RGB_DITHER_NORMAL,0,0);
#else // the new way gtk-2.0
		gdk_draw_pixbuf (gimage->window, gimage->style->fg_gc[gimage->state], scaled,
				0,0,0,0,dest_width, dest_height, GDK_RGB_DITHER_NORMAL,0,0);
#endif

		gdk_pixbuf_unref (scaled);
		gdk_pixbuf_unref (pixbuf);
	}

	gdk_flush();
	while(gtk_events_pending()) {
		gtk_main_iteration();
	} 
	gdk_flush();
}

void handle_X_events_gtk (void) {
	while(gtk_events_pending()) {
		gtk_main_iteration();
	} 
	gdk_flush();
}

#endif /* HAVE_MYGTK */

