/***************************************************************************
  This file is part of Morris.
  Copyright (C) 2009 Dirk Farin <dirk.farin@gmail.com>

  Morris is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************/

#ifndef GTK_MOVELOG_HH
#define GTK_MOVELOG_HH

#include "movelog.hh"
#include <gtk/gtk.h>


class MoveLog_Gtk : public MoveLog
{
public:
  MoveLog_Gtk();
  ~MoveLog_Gtk();

  void refresh();

private:
  GtkWidget* window;
  GtkWidget* scrolledWindow;
  GtkWidget* treeview;

  GtkListStore* liststore;

  gulong destroyHandler;
  boost::signals2::connection refreshConnection;

  void   cbDestroy();
  friend gboolean cbMoveLog_gtk_destroy(GtkWidget *widget, gpointer data);
};

#endif
