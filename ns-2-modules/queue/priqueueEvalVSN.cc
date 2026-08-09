#include "priqueueEvalVSN.h"
#include <iostream>
#include <aomdv_qpv/aomdv_qpv.h>

using namespace std;

#define DROP_IFQ_QFULL_REPL "REPL"
#define DROP_IFQ_QFULL_NEW "NEW"
#define DROP_IFQ_QFULL_SHRK "SHRK"
#define DROP_IFQ_MANUAL_DROP "MANU"

static class priqueueEvalVSNClass : public TclClass
{
public:
    priqueueEvalVSNClass() : TclClass("Queue/priqueueEvalVSN")
    {
    }
    TclObject *create(int, const char *const *)
    {
        return (new priqueueEvalVSN);
    }
} class_priqueueEvalVSN;

/**
 * @brief Handles Tcl command interface for the priqueueEvalVSN class.
 *
 * This method processes commands passed from the Tcl interpreter to the C++ object.
 * It supports the following commands:
 * - "printstats": Prints summary statistics of the queue.
 * - "shrink-queue": Shrinks the queue.
 * - "packetqueue-attach <queue>": Attaches a PacketQueue object by looking it up via TclObject.
 * - "setAOMDV_QPV <agent>": Sets the AOMDV_QPV agent by looking it up via TclObject.
 *
 * If the command is not recognized, it delegates processing to the base Queue class.
 *
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return TCL_OK on success, TCL_ERROR on failure, or the result of Queue::command for unhandled commands.
 */
int priqueueEvalVSN::command(int argc, const char *const *argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "printstats") == 0)
        {
            print_summarystats();
            return (TCL_OK);
        }
        else if (strcmp(argv[1], "shrink-queue") == 0)
        {
            shrink_queue();
            return (TCL_OK);
        }
    }
    else if (argc == 3)
    {
        if (!strcmp(argv[1], "packetqueue-attach"))
        {
            delete q_;

            if (!(q_ = (PacketQueue *)TclObject::lookup(argv[2])))
            {
                return (TCL_ERROR);
            }
            else
            {
                pq_ = q_;
                return (TCL_OK);
            }
        }
        else if (!strcmp(argv[1], "setAOMDV_QPV"))
        {
            aomdv_qpv_agent_ = (AOMDV_QPV *)TclObject::lookup(argv[2]);

            if (aomdv_qpv_agent_ == 0)
                return TCL_ERROR;
            return (TCL_OK);
        }
    }

    return Queue::command(argc, argv);
}

/**
 * @brief Constructor for the priqueueEvalVSN class.
 *
 * Initializes the priority queue evaluation object for VSN (Vehicular Sensor Networks).
 * - Allocates a new PacketQueue and assigns it to both q_ and pq_.
 * - Sets the congestedIFQ_ flag to false.
 * - Binds several configuration parameters and flags for runtime control:
 *   - drop_front_: Whether to drop packets from the front of the queue.
 *   - summarystats: Whether to collect summary statistics.
 *   - queue_in_bytes_: Whether the queue is measured in bytes.
 *   - mean_pktsize_: The mean packet size.
 *   - IFQ_DBG: Debug flag for the interface queue.
 * - Initializes the aomdv_qpv_agent_ pointer to NULL.
 */
priqueueEvalVSN::priqueueEvalVSN() : aomdv_qpv_agent_(NULL)
{
    q_ = new PacketQueue;
    pq_ = q_;
    congestedIFQ_ = false;

    bind_bool("drop_front_", &drop_front_);
    bind_bool("summarystats_", &summarystats);
    bind_bool("queue_in_bytes_", &qib_);
    bind("mean_pktsize_", &mean_pktsize_);
    bind("IFQ_DBG", &IFQ_DBG);
}

priqueueEvalVSN::~priqueueEvalVSN()
{
    delete q_;
}

void priqueueEvalVSN::reset()
{
    Queue::reset();
}

/**
 * @brief Enqueues a packet into the priority queue with congestion and priority handling.
 *
 * This method attempts to insert a packet into the queue, considering queue limits and packet priorities.
 * If the queue is not full, the packet is inserted in sorted order based on its priority.
 * If the queue is full, congestion is notified and the method checks if the incoming packet has a higher
 * priority than the lowest priority packet in the queue. If so, the lowest priority packet is replaced and dropped;
 * otherwise, the incoming packet is dropped.
 *
 * Priority rules:
 * - Non-video packets are the most prioritary.
 * - Video packets are classified by frame type: "M" (medium priority), "D" (lowest priority).
 *   (So: non-video > M > D)
 * - Among packets of the same type, lower prioNb_ values indicate higher priority.
 *
 * @param p Pointer to the packet to be enqueued.
 */
void priqueueEvalVSN::enque(Packet *p)
{
    int qlimBytes = qlim_ * mean_pktsize_;

    if (summarystats)
        Queue::updateStats(qib_ ? q_->byteLength() : q_->length());

    bool full = (!qib_ && (q_->length() >= qlim_)) ||
                (qib_ && (q_->byteLength() + hdr_cmn::access(p)->size() > qlimBytes));

    congestedIFQ_ = full;

    if (!full)
    {
        insertSorted(p);
    }
    else
    {
        notifyCongestion();
        Packet *lowest = findLowestPriorityPacket();
        bool replace = false;

        if (lowest)
        {
            int lp_type = 2, p_type = 2; // 0: non-video, 1: M, 2: D

            if (hdr_cmn::access(lowest)->ptype() == PT_VIDEO)
            {
                if (strcmp(hdr_cmn::access(lowest)->frameType_, "M") == 0)
                    lp_type = 1;
                else if (strcmp(hdr_cmn::access(lowest)->frameType_, "D") == 0)
                    lp_type = 2;
            }
            else
            {
                lp_type = 0;
            }

            if (hdr_cmn::access(p)->ptype() == PT_VIDEO)
            {
                if (strcmp(hdr_cmn::access(p)->frameType_, "M") == 0)
                    p_type = 1;
                else if (strcmp(hdr_cmn::access(p)->frameType_, "D") == 0)
                    p_type = 2;
            }
            else
            {
                p_type = 0;
            }

            // Priority: 0 (non-video) > 1 (M) > 2 (D)
            if (p_type < lp_type)
                replace = true;
            else if (p_type == lp_type && hdr_cmn::access(p)->prioNb_ < hdr_cmn::access(lowest)->prioNb_)
                replace = true;
        }

        if (replace)
        {
            q_->remove(lowest);
            drop(lowest, aomdv_qpv_agent_->mac_->phy_->dropReason(lowest, DROP_IFQ_QFULL_REPL));
            insertSorted(p);
        }
        else
        {
            drop(p, aomdv_qpv_agent_->mac_->phy_->dropReason(p, DROP_IFQ_QFULL_NEW));
        }
    }
}

/**
 * @brief Finds the packet with the lowest priority in the queue according to custom video/non-video rules.
 *
 * This function iterates through all packets in the queue and selects the packet to be dropped based on the following logic:
 * - Packets are classified into three classes:
 *   - 2: Video-D (frameType 'D')
 *   - 1: Video-M (other video frames)
 *   - 0: Non-video
 * - Non-video packets are only considered if there are no video packets left in the queue.
 * - Among video packets (D or M), the packet with the highest prioNb_ is selected first.
 * - The function returns a pointer to the selected packet, or nullptr if the queue is empty.
 *
 * @return Packet* Pointer to the packet with the lowest priority to be dropped, or nullptr if none found.
 */
Packet *priqueueEvalVSN::findLowestPriorityPacket()
{
    Packet *lowest = 0;
    int lowestClass = 0; // 2: D, 1: M, 0: non-video
    int lowestPrio = -1;

    for (int i = 0; i < q_->length(); ++i)
    {
        Packet *pi = q_->lookup(i);
        struct hdr_cmn *ch = hdr_cmn::access(pi);
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
                lowest = pi;
            continue;
        }

        // For D or M: highest prioNb first
        if (pktClass > lowestClass ||
            (pktClass == lowestClass && prio > lowestPrio))
        {
            lowest = pi;
            lowestClass = pktClass;
            lowestPrio = prio;
        }
    }

    return lowest;
}

/**
 * @brief Inserts a packet into the queue in sorted order based on type and priority.
 *
 * This function finds the correct position to insert the packet `p` into the queue `q_` so that the queue remains sorted.
 * Sorting rules:
 * - Packets of type PT_VIDEO are classified by frame type:
 *   - 'D' (diff frame): type 2 (highest priority among video)
 *   - 'M' (main frame): type 1
 *   - Other video frames: type 1
 *   - Non-video: type 0 (lowest priority)
 * - Non-video packets are always placed before video packets.
 * - Among video packets, 'M' frames are placed before 'D' frames.
 * - For packets of the same type, lower prioNb_ means higher priority (inserted earlier).
 *
 * The function removes packets after the insertion position, inserts the new packet,
 * then re-inserts the removed packets to maintain the sorted order.
 *
 * @param p Pointer to the Packet to be inserted into the queue.
 */
void priqueueEvalVSN::insertSorted(Packet *p)
{
    int insert_pos = q_->length();
    int p_type = 1; // 0: non-video, 1: video-M, 2: video-D

    if (hdr_cmn::access(p)->ptype() == PT_VIDEO)
    {
        if (hdr_cmn::access(p)->frameType_[0] == 'D')
            p_type = 2;
        else if (hdr_cmn::access(p)->frameType_[0] == 'M')
            p_type = 1;
    }
    else
    {
        p_type = 0;
    }

    for (int i = 0; i < q_->length(); ++i)
    {
        Packet *pi = q_->lookup(i);
        int pi_type = 1;

        if (hdr_cmn::access(pi)->ptype() == PT_VIDEO)
        {
            if (hdr_cmn::access(pi)->frameType_[0] == 'D')
                pi_type = 2;
            else if (hdr_cmn::access(pi)->frameType_[0] == 'M')
                pi_type = 1;
            else
                pi_type = 1;
        }
        else
        {
            pi_type = 0;
        }

        // Non-video always goes first
        if (p_type == 0 && pi_type != 0)
        {
            insert_pos = i;
            break;
        }
        // Video-M before video-D
        else if (p_type == 1 && pi_type == 2)
        {
            insert_pos = i;
            break;
        }
        // Same type, by increasing priority
        else if (p_type == pi_type && hdr_cmn::access(p)->prioNb_ < hdr_cmn::access(pi)->prioNb_)
        {
            insert_pos = i;
            break;
        }
    }

    Packet *tmp[1024];
    int n = 0;

    while (q_->length() > insert_pos && n < 1024)
    {
        tmp[n++] = q_->lookup(insert_pos);
        q_->remove(tmp[n - 1]);
    }

    q_->enque(p);

    for (int i = 0; i < n; ++i)
        q_->enque(tmp[i]);
}

void priqueueEvalVSN::shrink_queue()
{
    while ((!qib_ && q_->length() > qlim_) ||
           (qib_ && q_->byteLength() > qlim_ * mean_pktsize_))
    {
        Packet *lowest = findLowestPriorityPacket();

        if (lowest)
        {
            q_->remove(lowest);
            drop(lowest, aomdv_qpv_agent_->mac_->phy_->dropReason(lowest, DROP_IFQ_QFULL_SHRK));
        }
        else
        {
            break;
        }
    }
}

/**
 * Dequeues a packet from the priority queue.
 *
 * If summary statistics are enabled and the Scheduler instance is valid,
 * updates the queue statistics before dequeuing. The statistics updated
 * depend on whether the queue is measured in bytes or packets.
 *
 * @return Pointer to the dequeued Packet, or nullptr if the queue is empty.
 */
Packet *priqueueEvalVSN::deque()
{
    if (summarystats && &Scheduler::instance() != NULL)
        Queue::updateStats(qib_ ? q_->byteLength() : q_->length());

    return q_->deque();
}

void priqueueEvalVSN::print_summarystats()
{
    printf("priqueueEvalVSN stats:\n");
    printf("  Nombre de paquets : %d\n", q_->length());
    printf("  Taille totale (octets) : %d\n", q_->byteLength());

    printf("  True average queue: %5.3f", true_ave_);

    if (qib_)
        printf("  (in bytes)");

    printf("  time: %5.3f\n", total_time_);
}

void priqueueEvalVSN::log(char *event, char *additional)
{
    if (IFQ_DBG)
        cout << "L " << Scheduler::instance().clock() << " " << "IFQ" << " " << event << " "
             << additional << endl;
}

/**
 * @brief Counts and categorizes packets in the priority queue.
 *
 * Iterates through all packets in the queue and collects statistics:
 * - Total number of packets.
 * - Number of video packets and non-video packets.
 * - Number of video packets of type 'M' and 'D' (based on the first character of frameType_).
 * - Number of packets per priority value.
 *
 * If debugging is enabled (IFQ_DBG), logs the collected statistics.
 *
 * @return PacketCountStats Structure containing the counted statistics.
 */
PacketCountStats priqueueEvalVSN::countPackets()
{
    PacketCountStats stats;

    for (int i = 0; i < q_->length(); ++i)
    {
        Packet *p = q_->lookup(i);

        stats.total++;

        int prio = hdr_cmn::access(p)->prioNb_;

        stats.prio_count[prio]++;

        if (hdr_cmn::access(p)->ptype() == PT_VIDEO)
        {
            stats.video++;
            // frameType_ is a char array; only the first character matters
            char frameType = hdr_cmn::access(p)->frameType_[0];

            if (frameType == 'M')
                stats.video_M++;
            else if (frameType == 'D')
                stats.video_D++;
        }
        else
        {
            stats.non_video++;
        }
    }

    // Print the packet-count statistics
    if (IFQ_DBG)
    {
        char msg[1000];

        sprintf(msg, "Total: %d, Video: %d, Non-Video: %d, M: %d, D: %d",
                stats.total, stats.video, stats.non_video, stats.video_M, stats.video_D);
        log("countPackets (cross-layer)", msg);

        for (std::map<int, int>::iterator it = stats.prio_count.begin(); it != stats.prio_count.end(); ++it)
        {
            sprintf(msg, "Priority %d: %d packets", it->first, it->second);
            log("countPackets (cross-layer)", msg);
        }
    }

    return stats;
}

/**
 * @brief Drops the packet with the lowest priority from the queue.
 *
 * This method locates the packet with the lowest priority in the queue,
 * removes it, and invokes the drop mechanism with the appropriate drop reason.
 * The drop reason is determined by the MAC layer's physical drop reason handler.
 *
 * If no packet is found (i.e., the queue is empty), the method does nothing.
 */
void priqueueEvalVSN::dropLowestPriority()
{
    Packet *lowest = findLowestPriorityPacket();

    if (lowest)
    {
        q_->remove(lowest);
        drop(lowest, aomdv_qpv_agent_->mac_->phy_->dropReason(lowest, DROP_IFQ_MANUAL_DROP));
    }
}

/**
 * @brief Notifies the associated agent of congestion in the interface queue.
 *
 * This method checks if the AOMDV_QPV agent is set and if the interface queue is congested.
 * If both conditions are true, it calls the agent's notifyCongestionIFQ method, passing
 * the current priqueueEvalVSN instance as a parameter.
 */
void priqueueEvalVSN::notifyCongestion()
{
    if (aomdv_qpv_agent_)
    {
        if (congestedIFQ_)
        {
            aomdv_qpv_agent_->notifyCongestionIFQ(this);
        }
    }
}

/**
 * @brief Checks if the congestion threshold has been reached for a given packet.
 *
 * This function evaluates whether the congestion threshold is exceeded based on
 * the type and priority of the packet, as well as statistics collected in PacketCountStats.
 * For video packets, it distinguishes between main frames ('M'), diff frames ('D'), and other types,
 * applying the threshold to the corresponding counters and priority. For non-video packets,
 * the threshold is checked against the non-video counter and priority.
 *
 * @param p Pointer to the packet to be evaluated.
 * @param stats Reference to the statistics object containing packet counters.
 * @param congestionThreshold The threshold value to determine congestion.
 * @return true if the congestion threshold is reached for the packet type or priority, false otherwise.
 */
bool priqueueEvalVSN::notifyCongestionThreshold(Packet *p, PacketCountStats &stats, int congestionThreshold)
{
    struct hdr_cmn *ch = HDR_CMN(p);
    packet_t packetType = ch->ptype();
    char frameType = ch->frameType_[0];
    int priority = ch->prioNb_;

    bool congestedThreshold = false;
    int prio_count = 0;
    map<int, int>::const_iterator it = stats.prio_count.find(priority);

    if (it != stats.prio_count.end())
        prio_count = it->second;

    if (packetType == PT_VIDEO)
    {
        if (frameType == 'M')
        {
            // Main frame: congestionThreshold over the main frames and the priority
            if ((stats.video_M >= congestionThreshold) || (prio_count >= congestionThreshold))
                congestedThreshold = true;
        }
        else if (frameType == 'D')
        {
            // Diff frame: congestionThreshold over the diff frames and the priority
            if ((stats.video_D >= congestionThreshold) || (prio_count >= congestionThreshold))
                congestedThreshold = true;
        }
        else
        {
            // Other video: congestionThreshold over all video packets and the priority
            if ((stats.video >= congestionThreshold) || (prio_count >= congestionThreshold))
                congestedThreshold = true;
        }
    }
    else
    {
        // Non-video: congestionThreshold over the non-video packets and the priority
        if ((stats.non_video >= congestionThreshold) || (prio_count >= congestionThreshold))
            congestedThreshold = true;
    }

    return congestedThreshold;
}

/**
 * @brief Computes the congestion threshold for a given packet based on multiple factors.
 *
 * The congestion threshold determines the point at which congestion control mechanisms
 * should be triggered for a packet in the queue. The calculation considers:
 *   - Packet type (e.g., video or other)
 *   - Frame type for video packets ('M' or 'D')
 *   - Packet priority (higher priority yields lower threshold)
 *   - Current queue length relative to its maximum limit
 *
 * The final threshold is always at least 1.
 *
 * @param p Pointer to the Packet for which the congestion threshold is computed.
 * @return The computed congestion threshold as an integer (minimum value is 1).
 */
int priqueueEvalVSN::computeCongestionThreshold(Packet *p)
{
    struct hdr_cmn *ch = HDR_CMN(p);
    packet_t packetType = ch->ptype();
    char frameType = ch->frameType_[0];
    int priority = ch->prioNb_;

    // Base threshold
    double base = 10.0;

    // Packet-type factor
    double type_factor = (packetType == PT_VIDEO) ? 0.7 : 1.0;

    // Frame-type factor (three cases: other, M, D)
    double frame_factor;

    if (packetType != PT_VIDEO)
    {
        frame_factor = 1.0; // anything else (non-video)
    }
    else if (frameType == 'M')
    {
        frame_factor = 0.8; // M
    }
    else
    {
        frame_factor = 0.6; // D
    }

    // Priority factor: the higher the priority, the lower the congestionThreshold
    // priority.cc assigns dataPriority from 1 (the most critical) to
    // getMaxPrioNb() (the least critical); the value 0 is never produced. With
    // 1 - p/max the last level came out at exactly zero, hence a threshold
    // pulled down to the minimum by the max(1) below: it was declared congested
    // from the first unit of occupancy. The +1 lets it decrease without
    // degenerating.
    double prio_factor = double(aomdv_qpv_agent_->mac_->phy_->getMaxPrioNb() + 1 - priority) / double(aomdv_qpv_agent_->mac_->phy_->getMaxPrioNb());

    // Queue-length factor: the longer the queue, the lower the congestionThreshold
    double queue_factor = 1.0;

    if (qlim_ > 0)
    {
        // For instance: congestionThreshold falls linearly once the queue exceeds q_->length() packets, up to qlim_
        queue_factor = 1.0 - min(0.5, double(getQueueLength()) / double(getQlim()));
    }

    // congestionThreshold
    int congestionThreshold = int(base * type_factor * frame_factor * prio_factor * queue_factor);

    // Toujours au moins 1
    if (congestionThreshold < 1)
        congestionThreshold = 1;

    return congestionThreshold;
}
