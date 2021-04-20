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

#include "ttable.hh"

#include <iostream>

// 16 -   65536
// 17 -  131072
// 18 -  262144
// 19 -  524288
// 20 - 1048576
TranspositionTable::TranspositionTable(int nBits)
{
    tableSize = 1 << nBits;
    TT = new TTEntry[tableSize];

    mask = 0;
    for (int i = 0; i < nBits; i++) {
        mask <<= 1;
        mask |= 1;
    }

    hits = collisions = misses = 0;

    clear();
}

void TranspositionTable::clear()
{
    for (int i = 0; i < tableSize; i++) {
        TT[i].key = 0;
    }
}

TranspositionTable::~TranspositionTable()
{
    delete[] TT;
}

const TranspositionTable::TTEntry *TranspositionTable::search(Key key, const Board &b) const
{
    Key hashValue = key & mask;

    lookups++;
    if (TT[hashValue].key != 0) {
        hits++;
    }
    if (TT[hashValue].key != key) {
        misses++;
    }

#if SAFE_HASH
    if (TT[hashValue].key == key && !(b == TT[hashValue].board)) {
        std::cout << "HASH COLLISION\n";
        assert(0);
    }
#endif

    if (TT[hashValue].key == key) {
        return &TT[hashValue];
    } else {
        return NULL;
    }
}

void TranspositionTable::save(Key key, float value, Bound type, int depth, const Move &ttMove,
                                const Board &b)
{
    Key hashValue = key & mask;

    bool replaceEntry = false;
    /**/ if (TT[hashValue].key == 0) {
        replaceEntry = true;
    } else if (TT[hashValue].key != key) // collision
    {
        replaceEntry = true; /* Always replace such that irrelevant moves do not block the TT. */
        collisions++;
    } else {
        if (depth > TT[hashValue].depth8) {
            replaceEntry = true;
        } else {
            /* NOP */
        }
    }

    if (replaceEntry) {
        TT[hashValue].key = key;
        TT[hashValue].value8 = value;
        TT[hashValue].depth8 = depth;
        TT[hashValue].genBound8 = type;
        TT[hashValue].ttMove = ttMove;

#if SAFE_HASH
        TT[hashValue].board = b;
#endif
    }
}

float TranspositionTable::getFillStatus() const
{
    int nFilled = 0;
    for (int i = 0; i < tableSize; i++) {
        if (TT[i].key) {
            nFilled++;
        }
    }

    return float(nFilled) / tableSize;
}
