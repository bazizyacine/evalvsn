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
    // alone, each packet carrying the index of its first coefficient.
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
