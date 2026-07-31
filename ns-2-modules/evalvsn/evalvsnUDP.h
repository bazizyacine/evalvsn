#include "agent.h"
#include "trafgen.h"
#include "packet.h"

class evalvsnUDP : public Agent
{
public:
    evalvsnUDP();
    evalvsnUDP(packet_t);
    virtual void sendmsg(int nbytes, const char *flags = 0) { sendmsg(nbytes, NULL, flags); }
    virtual void sendmsg(int nbytes, AppData *data, const char *flags = 0);
    virtual void recv(Packet *pkt, Handler *);
    virtual int command(int argc, const char *const *argv);
    void print_status(); // Ajout de la méthode

protected:
    char *SenderTraceBuffer;
    FILE *SenderTraceFile;
    int TotalPacketsSent; // Ajout du compteur
};
