/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2019 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLUSTER_H_INCLUDED
#define CLUSTER_H_INCLUDED

#include <algorithm>
#include <array>
#include <atomic>
#include <istream>
#include <string>

#ifdef USE_MPI
#include <mpi.h>
#endif

#include "tt.h"

class Thread;

/// The Cluster namespace contains functionality required to run on distributed
/// memory architectures using MPI as the message passing interface. On a high level,
/// a 'lazy SMP'-like scheme is implemented where TT saves of sufficient depth are
/// collected on each rank and distributed to, and used by, all other ranks,
/// which search essentially independently.  The root (MPI rank 0) of the cluster
/// is responsible for all I/O and time management, communicating this info to
/// the other ranks as needed. UCI options such as Threads and Hash specify these
/// quantities per MPI rank.  It is recommended to have one rank (MPI process) per node.
/// For the non-MPI case, wrappers that will be compiler-optimized away are provided.

namespace Cluster {

/// Basic info to find the cluster-wide bestMove
struct MoveInfo {
  int move;
  int ponder;
  int depth;
  int score;
  int rank;
};

#ifdef USE_MPI

/// store the TTEntry with its full key, so it can be saved on the receiver side
using KeyedTTEntry = std::pair<Key, TTEntry>;

/// Threads locally cache their high-depth TT entries till a batch can be send by MPI
constexpr std::size_t TTCacheSize = 64;
class ClusterCache
{

public:

   ClusterCache();
   bool replace(const KeyedTTEntry& value);
   void handle_buffer(std::atomic<long unsigned int>& sendRecvPosted);
   void send_recv(std::atomic<uint64_t>& sendRecvPosted);

   // Keep a heap of entries replacing low depth with high depth entries
   std::array<KeyedTTEntry, TTCacheSize> buffer = {};
   // The TTCacheCounter tracks the number of local elements that are ready to be sent.
   uint64_t TTCacheCounter, TTCacheCounterTarget;
   // The receive buffer is used to gather information from all ranks.
   std::array<std::vector<Cluster::KeyedTTEntry>, 2> TTSendRecvBuffs;
   std::array<MPI_Request, 2> reqsTTSendRecv;
};

void init();
void finalize();
bool getline(std::istream& input, std::string& str);
int size();
int rank();
inline bool is_root() { return rank() == 0; }
void save(Thread* thread, TTEntry* tte, Key k, Value v, bool PvHit, Bound b, Depth d, Move m, Value ev);
void pick_moves(MoveInfo& mi, std::string& PVLine);
uint64_t nodes_searched();
uint64_t tb_hits();
uint64_t TT_saves();
void cluster_info(Depth depth);
void signals_init();
void signals_poll();
void mpi_sync();

#else

class ClusterCache { };

inline void init() { }
inline void finalize() { }
inline bool getline(std::istream& input, std::string& str) { return static_cast<bool>(std::getline(input, str)); }
constexpr int size() { return 1; }
constexpr int rank() { return 0; }
constexpr bool is_root() { return true; }
inline void save(Thread*, TTEntry* tte, Key k, Value v, bool PvHit, Bound b, Depth d, Move m, Value ev) { tte->save(k, v, PvHit, b, d, m, ev); }
inline void pick_moves(MoveInfo&, std::string&) { }
uint64_t nodes_searched();
uint64_t tb_hits();
uint64_t TT_saves();
inline void cluster_info(Depth) { }
inline void signals_init() { }
inline void signals_poll() { }
inline void mpi_sync() { }

#endif /* USE_MPI */

}

#endif // #ifndef CLUSTER_H_INCLUDED
