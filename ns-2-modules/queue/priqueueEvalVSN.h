#include <string.h>
#include "queue.h"
#include "config.h"
#include <map>
#include <mac/mac-802_11EvalVSN.h>

struct PacketCountStats
{
    int total;
    int video;
    int non_video;
    int video_M;
    int video_D;
    map<int, int> prio_count; // key = priority, value = number of packets

    PacketCountStats() : total(0), video(0), non_video(0), video_M(0), video_D(0) {}
};

class AOMDV_QPV;
class Mac802_11EvalVSN;

class priqueueEvalVSN : public Queue
{
public:
    priqueueEvalVSN();
    ~priqueueEvalVSN();

    void setAOMDV_QPV(AOMDV_QPV *agent) { aomdv_qpv_agent_ = agent; }
    AOMDV_QPV *getAOMDV_QPV() const { return aomdv_qpv_agent_; }

    /// Expose la longueur courante de la file
    int getQueueLength() const { return q_ ? q_->length() : 0; }
    /// (Optionnel) Retourne l'occupation en octets
    int getQueueBytes() const { return q_ ? q_->byteLength() : 0; }
    /// Expose la limite en paquets de la file
    int getQlim() const { return qlim_; }

protected:
    int command(int argc, const char *const *argv);

    void reset();

    void enque(Packet *);
    Packet *deque();

    void shrink_queue();

    void print_summarystats();

    // Added for efficient sorting and replacement
    Packet *findLowestPriorityPacket();
    void insertSorted(Packet *p);

    void log(char *event, char *additional);

    void notifyCongestion(); // Called on congestion
    void dropLowestPriority();

    bool notifyCongestionThreshold(Packet *p, PacketCountStats &stats, int congestionThreshold);
    int computeCongestionThreshold(Packet *p);
    PacketCountStats countPackets();

private:
    PacketQueue *q_;
    int drop_front_;
    int summarystats;
    int qib_;
    int mean_pktsize_;
    int IFQ_DBG;
    bool congestedIFQ_;
    AOMDV_QPV *aomdv_qpv_agent_; // Pointer to the AOMDV_QPV agent

    friend class AOMDV_QPV; // Lets AOMDV_QPV reach the private members
};
