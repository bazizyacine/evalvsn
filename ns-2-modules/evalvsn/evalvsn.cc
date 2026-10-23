#include "evalvsn.h"

static class evalvsnFileClass : public TclClass
{
public:
    evalvsnFileClass() : TclClass("evalvsnFile") {}
    TclObject *create(int, const char *const *) { return (new evalvsnFile()); }
} class_evalvsnFile;

static class evalvsnClass : public TclClass
{
public:
    evalvsnClass() : TclClass("Application/Traffic/evalvsn") {}
    TclObject *create(int, const char *const *) { return (new evalvsn()); }
} class_evalvsn;

int evalvsnFile::command(int argc, const char *const *argv)
{
    if (argc == 3)
    {
        if (strcmp(argv[1], "filename") == 0)
        {
            name_ = new char[strlen(argv[2]) + 1];
            strcpy(name_, argv[2]);

            return (TCL_OK);
        }
    }

    return (NsObject::command(argc, argv));
}

evalvsnFile::evalvsnFile() : status_(0), a_(0)
{
}

int evalvsnFile::get_a()
{
    return a_;
}

void evalvsnFile::get_next(int &ndx, packetsInfo &t)
{
    t.sTime = trace_[ndx].sTime;
    t.seqNb = trace_[ndx].seqNb;
    t.pSize = trace_[ndx].pSize;
    t.frameNb = trace_[ndx].frameNb;
    strcpy(t.frameType, trace_[ndx].frameType);
    t.prioNb = trace_[ndx].prioNb;
    t.offset = trace_[ndx].offset;
    t.payload = trace_[ndx].payload;

    // trace_ holds nrec_ - 1 records, so nrec_ - 2 is the last of them. The
    // old test - post-increment compared to nrec_ - 1 - wrapped one call too
    // late: the wrap call itself copied trace_[nrec_ - 1], one past the end,
    // and the strcpy of that garbage frameType could run over the caller's
    // record and what sits after it. The fetched garbage was never sent -
    // a_ stops the traffic first - so wrapping one fetch earlier sends
    // exactly the same packets, minus the out-of-bounds read.
    if (++ndx >= nrec_ - 1)
    {
        ndx = 0;
        a_ = 1;
    }
}

int evalvsnFile::setup()
{
    FILE *pFile;
    packetsInfo *t;
    double sTime, sTimeLast = 0;
    char frameType[10];
    // Wide enough for the seven fields plus a hexadecimal payload column: a
    // 240-byte payload is 480 characters. The payload is now parsed, not
    // skipped, so a line this buffer would truncate is a payload this reader
    // would corrupt - hence the check against payloadBuf below.
    char line[4096];
    char payloadBuf[4096];
    int seqNb, pSize, frameNb, prioNb, offset, i, nf;

    nrec_ = 0;

    if (!status_)
    {
        status_ = 1;

        if ((pFile = fopen(name_, "r")) == NULL)
        {
            printf("can't open file %s\n", name_);

            return -1;
        }

        // Comment lines are skipped by their '#', not by a byte offset. The
        // header used to be jumped with fseek(pFile, 51, SEEK_SET) - the exact
        // width of one header line - so any change to that line silently
        // started the read mid-record. Reading whole lines and parsing them
        // with sscanf also makes trailing columns harmless, where fscanf on
        // the stream swallowed them as the first field of the next record.
        while (fgets(line, sizeof(line), pFile) != NULL)
        {
            if (line[0] == '#' || line[0] == '\n')
                continue;

            if (sscanf(line, "%lf %i %i %i %9s %i %i",
                       &sTime, &seqNb, &pSize, &frameNb, frameType, &prioNb, &offset) == 7)
                nrec_++;
        }

        // nrec_ keeps the meaning the rest of this class was written around:
        // one more than the number of records. Only the way records are found
        // changed here; changing the arithmetic as well broke playback.
        nrec_++;

        printf("Total packets = %i\n", nrec_ - 1);

        rewind(pFile);

        trace_ = new packetsInfo[nrec_ - 1];

        i = 0;
        t = trace_;

        while (i < nrec_ - 1 && fgets(line, sizeof(line), pFile) != NULL)
        {
            if (line[0] == '#' || line[0] == '\n')
                continue;

            nf = sscanf(line, "%lf %i %i %i %9s %i %i %4095s",
                        &sTime, &seqNb, &pSize, &frameNb, frameType, &prioNb, &offset, payloadBuf);

            // Eight columns, strictly: the refonte has one trace format and
            // the author closed the seven-column tolerance with it. An old
            // trace fails loudly instead of playing without its payload.
            if (nf != 8)
            {
                printf("record %i of %s has %i fields; the format is 8 columns with the payload last\n",
                       i, name_, nf);
                abort();
            }

            t->sTime = sTime - sTimeLast;

            t->seqNb = seqNb;

            t->pSize = pSize;

            t->frameNb = frameNb;

            strcpy(t->frameType, frameType);

            t->prioNb = prioNb;

            t->offset = offset;

            // Eighth column: the hexadecimal payload (R17), always present.
            // A payload that fills the buffer to the brim was truncated by
            // fgets: refuse the record loudly rather than transport a
            // corrupt payload. The "-" marker carries an empty one.
            if (strlen(payloadBuf) >= sizeof(payloadBuf) - 1)
            {
                printf("payload of packet %i is longer than this reader's %zu-character buffer\n",
                       seqNb, sizeof(payloadBuf) - 1);
                abort();
            }

            if (strcmp(payloadBuf, "-") == 0)
                t->payload = 0;
            else
            {
                t->payload = new char[strlen(payloadBuf) + 1];
                strcpy(t->payload, payloadBuf);
            }

            sTimeLast = sTime;

            i++;
            t++;
        }

        fclose(pFile);
    }

    return 0;
}

void evalvsnFile::recv(Packet *, Handler *)
{
    abort();
}

int evalvsn::command(int argc, const char *const *argv)
{
    Tcl &tcl = Tcl::instance();

    if (argc == 3)
    {
        if (strcmp(argv[1], "attach-tracefile") == 0)
        {
            // A checked cast, not a C cast: given ns-2's own Tracefile, the
            // C cast accepted an object 4 bytes too short for a_, and the
            // end-of-trace flag was then read from, and written into, the
            // next heap chunk (14 September 2026).
            tfile_ = dynamic_cast<evalvsnFile *>(TclObject::lookup(argv[2]));

            if (tfile_ == 0)
            {
                tcl.resultf("attach-tracefile: %s is not an evalvsnFile", argv[2]);

                return (TCL_ERROR);
            }

            return (TCL_OK);
        }
    }

    return (TrafficGenerator::command(argc, argv));
}

evalvsn::evalvsn()
{
    tfile_ = (evalvsnFile *)NULL;

    bind("packetSize_", &size_);
}

void evalvsn::init()
{
    if (tfile_)
    {
        ndx_ = tfile_->setup();
    }
}

void evalvsn::timeout()
{
    if (!running_)
    {
        return;
    }

    agent_->set_pkttype(PT_VIDEO);

    agent_->set_seqNb(trec_.seqNb);

    agent_->set_pSize(trec_.pSize);

    agent_->set_frameNb(trec_.frameNb);

    agent_->set_frameType(trec_.frameType);

    agent_->set_prioNb(trec_.prioNb);

    agent_->set_offset(trec_.offset);

    // The payload travels in the packet (R17): attached as PacketData, the
    // ns-2 mechanism evalvsnUDP::sendmsg() already forwards via setdata().
    // The simulated size stays pSize - the payload is the same bytes the
    // size already counts, not an addition to them.
    if (trec_.payload)
    {
        PacketData *data = new PacketData(1 + strlen(trec_.payload));
        strcpy((char *)data->data(), trec_.payload);
        agent_->sendmsg(trec_.pSize, data);
    }
    else
    {
        agent_->sendmsg(trec_.pSize);
    }

    // The record just sent was fetched by the previous call; a_ says it was
    // the last of the trace. Tested after the send, not before: tested
    // before, the last packet of every trace - fetched, flagged, never sent
    // - was missing from every simulation since 15 August 2026, one packet
    // short of what st-packet.dat and the metrics counted as sent (found by
    // the test bench, 14 September 2026).
    if (tfile_->get_a() == 1)
    {
        running_ = 0;

        return;
    }

    nextPkttime_ = next_interval(trec_.pSize);

    timer_.resched(nextPkttime_);
}

double evalvsn::next_interval(int &size)
{
    tfile_->get_next(ndx_, trec_);

    size = trec_.pSize;

    return trec_.sTime;
}
