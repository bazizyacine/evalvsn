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

            // A path that cannot be opened used to become a silent NULL and
            // a segmentation fault at the first send: refuse it loudly.
            if (ReceiverTraceFile == 0)
            {
                fprintf(stderr, "cannot open trace file %s\n", ReceiverTraceBuffer);
                abort();
            }
            // The banner st-packetGenerate.sh writes: what arrived is
            // declared in the same format as what was sent.
            fprintf(ReceiverTraceFile, "#!evsn-trace version=1 fields=8 payload=hex\n");
            fprintf(ReceiverTraceFile, "#sTime\tseqNb\tpSize\tframeNb\tframeType\tprioNb\toffset\tpayload\n");

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

evalvsnSink::evalvsnSink() : Agent(PT_UDP), TotalPacketsReceived(0), DuplicatesDiscarded(0)
{
    bind("packetSize_", &size_);
}

evalvsnSink::evalvsnSink(packet_t type) : Agent(type), TotalPacketsReceived(0), DuplicatesDiscarded(0)
{
    bind("packetSize_", &size_);
}

void evalvsnSink::sendmsg(int nbytes, AppData *data, const char *flags)
{
}

void evalvsnSink::recv(Packet *pkt, Handler *)
{
    hdr_cmn *ch = hdr_cmn::access(pkt);

    // A second copy of a packet - a route salvaged after a lost MAC
    // acknowledgement delivers both - is what a sink application discards
    // by sequence number. Counted apart, so that the received count and
    // the trace hold distinct packets and "received" can never exceed
    // "sent" (the test bench, 14 September 2026, with AOMDV_QPV:
    // twelve duplicates in 5168 packets).
    if (seenSeqNb.count(ch->seqNb_))
    {
        DuplicatesDiscarded++;
        Packet::free(pkt);
        return;
    }
    seenSeqNb.insert(ch->seqNb_);

    TotalPacketsReceived++;

    fprintf(ReceiverTraceFile, "%f\t%i\t%i\t%i\t%s\t%i\t%i", Scheduler::instance().clock(), ch->seqNb_, ch->pSize_, ch->frameNb_, ch->frameType_, ch->prioNb_, ch->offset__);

    // Eighth column, always (one format): the payload as it arrived, or
    // "-" when the packet carried none. The decoder decodes these payloads
    // alone, each packet carrying the index of its first coefficient (d53).
    if (pkt->userdata() != 0 && pkt->userdata()->type() == PACKET_DATA)
        fprintf(ReceiverTraceFile, "\t%s\n", (char *)((PacketData *)pkt->userdata())->data());
    else
        fprintf(ReceiverTraceFile, "\t-\n");

    if (app_)
    {
        app_->recv(ch->pSize_);
    }

    Packet::free(pkt);
}

void evalvsnSink::print_status()
{
    printf("Total packets received = %i\n", TotalPacketsReceived);
    printf("Duplicate packets discarded = %i\n", DuplicatesDiscarded);
}
