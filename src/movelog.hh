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

#ifndef MOVELOG_HH
#define MOVELOG_HH

#include <boost/signals2.hpp>

/* A window showing a log of all moves in the current game.
 */
class MoveLog
{
public:
    virtual ~MoveLog()
    {
    }

    virtual void refresh() = 0;

    boost::signals2::signal<void()> &getSignal_windowClosed()
    {
        return m_signal_windowClosed;
    }

protected:
    boost::signals2::signal<void()> m_signal_windowClosed;
};

#endif
