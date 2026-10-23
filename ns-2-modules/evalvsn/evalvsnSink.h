#include "agent.h"
#include "trafgen.h"
#include "packet.h"
#include <set>

class evalvsnSink : public Agent
{
public:
    evalvsnSink();
    evalvsnSink(packet_t);
    virtual void sendmsg(int nbytes, const char *flags = 0) { sendmsg(nbytes, NULL, flags); }
    virtual void sendmsg(int nbytes, AppData *data, const char *flags = 0);
    virtual void recv(Packet *pkt, Handler *);
    virtual int command(int argc, const char *const *argv);
    void print_status();

protected:
    char *ReceiverTraceBuffer;
    FILE *ReceiverTraceFile;
    int TotalPacketsReceived;
    int DuplicatesDiscarded;
    std::set<int> seenSeqNb;
};
