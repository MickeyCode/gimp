/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcircle.c
 * Copyright (C) 2014 Michael Natterer <mitch@gimp.org>
 *
 * Based on code from the color-rotate plug-in
 * Copyright (C) 1997-1999 Sven Anders (anderss@fmi.uni-passau.de)
 *                         Based on code from Pavel Grinfeld (pavel@ml.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "gimpcircle.h"


enum
{
  PROP_0,
  PROP_SIZE,
  PROP_BORDER_WIDTH,
  PROP_BACKGROUND
};


struct _GimpCirclePrivate
{
  gint                  size;
  gint                  border_width;
  GimpCircleBackground  background;

  GdkWindow            *event_window;
};


static void        gimp_circle_dispose         (GObject              *object);
static void        gimp_circle_set_property    (GObject              *object,
                                                guint                 property_id,
                                                const GValue         *value,
                                                GParamSpec           *pspec);
static void        gimp_circle_get_property    (GObject              *object,
                                                guint                 property_id,
                                                GValue               *value,
                                                GParamSpec           *pspec);

static void        gimp_circle_realize         (GtkWidget            *widget);
static void        gimp_circle_unrealize       (GtkWidget            *widget);
static void        gimp_circle_map             (GtkWidget            *widget);
static void        gimp_circle_unmap           (GtkWidget            *widget);
static void        gimp_circle_size_request    (GtkWidget            *widget,
                                                GtkRequisition       *requisition);
static void        gimp_circle_size_allocate   (GtkWidget            *widget,
                                                GtkAllocation        *allocation);
static gboolean    gimp_circle_expose_event    (GtkWidget            *widget,
                                                GdkEventExpose       *event);

static void        gimp_circle_background_hsv  (gdouble               angle,
                                                gdouble               distance,
                                                guchar               *rgb);

static void        gimp_circle_draw_background (cairo_t              *cr,
                                                gint                  size,
                                                GimpCircleBackground  background);


G_DEFINE_TYPE (GimpCircle, gimp_circle, GTK_TYPE_WIDGET)

#define parent_class gimp_circle_parent_class


static void
gimp_circle_class_init (GimpCircleClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose              = gimp_circle_dispose;
  object_class->get_property         = gimp_circle_get_property;
  object_class->set_property         = gimp_circle_set_property;

  widget_class->realize              = gimp_circle_realize;
  widget_class->unrealize            = gimp_circle_unrealize;
  widget_class->map                  = gimp_circle_map;
  widget_class->unmap                = gimp_circle_unmap;
  widget_class->size_request         = gimp_circle_size_request;
  widget_class->size_allocate        = gimp_circle_size_allocate;
  widget_class->expose_event         = gimp_circle_expose_event;

  g_object_class_install_property (object_class, PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     NULL, NULL,
                                                     32, 1024, 96,
                                                     GIMP_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_BORDER_WIDTH,
                                   g_param_spec_int ("border-width",
                                                     NULL, NULL,
                                                     0, 64, 0,
                                                     GIMP_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_BACKGROUND,
                                   g_param_spec_enum ("background",
                                                      NULL, NULL,
                                                      GIMP_TYPE_CIRCLE_BACKGROUND,
                                                      GIMP_CIRCLE_BACKGROUND_HSV,
                                                      GIMP_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

  g_type_class_add_private (klass, sizeof (GimpCirclePrivate));
}

static void
gimp_circle_init (GimpCircle *circle)
{
  circle->priv = G_TYPE_INSTANCE_GET_PRIVATE (circle,
                                              GIMP_TYPE_CIRCLE,
                                              GimpCirclePrivate);

  gtk_widget_set_has_window (GTK_WIDGET (circle), FALSE);
}

static void
gimp_circle_dispose (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_circle_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GimpCircle *circle = GIMP_CIRCLE (object);

  switch (property_id)
    {
    case PROP_SIZE:
      circle->priv->size = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (circle));
      break;

    case PROP_BORDER_WIDTH:
      circle->priv->border_width = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (circle));
      break;

    case PROP_BACKGROUND:
      circle->priv->background = g_value_get_enum (value);
      gtk_widget_queue_draw (GTK_WIDGET (circle));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_circle_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GimpCircle *circle = GIMP_CIRCLE (object);

  switch (property_id)
    {
    case PROP_SIZE:
      g_value_set_int (value, circle->priv->size);
      break;

    case PROP_BORDER_WIDTH:
      g_value_set_int (value, circle->priv->border_width);
      break;

    case PROP_BACKGROUND:
      g_value_set_enum (value, circle->priv->background);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_circle_realize (GtkWidget *widget)
{
  GimpCircle    *circle = GIMP_CIRCLE (widget);
  GtkAllocation  allocation;
  GdkWindowAttr  attributes;
  gint           attributes_mask;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x           = allocation.x;
  attributes.y           = allocation.y;
  attributes.width       = allocation.width;
  attributes.height      = allocation.height;
  attributes.wclass      = GDK_INPUT_ONLY;
  attributes.event_mask  = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  circle->priv->event_window = gdk_window_new (gtk_widget_get_window (widget),
                                               &attributes, attributes_mask);
  gdk_window_set_user_data (circle->priv->event_window, circle);
}

static void
gimp_circle_unrealize (GtkWidget *widget)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  if (circle->priv->event_window)
    {
      gdk_window_set_user_data (circle->priv->event_window, NULL);
      gdk_window_destroy (circle->priv->event_window);
      circle->priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
gimp_circle_map (GtkWidget *widget)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (circle->priv->event_window)
    gdk_window_show (circle->priv->event_window);
}

static void
gimp_circle_unmap (GtkWidget *widget)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  if (circle->priv->event_window)
    gdk_window_hide (circle->priv->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gimp_circle_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  requisition->width  = 2 * circle->priv->border_width + circle->priv->size;
  requisition->height = 2 * circle->priv->border_width + circle->priv->size;
}

static void
gimp_circle_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (circle->priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);
}

static gboolean
gimp_circle_expose_event (GtkWidget      *widget,
                          GdkEventExpose *event)
{
  GimpCircle *circle = GIMP_CIRCLE (widget);

  if (gtk_widget_is_drawable (widget))
    {
      GtkAllocation  allocation;
      gint           size = circle->priv->size;
      cairo_t       *cr;

      cr = gdk_cairo_create (event->window);
      gdk_cairo_region (cr, event->region);
      cairo_clip (cr);

      gtk_widget_get_allocation (widget, &allocation);

      cairo_translate (cr,
                       allocation.x + (allocation.width  - size) / 2,
                       allocation.y + (allocation.height - size) / 2);

      gimp_circle_draw_background (cr, size, circle->priv->background);

      cairo_destroy (cr);
    }

  return FALSE;
}


/*  public functions  */

GtkWidget *
gimp_circle_new (void)
{
  return g_object_new (GIMP_TYPE_CIRCLE, NULL);
}


/*  private functions  */

static void
gimp_circle_background_hsv (gdouble  angle,
                            gdouble  distance,
                            guchar  *rgb)
{
  gdouble v = 1 - sqrt (distance) / 4; /* it just looks nicer this way */

  gimp_hsv_to_rgb4 (rgb, angle / (2.0 * G_PI), distance, v);
}

static gdouble
get_angle_and_distance (gdouble  center_x,
                        gdouble  center_y,
                        gdouble  radius,
                        gdouble  x,
                        gdouble  y,
                        gdouble *distance)
{
  gdouble angle = atan2 (center_y - y,
                         x - center_x);

  if (angle < 0)
    angle += 2 * G_PI;

  if (distance)
    *distance = sqrt ((SQR (x - center_x) +
                       SQR (y - center_y)) / SQR (radius));

  return angle;
}

static void
gimp_circle_draw_background (cairo_t              *cr,
                             gint                  size,
                             GimpCircleBackground  background)
{
  cairo_save (cr);

  if (background == GIMP_CIRCLE_BACKGROUND_PLAIN)
    {
      cairo_arc (cr, size / 2.0, size / 2.0, size / 2.0 - 1.5, 0.0, 2 * G_PI);

      cairo_set_line_width (cr, 3.0);
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6);
      cairo_stroke_preserve (cr);

      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8);
      cairo_stroke (cr);
    }
  else
    {
      cairo_surface_t *surface;
      guchar          *data;
      gint             stride;
      gint             x, y;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);

      data   = cairo_image_surface_get_data (surface);
      stride = cairo_image_surface_get_stride (surface);

      for (y = 0; y < size; y++)
        {
          for (x = 0; x < size; x++)
            {
              gdouble angle;
              gdouble distance;
              guchar  rgb[3] = { 0, };

              angle = get_angle_and_distance (size / 2.0, size / 2.0, size / 2.0,
                                              x, y,
                                              &distance);

              switch (background)
                {
                case GIMP_CIRCLE_BACKGROUND_HSV:
                  gimp_circle_background_hsv (angle, distance, rgb);
                  break;

                default:
                  break;
                }

              GIMP_CAIRO_ARGB32_SET_PIXEL (data + y * stride + x * 4,
                                           rgb[0], rgb[1], rgb[2], 255);
            }
        }

      cairo_surface_mark_dirty (surface);
      cairo_set_source_surface (cr, surface, 0.0, 0.0);
      cairo_surface_destroy (surface);

      cairo_arc (cr, size / 2.0, size / 2.0, size / 2.0, 0.0, 2 * G_PI);
      cairo_clip (cr);

      cairo_paint (cr);
    }

  cairo_restore (cr);
}