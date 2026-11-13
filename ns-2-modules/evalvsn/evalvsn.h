// This file derives from ns-2.35's trace/traffictrace.cc, the trace-driven
// traffic generator, and keeps its license; the original notice follows.
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
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
