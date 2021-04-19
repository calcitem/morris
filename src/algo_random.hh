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

#ifndef ALGO_RANDOM_HH
#define ALGO_RANDOM_HH

#include "control.hh"
#include <stdlib.h>

/* A very dumb AI that just makes a random move.
   Was mainly used to test the move generation.
   Is not employed in the current program.
 */
class PlayerIF_AlgoRandom : public PlayerIF
{
public:
    PlayerIF_AlgoRandom()
    {
    }

    void registerThreadTunnel(ThreadTunnel &tunnel)
    {
        m_tunnel = &tunnel;
    }

    bool isInteractivePlayer() const
    {
        return false;
    }

    void startMove(const Board &current, int moveID);

    void forceMove()
    {
    }
    void cancelMove()
    {
    }

private:
    class ThreadTunnel *m_tunnel;
};

#endif
