#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "config.h"
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include "random.h"
#include "object.h"
#include "trafgen.h"

struct packetsInfo
{
    double sTime;
    int seqNb;
    int pSize;
    int frameNb;
    char frameType[10];
    int prioNb;
    int offset;
    // The hexadecimal payload of the eighth trace column, or null when the
    // trace is in the seven-column format that predates it. Owned by the
    // evalvsnFile that parsed it; records passed around by get_next() share
    // the same storage.
    char *payload;
};

class evalvsnFile : public NsObject
{
public:
    evalvsnFile();
    void get_next(int &, packetsInfo &);
    int setup();
    int command(int argc, const char *const *argv);
    int get_a();

private:
    void recv(Packet *, Handler *);
    int status_;
    char *name_;
    int nrec_;
    packetsInfo *trace_;
    int a_;
};

class evalvsn : public TrafficGenerator
{
public:
    evalvsn();
    int command(int argc, const char *const *argv);
    virtual double next_interval(int &size);

protected:
    void timeout();
    evalvsnFile *tfile_;
    packetsInfo trec_;
    int ndx_;
    void init();
};
