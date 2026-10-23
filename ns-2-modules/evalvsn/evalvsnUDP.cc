#include "evalvsnUDP.h"
#include "rtp.h"
#include "random.h"
#include "address.h"
#include "ip.h"

static class evalvsnUDPClass : public TclClass
{
public:
    evalvsnUDPClass() : TclClass("Agent/evalvsnUDP")
    {
    }

    TclObject *create(int, const char *const *)
    {
        return (new evalvsnUDP());
    }
} class_evalvsnUDP;

int evalvsnUDP::command(int argc, const char *const *argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "closefile") == 0)
        {
            fclose(SenderTraceFile);
            return (TCL_OK);
        }

        if (strcmp(argv[1], "printstatus") == 0)
        {
            print_status();
            return (TCL_OK);
        }
    }

    if (argc == 3)
    {
        if (strcmp(argv[1], "set_filename") == 0)
        {
            SenderTraceBuffer = new char[strlen(argv[2]) + 1];
            strcpy(SenderTraceBuffer, argv[2]);
            SenderTraceFile = fopen(SenderTraceBuffer, "w");

            // A path that cannot be opened used to become a silent NULL and
            // a segmentation fault at the first send: refuse it loudly.
            if (SenderTraceFile == 0)
            {
                fprintf(stderr, "cannot open trace file %s\n", SenderTraceBuffer);
                abort();
            }
            // The banner st-packetGenerate.sh writes, so the trace this agent
            // mirrors and the trace it was fed are the same format to every
            // reader.
            fprintf(SenderTraceFile, "#!evsn-trace version=1 fields=8 payload=hex\n");
            fprintf(SenderTraceFile, "#sTime\tseqNb\tpSize\tframeNb\tframeType\tprioNb\toffset\tpayload\n");

            return (TCL_OK);
        }
    }

    if (argc == 4)
    {
        if (strcmp(argv[1], "send") == 0)
        {
            PacketData *data = new PacketData(1 + strlen(argv[3]));
            strcpy((char *)data->data(), argv[3]);
            sendmsg(atoi(argv[2]), data);

            return (TCL_OK);
        }
    }

    if (argc == 5)
    {
        if (strcmp(argv[1], "sendmsg") == 0)
        {
            PacketData *data = new PacketData(1 + strlen(argv[3]));
            strcpy((char *)data->data(), argv[3]);
            sendmsg(atoi(argv[2]), data, argv[4]);

            return (TCL_OK);
        }
    }

    return (Agent::command(argc, argv));
}

evalvsnUDP::evalvsnUDP() : Agent(PT_UDP), TotalPacketsSent(0)
{
    bind("packetSize_", &size_);
}

evalvsnUDP::evalvsnUDP(packet_t type) : Agent(type), TotalPacketsSent(0)
{
    bind("packetSize_", &size_);
}

void evalvsnUDP::sendmsg(int nbytes, AppData *data, const char *flags)
{
    Packet *p = allocpkt();
    hdr_cmn *ch = hdr_cmn::access(p);

    ch->size_ = nbytes;

    fprintf(SenderTraceFile, "%f\t%i\t%i\t%i\t%s\t%i\t%i", Scheduler::instance().clock(), ch->seqNb_, ch->pSize_, ch->frameNb_, ch->frameType_, ch->prioNb_, ch->offset__);

    // Eighth column, always (one format, the author ruled): the payload
    // this packet carries, or "-" when it carries none.
    if (data != 0 && data->type() == PACKET_DATA)
        fprintf(SenderTraceFile, "\t%s\n", (char *)((PacketData *)data)->data());
    else
        fprintf(SenderTraceFile, "\t-\n");

    p->setdata(data);

    target_->recv(p);

    idle();

    TotalPacketsSent++; // Bump the counter
}

void evalvsnUDP::recv(Packet *pkt, Handler *)
{
}

void evalvsnUDP::print_status()
{
    printf("Total packets sent = %i\n", TotalPacketsSent);
}
