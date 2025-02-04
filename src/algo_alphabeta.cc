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

#include "algo_alphabeta.hh"
#include "ttable.hh"
#include "threadtunnel.hh"
#include "util.hh"
#include "mainapp.hh"

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <math.h>

#define RANDOMIZE true
#define LOGSEARCH false
#define ALGOTRACE 0

const PlayerIF_AlgoAB::eval_t VALUE_INFINITE = 10000;
const PlayerIF_AlgoAB::eval_t EVAL_WIN = 9000;

inline void addPly(PlayerIF_AlgoAB::eval_t &e)
{
    /**/ if (e > EVAL_WIN)
        e--;
    else if (e < -EVAL_WIN)
        e++;
}

inline void subPly(PlayerIF_AlgoAB::eval_t &e)
{
    /**/ if (e > EVAL_WIN)
        e++;
    else if (e < -EVAL_WIN)
        e--;
}

int nPlysUntilEnd(PlayerIF_AlgoAB::eval_t e)
{
    e = fabs(e);
    return VALUE_INFINITE - e;
}

static int timeDiff_ms(const struct timeval &t1,
                       const struct timeval &t2)
{
    const int diffSec = t2.tv_sec - t1.tv_sec;
    const int diffUS = t2.tv_usec - t1.tv_usec;

    const int diffMS = diffSec * 1000 + diffUS / 1000;

    return diffMS;
}

PlayerIF_AlgoAB::PlayerIF_AlgoAB()
    : m_tunnel(NULL),
    thread(NULL)
{
    moveCnt = 0;

    m_maxMSecs = 1000;
    m_maxDepth = 25;

    m_weight[Weight_Material] = 1.0;
    m_weight[Weight_Freedom] = 0.2;
    m_weight[Weight_Mills] = 0.8;
    m_weight[Weight_Experience] = 1.0;
}

void PlayerIF_AlgoAB::resetGame()
{
    /* We have to clear the t-table to prevent that
        the computer always plays the same game. */

    m_ttable->clear();
}

void startSearchThread(class PlayerIF_AlgoAB *);

void PlayerIF_AlgoAB::startMove(const Position &curr, int moveID)
{
    /*
      std::cout << "-------------------- ";
      if (curr.getCurrentPlayer()==PL_White) std::cout << "white";
      else std::cout << "black";
      std::cout << " (ply=" << curr.getPly() << ")\n";
      */

    moveCnt++;

    m_stopThread = false;
    m_ignoreMove = false;
    m_computedSomeMove = false;

    rootPos = curr;
    m_moveID = moveID;

    thread = g_thread_new(NULL, (GThreadFunc)startSearchThread, this);
}

// kicker
void startSearchThread(class PlayerIF_AlgoAB *algo)
{
    algo->doSearch();
}

void PlayerIF_AlgoAB::doSearch()
{
    gettimeofday(&m_startTime, NULL);

    m_move.reset();
    m_ttable->resetStats();

    float e;

    for (int depth = 1; depth <= m_maxDepth; depth++) {
        m_nodesEvaluated = 0;

        Variation var;
        e = search(rootPos, -VALUE_INFINITE, VALUE_INFINITE, 0, depth, var, true);

        // normalize evaluation for white
        if (rootPos.getCurrentPlayer() == PL_Black) {
            e = -e;
        }

        if (LOGSEARCH)
            std::cout << "STEP move " << m_move << " depth " << depth << " -> eval=" << e
            << " nodes evaluated= " << m_nodesEvaluated
            << "\n";

        if (fabs(e) >= EVAL_WIN) {
            int overInPlys = nPlysUntilEnd(e);

            if (overInPlys <= depth + 1) {
                // we won't find a better winning line
                break;
            }
        }
    }

    m_tunnel->doMove(m_move, m_moveID);
    installJoinThreadHandler();
}

void PlayerIF_AlgoAB::installJoinThreadHandler()
{
    class IdleFunc_JoinAlgoThread : public IdleFunc
    {
    public:
        IdleFunc_JoinAlgoThread(PlayerIF_AlgoAB *algo) : obj(algo)
        {
        }

        void operator()()
        {
            obj->joinThread();
        }

    private:
        PlayerIF_AlgoAB *obj;
    };

    IdleFunc::install(new IdleFunc_JoinAlgoThread(this));
}

void PlayerIF_AlgoAB::forceMove()
{
    m_stopThread = true;
}

void PlayerIF_AlgoAB::cancelMove()
{
    m_ignoreMove = true;
    m_stopThread = true;

    joinThread();
}

void PlayerIF_AlgoAB::joinThread()
{
    if (thread) {
        g_thread_join(thread);
        thread = NULL;
    }
}

inline void PlayerIF_AlgoAB::checkTime()
{
    struct timeval endTime;
    gettimeofday(&endTime, NULL);

    int timeDiffMS = timeDiff_ms(m_startTime, endTime);

    // thinking time is over
    if (timeDiffMS >= m_maxMSecs) {
        m_stopThread = true;
    }

    // update progress bar
    float perc = timeDiffMS;
    perc /= m_maxMSecs;
    if (perc > 1.0)
        perc = 1.0;

    m_tunnel->setProgress(perc);
}

// ----------------------------------------------------------------------------------------------------

#define INDENT std::cout << "-" << (&"| | | | | | | | | | "[20 - currDepth * 2]);

float PlayerIF_AlgoAB::search(const Position &pos, float alpha, float beta,
                               int originDepth, int depth, Variation &variation, bool useTT)
{
    if (ALGOTRACE) {
        INDENT;
        std::cout << "--- NEGAMAX (" << alpha << ";" << beta << ") ---\n";
    }

    const bool atRoot = (originDepth == 0);

    // check thinking time and stop if we were thinking too long

    if (depth > 5 || atRoot) {
        if (useTT)
            checkTime();
    }

    if (m_stopThread && m_computedSomeMove) {
        if (!m_ignoreMove) {
            m_tunnel->doMove(m_move, m_moveID);
        }
        installJoinThreadHandler();
        g_thread_exit(NULL);
    }

    // check winning situations

    if (pos.getNPiecesLeft(pos.getCurrentPlayer()) < 3) {
        return -VALUE_INFINITE;
    }

    // check transposition-table

    const float oldAlpha = alpha;

    const TranspositionTable::TTEntry *entry = NULL;
    if (useTT)
        entry = m_ttable->search(pos.key(), pos);
    if (entry) {
        if (entry->depth8 >= depth) {
            if (ALGOTRACE) {
                INDENT;
                std::cout << "found table entry !\n";
            }

            if (entry->getBoundType() == TranspositionTable::BOUND_EXACT) {
                if (atRoot) {
                    m_move = entry->ttMove;
                    m_computedSomeMove = true;
                    logBestMoveFromTable(pos, m_move, entry->value8, entry->depth8);
                }

                if (ALGOTRACE) {
                    INDENT;
                    std::cout << "entry = " << *entry << "\n";
                }
                return entry->value8;
            } else if (entry->getBoundType() == TranspositionTable::BOUND_LOWER) {
                alpha = std::max(alpha, entry->value8);
            } else if (entry->getBoundType() == TranspositionTable::BOUND_UPPER) {
                beta = std::min(beta, entry->value8);
            }

            if (alpha >= beta) {
                if (atRoot) {
                    m_move = entry->ttMove;
                    m_computedSomeMove = true;
                }

                if (ALGOTRACE) {
                    std::cout << "  table-> " << entry->value8 << "\n";
                }

                return entry->value8;
            }
        }
    }

    // process leaves

    if (depth == 0) {
        float eval = Eval(pos, depth);

        /*
              float memeval = -m_posMemory.lookupHash(pos);
              if (memeval!=0)
            {
              subPly(memeval);
              return memeval;
            }
              */

        if (ALGOTRACE) {
            INDENT;
            std::cout << "leaf. Eval=" << eval << "\n";
        }
        return eval;
    }

    // try previous best-move first

    Position tmpBoard;
    float bestEval = -VALUE_INFINITE;
    Move bestMove;

    tmpBoard = pos;

    // recurse

    std::vector<Move> moves;
    moves.reserve(100); // speed-up move generation
    m_ruleSpec->generateMoves(moves, pos);

    if (moves.size() == 0) {
        return -VALUE_INFINITE;
    }

    // Move ordering: put most promising move to front

    if (entry) {
        for (int i = 1; i < moves.size(); i++)
            if (moves[i] == entry->ttMove) {
                std::swap(moves[0], moves[i]);
                break;
            }
    }

    // random move order to randomize play
    if (RANDOMIZE && atRoot) {
        for (int i = 1; i < moves.size(); i++) {
            int idx2 = (rand() % (moves.size() - i)) + i;

            std::swap(moves[i], moves[idx2]);
        }
    }

    if (ALGOTRACE) {
        INDENT;
        std::cout << "list of moves: ";
        for (size_t i = 0; i < moves.size(); i++) {
            std::cout << moves[i] << " ";
        }
        std::cout << "\n";
    }

    for (size_t i = 0; i < moves.size(); i++) {
        if (ALGOTRACE) {
            INDENT;
            std::cout << "try move: " << moves[i] << "  (" << alpha << "," << beta << ")\n";
        }

        tmpBoard.doMove(moves[i]);

        Variation childVar;
        eval_t recBeta = beta;
        subPly(recBeta);
        eval_t recAlpha = alpha;
        subPly(recAlpha);
        float eval = -search(tmpBoard, -recBeta, -recAlpha, originDepth + 1, depth - 1, childVar, useTT);
        addPly(eval);

        if (originDepth == 0 && m_experience != NULL) {
            if (fabs(eval) < EVAL_WIN) {
                float offset = m_experience->getOffset(m_ruleSpec->getBoardID_Symmetric(tmpBoard), m_selfPlayer);
                offset *= m_weight[Weight_Experience];
                eval += offset;
            }
        }

        /*
              if (abs(eval)>EVAL_WIN)
            m_posMemory.storeBoard(tmpBoard,eval);
              */

        tmpBoard.undoMove(moves[i]);

        if (eval > bestEval) {
            bestEval = eval;
            bestMove = moves[i];

            variation.clear();
            variation.push_back(moves[i]);
            variation.append(childVar);

            if (atRoot) {
                logBestMove(variation, bestEval, depth);
                m_move = bestMove;
                m_computedSomeMove = true;
            }

            if (ALGOTRACE) {
                INDENT;
                std::cout << "set best move to " << moves[i] << " @eval=" << eval << "\n";
            }

            // alpha-beta pruning
            if (bestEval >= beta) {
                if (ALGOTRACE) {
                    INDENT;
                    std::cout << "beta cut-off\n";
                }
                break;
            }
        }

        // MAX
        if (bestEval > alpha) {
            alpha = bestEval;
            if (ALGOTRACE) {
                INDENT;
                std::cout << "adjust alpha to " << alpha << "\n";
            }
        }
    }

    // save into transposition-table
    m_ttable->save(pos.key(), bestEval, TranspositionTable::boundType(bestEval, oldAlpha, beta),
                     depth, bestMove, pos);

    if (ALGOTRACE) {
        INDENT;
        std::cout << "bestEval= " << bestEval << "\n";
        INDENT;
        std::cout << "bestMove= " << bestMove << "\n";
    }

    return bestEval;
}

float PlayerIF_AlgoAB::Eval(const Position &pos, int levelsToGo) const
{
    m_nodesEvaluated++;

    float eval = 0.0;

    const Player me = pos.getCurrentPlayer();
    const Player other = opponent(me);

    const RuleSpec &ruleSpec = *m_ruleSpec;
    const BoardSpec &boardSpec = *ruleSpec.boardSpec;

    // ========== material ==========

    eval += m_weight[Weight_Material] * (pos.getNPiecesLeft(pos.getCurrentPlayer()) -
                                         pos.getNPiecesLeft(pos.getOpponentPlayer()));

    // ========== freedom ==========

    int myFreedom = 0;
    int oppFreedom = 0;
    for (int i = 0; i < boardSpec.nPositions(); i++) {
        /**/ if (pos.getPosition(i) == me)
            myFreedom += ruleSpec.freedomAtPosition(pos, i);
        else if (pos.getPosition(i) == other)
            oppFreedom += ruleSpec.freedomAtPosition(pos, i);
    }

    // Note: consider special case at start of game: no pieces on the pos -> no freedom.

    if (myFreedom == 0 &&
        pos.getNPiecesOnBoard(pos.getCurrentPlayer()) > 0) {
        if (ruleSpec.mayJump && pos.getNPiecesLeft(pos.getCurrentPlayer()) == 3) {
        } else if (pos.getNPiecesToSet(pos.getCurrentPlayer()) > 0) {
        } else {
            return -VALUE_INFINITE;
        }
    }

    eval += m_weight[Weight_Freedom] * (myFreedom - oppFreedom);

    // ========== mills ==========

    int myMills = 0;
    int otherMills = 0;
    for (int i = 0; i < boardSpec.nMills(); i++) {
        const MillPosVector &mill = boardSpec.getMill(i);

        Player p0 = pos.getPosition(mill[0]);
        if (p0 != PL_None &&
            p0 == pos.getPosition(mill[1]) &&
            p0 == pos.getPosition(mill[2])) {
            if (p0 == me)
                myMills++;
            else
                otherMills++;
        }
    }

    eval += m_weight[Weight_Mills] * (myMills - otherMills);

#if 0
    // dummy weights for debugging ...

    for (int i = 0; i < m_control->getBoardSpec().nPositions(); i++) {
        /**/ if (pos.getPosition(i) == me)    eval += 24 - i;
        else if (pos.getPosition(i) == other) eval -= 24 - i;
    }
#endif

    return eval;
}

void PlayerIF_AlgoAB::logBestMoveFromTable(const Position &b, const Move &m, eval_t e, int depth) const
{
    Position pos = b;
    Move move = m;

    Variation v;

    for (int i = 0; i <= depth; i++) {
        v.push_back(move);

        pos.doMove(move);

        const TranspositionTable::TTEntry *entry;
        entry = m_ttable->search(pos.key(), pos);
        if (entry) {
            move = entry->ttMove;
        } else
            break;
    }

    logBestMove(v, e, depth, " <- from ttable");
}

void PlayerIF_AlgoAB::logBestMove(const Move &m, eval_t e, int depth) const
{
    Variation v;
    v.push_back(m);
    logBestMove(v, e, depth);
}

void PlayerIF_AlgoAB::logBestMove(const Variation &v, eval_t e, int depth, const char *suffix) const
{
    std::stringstream strstr;

    Player p = rootPos.getCurrentPlayer();
    for (int i = 0; i < v.size(); i++) {
        strstr << writeMove(v[i], m_ruleSpec->boardSpec) << " ";
        if (p == PL_Black && i < v.size() - 1)
            strstr << "/ ";
        p = opponent(p);
    }

    strstr << "(";
    if (fabs(e) > EVAL_WIN) {
        strstr << e << " ";

        Player winner;
        if (rootPos.getCurrentPlayer() == PL_White)
            winner = PL_White;
        else
            winner = PL_Black;
        if (e < 0)
            winner = opponent(winner);

        char *player;
        if (winner == PL_White)
            player = _("white");
        else
            player = _("black");

        int eInt = e;
        int eAbs = (eInt < 0 ? -eInt : eInt);
        int iInt = VALUE_INFINITE;

        int winInMoves = (iInt - 1 - eAbs) / 2 + 1;

        char buf[100];
        if (winInMoves > 1)
            sprintf(buf, _("%s wins in %d moves)"), player, winInMoves);
        else
            sprintf(buf, _("%s wins the next move)"), player);

        strstr << buf;
    } else {
        if (rootPos.getCurrentPlayer() == PL_Black)
            e = -e;
        strstr << e << ')';
    }

    strstr << " [" << depth << "]" << suffix;

    m_tunnel->showThinkingInfo(strstr.str());
}

void PlayerIF_AlgoAB::notifyWinner(Player p)
{
    if (p == PL_None) {
        return;
    }

    if (m_experience == NULL) {
        return;
    }

    const GameControl &control = MainApp::app().getControl();

    for (int i = 0; i < control.getHistorySize() - 1; i++) {
        boost::shared_ptr<Position> pos = control.getHistoryBoard(i);

        m_experience->addBoard(m_ruleSpec->getBoardID_Symmetric(*pos), p);
    }
}
