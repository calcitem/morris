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

#ifndef TTABLE_HH
#define TTABLE_HH

#include "board.hh"

#define SAFE_HASH 0

class TranspositionTable
{
public:
    TranspositionTable(int nBits);
    ~TranspositionTable();

    void clear();

    enum Bound
    {
        BOUND_LOWER,
        BOUND_EXACT,
        BOUND_UPPER
    };

    struct TTEntry
    {
        Key key;
        float value8;
        signed char depth8; // depth to which this node was calculated
        signed char genBound8;
        Move ttMove;

#if SAFE_HASH
        Board board; // TMP
#endif

        Bound getBoundType() const
        {
            return (Bound)(genBound8);
        }
    };

    const TTEntry *lookup(Key h, const Board &) const;
    void insert(Key h, float eval, Bound, int depth, const Move &bestMove, const Board &);

    static inline Bound boundType(float eval, float alpha, float beta)
    {
        if (eval <= alpha)
            return BOUND_UPPER;
        if (eval >= beta)
            return BOUND_LOWER;

        return BOUND_EXACT;
    }

    void resetStats()
    {
        lookups = hits = collisions = misses = 0;
    }

    int nHits() const
    {
        return hits;
    }

    int nCollisions() const
    {
        return collisions;
    }

    int nLookups() const
    {
        return lookups;
    }

    float getFillStatus() const;

private:
    TTEntry *table;
    int tableSize;
    Key mask;

    mutable int lookups, hits, collisions, misses;
};

typedef boost::shared_ptr<TranspositionTable> ttable_ptr;

#include <iostream>
#include <iomanip>

inline std::ostream &operator<<(std::ostream &ostr, const TranspositionTable::Bound &t)
{
    switch (t) {
    case TranspositionTable::BOUND_LOWER:
        ostr << "lower";
        break;
    case TranspositionTable::BOUND_UPPER:
        ostr << "upper";
        break;
    case TranspositionTable::BOUND_EXACT:
        ostr << "accurate";
        break;
    }

    return ostr;
}

inline std::ostream &operator<<(std::ostream &ostr, const TranspositionTable::TTEntry &e)
{
    ostr << "hash=" << std::hex << e.key
        << " eval=" << e.value8
        << " depth=" << ((int)e.depth8)
        << " bound=" << e.getBoundType()
        << " ttMove=" << e.ttMove;
    return ostr;
}

#endif
