/*
 * Copyright (c) 2008, Marcello Caleffi, <marcello.caleffi@unina.it>,
 * http://wpage.unina.it/marcello.caleffi
 *
 * The AOMDV_QPV code has been developed at DIET, Department of Electronic
 * and Telecommunication Engineering, University of Naples "Federico II"
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.

 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
                                * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
            * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

/*
Copyright (c) 1997, 1998 Carnegie Mellon University.  All Rights
Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The AODV code developed by the CMU/MONARCH group was optimized and tuned by Samir Das and Mahesh Marina, University of Cincinnati. The work was partially done in Sun Microsystems.
*/

#include <assert.h>
#include <mac/mac-802_11EvalVSN.h>
#include <aomdv_qpv/aomdv_qpv.h>

#define CURRENT_TIME Scheduler::instance().clock()
#define QDEBUG

/*
  Packet Queue used by AOMDV_QPV.
*/

// Constructeur
aomdv_qpv_rqueue::aomdv_qpv_rqueue() : head_(0), tail_(0),
                                       len_(0), limit_(AOMDV_QPV_RTQ_MAX_LEN),
                                       timeout_(AOMDV_QPV_RTQ_TIMEOUT),
                                       agent_(0)
{
}

// Enqueue, dropping by priority when the queue is full
/**
 * @brief Enqueues a packet into the AOMDV QPV routing queue with priority handling.
 *
 * This function inserts a packet into the routing queue, ensuring that the queue does not exceed its size limit.
 * Before enqueuing, it purges expired packets. If the queue is full, it removes the packet with the lowest priority
 * (or the head if no such packet is found) to make space for the new packet. The removed packet is dropped with an
 * appropriate reason (queue full or timeout). The new packet is then added to the tail of the queue, and its timeout
 * is set accordingly.
 *
 * @param p Pointer to the packet to be enqueued.
 */
void aomdv_qpv_rqueue::enque(Packet *p)
{
  struct hdr_cmn *ch = HDR_CMN(p);

  // Purge the expired packets
  purge();

  // Queue full: sacrifice the lowest-priority packet
  if (len_ >= limit_)
  {
    Packet *lowestPrioPkt = findLowestPriorityPacket();
    Packet *prev = 0, *cur = head_;

    // If no low-priority packet was found, drop the head
    if (!lowestPrioPkt)
      lowestPrioPkt = head_;

    // Retirer lowestPrioPkt de la file
    if (lowestPrioPkt == head_)
    {
      lowestPrioPkt = remove_head();
    }
    else
    {
      // Find the predecessor
      while (cur && cur != lowestPrioPkt)
      {
        prev = cur;
        cur = cur->next_;
      }

      if (cur == lowestPrioPkt && prev)
      {
        prev->next_ = lowestPrioPkt->next_;

        if (lowestPrioPkt == tail_)
          tail_ = prev;

        len_--;
      }
    }

    // Drop with the appropriate reason
    if (HDR_CMN(lowestPrioPkt)->ts_ > CURRENT_TIME)
    {
      drop(lowestPrioPkt, agent_->mac_->phy_->dropReason(lowestPrioPkt, DROP_RTR_QFULL));
    }
    else
    {
      drop(lowestPrioPkt, agent_->mac_->phy_->dropReason(lowestPrioPkt, DROP_RTR_QTIMEOUT));
    }
  }

  // Enqueue normal
  p->next_ = 0;
  ch->ts_ = CURRENT_TIME + timeout_;

  if (!head_)
  {
    head_ = tail_ = p;
  }
  else
  {
    tail_->next_ = p;
    tail_ = p;
  }

  len_++;

#ifdef QDEBUG
  verifyQueue();
#endif // QDEBUG
}

// Dequeue the head
/**
 * @brief Dequeues the first packet from the routing queue.
 *
 * This method first purges any packets that have timed out from the queue.
 * It then removes and returns the packet at the head of the queue.
 * If queue debugging is enabled (QDEBUG), it verifies the integrity of the queue after removal.
 *
 * @return Pointer to the dequeued Packet, or nullptr if the queue is empty.
 */
Packet *aomdv_qpv_rqueue::deque()
{
  Packet *p;

  /*
   * Purge any packets that have timed out.
   */
  purge();

  p = remove_head();

#ifdef QDEBUG
  verifyQueue();
#endif // QDEBUG

  return p;
}

// Dequeue the first packet for a given destination
Packet *aomdv_qpv_rqueue::deque(nsaddr_t dst)
{
  Packet *p, *prev;

  /*
   * Purge any packets that have timed out.
   */
  purge();

  findPacketWithDst(dst, p, prev);
  assert(p == 0 || (p == head_ && prev == 0) || (prev->next_ == p));

  if (p == 0)
    return 0;

  if (p == head_)
  {
    p = remove_head();
  }
  else if (p == tail_)
  {
    prev->next_ = 0;
    tail_ = prev;
    len_--;
  }
  else
  {
    prev->next_ = p->next_;
    len_--;
  }

#ifdef QDEBUG
  verifyQueue();
#endif // QDEBUG

  return p;
}

// Test d'existence
char aomdv_qpv_rqueue::find(nsaddr_t dst)
{
  Packet *p, *prev;

  findPacketWithDst(dst, p, prev);

  if (0 == p)
    return 0;
  else
    return 1;
}

/*
  Private Routines
*/

// Removes and returns the head
/**
 * @brief Removes and returns the packet at the head of the queue.
 *
 * This function removes the packet currently at the head of the queue.
 * If the queue becomes empty after removal, both the head and tail pointers
 * are set to null. Otherwise, the head pointer is advanced to the next packet.
 * The queue length is decremented if a packet is removed.
 *
 * @return Pointer to the removed Packet, or nullptr if the queue was empty.
 */
Packet *aomdv_qpv_rqueue::remove_head()
{
  Packet *p = head_;

  if (head_ == tail_)
  {
    head_ = tail_ = 0;
  }
  else
  {
    head_ = head_->next_;
  }

  if (p)
    len_--;

  return p;
}

// Look up by destination
void aomdv_qpv_rqueue::findPacketWithDst(nsaddr_t dst, Packet *&p, Packet *&prev)
{
  p = prev = 0;

  for (p = head_; p; p = p->next_)
  {
    //		if(HDR_IP(p)->dst() == dst) {
    if (HDR_IP(p)->daddr() == dst)
    {
      return;
    }
    prev = p;
  }
}

// Internal consistency check
/**
 * @brief Verifies the integrity of the packet queue.
 *
 * This method traverses the linked list of packets starting from the head,
 * counting the number of packets and tracking the last packet in the queue.
 * It asserts that the counted number of packets matches the stored queue length (`len_`)
 * and that the last packet encountered is the same as the stored tail pointer (`tail_`).
 * These assertions help ensure that the internal state of the queue is consistent.
 */
void aomdv_qpv_rqueue::verifyQueue()
{
  Packet *p, *prev = 0;
  int cnt = 0;

  for (p = head_; p; p = p->next_)
  {
    cnt++;
    prev = p;
  }

  assert(cnt == len_);
  assert(prev == tail_);
}

/*
void aomdv_qpv_rqueue::purge()
{
  Packet *p;

  while ((p = head_) && HDR_CMN(p)->ts_ < CURRENT_TIME)
  {
    // assert(p == remove_head());
    p = remove_head();
    drop(p, agent_->mac_->phy_->dropReason(p, DROP_RTR_QTIMEOUT));
  }
}
*/

// Returns 1 if a packet has expired (ts_ < CURRENT_TIME)
/**
 * @brief Searches the queue for an aged (expired) packet.
 *
 * Iterates through the packet queue starting from the head. If a packet is found
 * whose timestamp (HDR_CMN(p)->ts_) is less than the current simulation time (CURRENT_TIME),
 * the function returns true and sets the pointers 'p' and 'prev' to the found packet and its
 * predecessor, respectively. If no such packet is found, returns false and sets both pointers to 0.
 *
 * @param[out] p    Reference to a pointer that will point to the aged packet if found, or 0 otherwise.
 * @param[out] prev Reference to a pointer that will point to the previous packet in the queue,
 *                  or 0 if the aged packet is at the head or not found.
 * @return true if an aged packet is found, false otherwise.
 */
bool aomdv_qpv_rqueue::findAgedPacket(Packet *&p, Packet *&prev)
{
  p = prev = 0;

  for (p = head_; p; p = p->next_)
  {
    if (HDR_CMN(p)->ts_ < CURRENT_TIME)
    {
      return true;
    }

    prev = p;
  }

  return false;
}

// Purge the expired packets
/**
 * @brief Removes all aged (expired) packets from the queue.
 *
 * This function iteratively searches for packets in the queue that have exceeded
 * their allowed lifetime (aged packets) and removes them. The removal process
 * handles packets at the head, tail, or in the middle of the queue. After each
 * removal, the queue's integrity is optionally verified in debug mode.
 *
 * The function stops when no more aged packets are found.
 *
 * Preconditions:
 * - The queue is properly initialized.
 *
 * Postconditions:
 * - All aged packets are removed from the queue.
 * - The queue's length and pointers are updated accordingly.
 */
void aomdv_qpv_rqueue::purge()
{
  Packet *p, *prev;

  while (findAgedPacket(p, prev))
  {
    assert(p == 0 || (p == head_ && prev == 0) || (prev->next_ == p));

    if (p == 0)
      return;

    if (p == head_)
    {
      p = remove_head();
    }
    else if (p == tail_)
    {
      prev->next_ = 0;
      tail_ = prev;
      len_--;
    }
    else
    {
      prev->next_ = p->next_;
      len_--;
    }

#ifdef QDEBUG
    verifyQueue();
#endif // QDEBUG

    p = prev = 0;
  }
}

/**
 * @brief Finds the packet with the lowest priority in the queue according to specific rules.
 *
 * The function iterates through the packet queue and selects the "lowest priority" packet
 * based on the following criteria:
 *   - Packets are classified into three classes:
 *       2: Video-D (frameType 'D')
 *       1: Video-M (other video frames)
 *       0: Non-video packets
 *   - Non-video packets are only considered if there are no video packets left in the queue.
 *   - Among video packets (D or M), the packet with the highest prioNb_ is selected first.
 *   - If multiple packets have the same class and priority, the first encountered is chosen.
 *
 * @return Pointer to the packet with the lowest priority according to the above rules,
 *         or nullptr if the queue is empty.
 */
Packet *aomdv_qpv_rqueue::findLowestPriorityPacket()
{
  Packet *lowest = 0;
  int lowestClass = 0; // 2: D, 1: M, 0: non-video
  int lowestPrio = -1;

  for (Packet *p = head_; p; p = p->next_)
  {
    struct hdr_cmn *ch = HDR_CMN(p);
    packet_t packetType = ch->ptype();
    char frameType = ch->frameType_[0];
    int prio = ch->prioNb_;

    int pktClass = 0; // non-video by default
    if (packetType == PT_VIDEO)
    {
      if (frameType == 'D')
        pktClass = 2; // video-D
      else if (frameType == 'M')
        pktClass = 1; // video-M
    }

    // A non-video packet is never sacrificed while video packets remain
    if (pktClass == 0)
    {
      if (lowest && lowestClass > 0)
        continue;
      if (!lowest)
        lowest = p;
      continue;
    }

    // For D or M: highest prioNb first
    if (pktClass > lowestClass ||
        (pktClass == lowestClass && prio > lowestPrio))
    {
      lowest = p;
      lowestClass = pktClass;
      lowestPrio = prio;
    }
  }

  return lowest;
}

// Unlinks an arbitrary node
void aomdv_qpv_rqueue::removeNode(Packet *target)
{
  Packet *prev = 0;

  for (Packet *cur = head_; cur; prev = cur, cur = cur->next_)
  {
    if (cur == target)
    {
      if (!prev)
        head_ = cur->next_;
      else
        prev->next_ = cur->next_;

      if (cur == tail_)
        tail_ = prev;

      len_--;

      return;
    }
  }
}
