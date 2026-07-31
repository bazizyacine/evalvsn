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

    if (ndx++ == nrec_ - 1)
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
    int seqNb, pSize, frameNb, prioNb, offset, i;

    nrec_ = 0;

    if (!status_)
    {
        status_ = 1;

        if ((pFile = fopen(name_, "r")) == NULL)
        {
            printf("can't open file %s\n", name_);

            return -1;
        }

        fseek(pFile, 51, SEEK_SET);

        while (!feof(pFile))
        {
            nrec_++;

            fscanf(pFile, "%lf %i %i %i %s %i %i", &sTime, &seqNb, &pSize, &frameNb, frameType, &prioNb, &offset);
        }

        printf("Total packets = %i\n", nrec_ - 1);

        rewind(pFile);

        fseek(pFile, 51, SEEK_SET);

        trace_ = new packetsInfo[nrec_ - 1];

        for (i = 0, t = trace_; i < nrec_ - 1; i++, t++)
        {
            fscanf(pFile, "%lf %i %i %i %s %i %i", &sTime, &seqNb, &pSize, &frameNb, frameType, &prioNb, &offset);

            t->sTime = sTime - sTimeLast;

            t->seqNb = seqNb;

            t->pSize = pSize;

            t->frameNb = frameNb;

            strcpy(t->frameType, frameType);

            t->prioNb = prioNb;

            t->offset = offset;

            sTimeLast = sTime;
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
            tfile_ = (evalvsnFile *)TclObject::lookup(argv[2]);

            if (tfile_ == 0)
            {
                tcl.resultf("no such node %s", argv[2]);

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

    if (tfile_->get_a() == 1)
    {
        running_ = 0;

        return;
    }

    agent_->set_pkttype(PT_VIDEO);

    agent_->set_seqNb(trec_.seqNb);

    agent_->set_pSize(trec_.pSize);

    agent_->set_frameNb(trec_.frameNb);

    agent_->set_frameType(trec_.frameType);

    agent_->set_prioNb(trec_.prioNb);

    agent_->set_offset(trec_.offset);

    agent_->sendmsg(trec_.pSize);

    nextPkttime_ = next_interval(trec_.pSize);

    timer_.resched(nextPkttime_);
}

double evalvsn::next_interval(int &size)
{
    tfile_->get_next(ndx_, trec_);

    size = trec_.pSize;

    return trec_.sTime;
}
