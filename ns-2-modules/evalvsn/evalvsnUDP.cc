// This file derives from ns-2.35's apps/udp.cc, the UDP agent, and keeps its
// license; the original notice follows.
/*
 * Copyright (C) Xerox Corporation 1997. All rights reserved.
 *  
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linking this file statically or dynamically with other modules is making
 * a combined work based on this file.  Thus, the terms and conditions of
 * the GNU General Public License cover the whole combination.
 *
 * In addition, as a special exception, the copyright holders of this file
 * give you permission to combine this file with free software programs or
 * libraries that are released under the GNU LGPL and with code included in
 * the standard release of ns-2 under the Apache 2.0 license or under
 * otherwise-compatible licenses with advertising requirements (or modified
 * versions of such code, with unchanged license).  You may copy and
 * distribute such a system following the terms of the GNU GPL for this
 * file and the licenses of the other code concerned, provided that you
 * include the source code of that other code when and as the GNU GPL
 * requires distribution of source code.
 *
 * Note that people who make modified versions of this file are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so.  The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception.
 */

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
