#include "evalvsnSink.h"
#include "rtp.h"
#include "random.h"
#include "address.h"
#include "ip.h"

static class evalvsnSinkClass : public TclClass
{
public:
    evalvsnSinkClass() : TclClass("Agent/evalvsnSink")
    {
    }

    TclObject *create(int, const char *const *)
    {
        return (new evalvsnSink());
    }
} class_evalvsnSink;

int evalvsnSink::command(int argc, const char *const *argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "printstatus") == 0)
        {
            print_status();

            return (TCL_OK);
        }
    }

    if (argc == 2)
    {
        if (strcmp(argv[1], "closefile") == 0)
        {
            fclose(ReceiverTraceFile);

            return (TCL_OK);
        }
    }

    if (argc == 3)
    {
        if (strcmp(argv[1], "set_filename") == 0)
        {
            ReceiverTraceBuffer = new char[strlen(argv[2]) + 1];
            strcpy(ReceiverTraceBuffer, argv[2]);
            ReceiverTraceFile = fopen(ReceiverTraceBuffer, "w");
            fprintf(ReceiverTraceFile, "#sTime\tseqNb\tpSize\tframeNb\tframeType\tprioNb\toffset\n");

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

evalvsnSink::evalvsnSink() : Agent(PT_UDP), TotalPacketsReceived(0)
{
    bind("packetSize_", &size_);
}

evalvsnSink::evalvsnSink(packet_t type) : Agent(type), TotalPacketsReceived(0)
{
    bind("packetSize_", &size_);
}

void evalvsnSink::sendmsg(int nbytes, AppData *data, const char *flags)
{
}

void evalvsnSink::recv(Packet *pkt, Handler *)
{
    hdr_cmn *ch = hdr_cmn::access(pkt);

    TotalPacketsReceived++;

    fprintf(ReceiverTraceFile, "%f\t%i\t%i\t%i\t%s\t%i\t%i\n", Scheduler::instance().clock(), ch->seqNb_, ch->pSize_, ch->frameNb_, ch->frameType_, ch->prioNb_, ch->offset__);

    if (app_)
    {
        app_->recv(ch->pSize_);
    }

    Packet::free(pkt);
}

void evalvsnSink::print_status()
{
    printf("Total packets received = %i\n", TotalPacketsReceived);
}
