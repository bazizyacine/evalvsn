// pham.cc - Encoder=JPEG, the Pham witness (tranche 3b-2, R34/R27).
//
// The MMPEG chain is compared against an external codec that a real image
// sensor runs: C. Pham's ucam-II JPEG encoder, ported byte-faithfully under
// pham/ (see pham/PROOF.md for what proves the port), and its untouched
// reference decoder. Both are driven here as external programs, the way the
// chain already drives ffmpeg, so that the reference decoder stays the
// deposited code and the proof of interoperability keeps its meaning.
//
// The contract with the rest of the chain: one st-priority line per Pham
// packet, frameType M and dataPriority 1, the line's payload being the
// packet exactly as the encoder wrote it (the size token, the two offset
// bytes and the payload, in hexadecimal). The network packetiser then emits
// one packet per line as long as MSS + 2 <= the set's payload, and a Pham
// packet is lost when any network packet carrying it is lost. Every packet
// resets the MQ coder, so the decoder reconstructs the frame from the
// surviving packets alone and neighbour copy conceals the missing blocks.

#include "evalvsn.h"
#include <unistd.h>

// The directory of the two programs: next to this executable, ../pham/,
// unless EVALVSN_PHAM_DIR says otherwise.
string phamDir()
{
    const char *env = getenv("EVALVSN_PHAM_DIR");

    if (env && *env)
    {
        return string(env);
    }

    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);

    if (n <= 0)
    {
        return "pham";
    }

    buf[n] = '\0';
    string exe(buf);
    string binDir = exe.substr(0, exe.find_last_of('/'));

    return binDir + "/../pham";   // /proc/self/exe is absolute, so this is too
}

// An 8-bit palettised BMP with biClrUsed = 256, the only convention the
// reference reader accepts (a biClrUsed of 0 makes it read the palette as
// the first pixel rows). Rows are bottom-up and padded to four bytes.
void writeBmp8(const string &path, const Mat &gray)
{
    int w = gray.cols, h = gray.rows;
    int rowBytes = (w + 3) & ~3;
    uint32_t imageBytes = (uint32_t)rowBytes * h;
    uint32_t offBits = 14 + 40 + 256 * 4;
    uint32_t fileSize = offBits + imageBytes;

    ofstream out(path, ofstream::binary);

    auto le16 = [&](uint16_t v) { out.put(v & 0xFF); out.put((v >> 8) & 0xFF); };
    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out.put((v >> (8 * i)) & 0xFF); };

    out.put('B'); out.put('M');
    le32(fileSize); le16(0); le16(0); le32(offBits);
    le32(40); le32((uint32_t)w); le32((uint32_t)h); le16(1); le16(8); le32(0);
    le32(imageBytes); le32(2835); le32(2835); le32(256); le32(256);

    for (int i = 0; i < 256; i++)
    {
        out.put((char)i); out.put((char)i); out.put((char)i); out.put(0);
    }

    vector<char> row(rowBytes, 0);

    for (int y = h - 1; y >= 0; y--)
    {
        const uint8_t *src = gray.ptr<uint8_t>(y);
        memcpy(row.data(), src, w);
        out.write(row.data(), rowBytes);
    }

    out.close();
}

// Runs the ported encoder on one grey frame and returns its packets, each
// as the hexadecimal string of the bytes the encoder wrote for it: the
// 2-byte size token (packet size + 2, big-endian), the 2 offset bytes and
// the payload. frameBytes receives the encoded size of the frame.
vector<string> phamEncodeFrame(inputParameters IP, const Mat &gray, const string &workDir, int frameNb, int &frameBytes)
{
    vector<string> packets;
    string bmp = workDir + "/frame_" + to_string(frameNb) + ".bmp";
    string dat = workDir + "/frame_" + to_string(frameNb) + ".dat";

    writeBmp8(bmp, gray);

    string cmd = phamDir() + "/pham_encode -Q " + to_string(IP.qualityFactor) + " -mss " + to_string(IP.mss) + " " + bmp + " " + dat + " 2>/dev/null";

    frameBytes = 0;

    if (system(cmd.c_str()) != 0)
    {
        cerr << "pham_encode failed on " << bmp << '\n';
        return packets;
    }

    // The .dat is a space-separated hexadecimal stream: a 4-digit size
    // token, then that many 2-digit bytes (the first two being the block
    // offset), repeated per packet.
    ifstream in(dat);
    string tok;

    while (in >> tok)
    {
        if (tok.size() != 4)
        {
            cerr << "pham_encode: unexpected token " << tok << " in " << dat << '\n';
            break;
        }

        int n = stoi(tok, nullptr, 16);
        string hex = tok;

        for (int i = 0; i < n; i++)
        {
            if (!(in >> tok) || tok.size() != 2)
            {
                cerr << "pham_encode: truncated packet in " << dat << '\n';
                return packets;
            }

            hex += tok;
        }

        packets.push_back(hex);
        frameBytes += n + 2;
    }

    return packets;
}

// Appends the frame's packets to st-priority.dat and its line to
// st-frame.dat, in the columns traceEncoder writes for MMPEG.
int phamTraceEncoder(inputParameters IP, const vector<string> &packets, double sendTime, int frameNb, const string &frameType, int frameBytes, const string &priorityName, const string &frameName)
{
    ofstream priorityTrace(priorityName, ofstream::app);

    for (const string &p : packets)
    {
        priorityTrace << sendTime << '\t' << frameNb << '\t' << frameType << '\t' << 1 << '\t' << (p.size() / 2) << '\t' << p << '\n';
    }

    priorityTrace.close();

    ofstream frameTrace(frameName, ofstream::app);
    frameTrace << frameNb << '\t' << frameType << '\t' << frameBytes << '\n';
    frameTrace.close();

    return EXIT_SUCCESS;
}

// A Pham packet as the packetiser saw it: the st-priority line it came
// from and the network packets that carried it.
struct phamPacket
{
    int frameNb;
    string hex;
    vector<int> seqNbs;
};

// Rebuilds each frame's .dat from the packets that survived the network
// and runs the reference decoder on it; the reference pass (no rcvFile)
// keeps every packet. The decoded frames go to referenceImages/ or
// sim/decodedImages/ as frame_<n>.png, as the MMPEG decoder writes them.
int phamVideoDecoder(inputParameters IP, const string &stPath, const string &rtPath, const string &priorityName, const string &stPacketName, const vector<packetsInfo> &vecPK, int frames, int height, int width)
{
    vector<phamPacket> packets;
    ifstream priorityTrace(priorityName);
    string line;

    getline(priorityTrace, line, '\n');

    while (getline(priorityTrace, line, '\n'))
    {
        stringstream ls(line);
        phamPacket p;
        double sendTime;
        string frameType, hex;
        int prio, size;

        ls >> sendTime >> p.frameNb >> frameType >> prio >> size >> p.hex;
        packets.push_back(p);
    }

    priorityTrace.close();

    // The network packets, in emission order: a new Pham packet starts
    // at every offset 0, which is how st-packetGenerate.sh cut the lines.
    if (!IP.rcvFile.empty())
    {
        ifstream packetTrace(stPacketName);
        size_t k = 0;
        bool first = true;

        while (getline(packetTrace, line, '\n'))
        {
            if (line.empty() || line[0] == '#' || line[0] == ' ')
            {
                continue;
            }

            stringstream ls(line);
            packetsInfo PK;
            ls >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            if (PK.offset == 0 && !first)
            {
                k++;
            }

            first = false;

            if (k < packets.size())
            {
                packets[k].seqNbs.push_back(PK.seqNb);
            }
        }

        packetTrace.close();

        if (k + 1 != packets.size())
        {
            cerr << "pham: " << packets.size() << " packet line(s) in st-priority.dat but " << (k + 1) << " group(s) in st-packet.dat\n";
            return EXIT_FAILURE;
        }
    }

    set<int> lost;

    for (const packetsInfo &PK : vecPK)
    {
        lost.insert(PK.seqNb);
    }

    string imagePath = IP.rcvFile.empty() ? stPath + "/referenceImages" : stPath + '/' + rtPath + "/decodedImages";
    string workDir = stPath + "/pham";
    mkDir(workDir);

    Mat currentFrame = Mat::zeros(height, width, CV_8UC1);

    for (int f = 1; f <= frames; f++)
    {
        string dat = workDir + "/rx_" + to_string(f) + ".dat";
        ofstream out(dat);
        int kept = 0;

        for (const phamPacket &p : packets)
        {
            if (p.frameNb != f)
            {
                continue;
            }

            bool alive = true;

            for (int seq : p.seqNbs)
            {
                if (lost.count(seq))
                {
                    alive = false;
                    break;
                }
            }

            if (!alive)
            {
                continue;
            }

            // Back to the encoder's stream: size token, then the bytes.
            out << p.hex.substr(0, 4);

            for (size_t i = 4; i + 1 < p.hex.size(); i += 2)
            {
                out << ' ' << p.hex.substr(i, 2);
            }

            out << ' ';
            kept++;
        }

        out.close();

        bool decoded = false;

        if (kept > 0)
        {
            // The reference decoder wants the original BMP for its geometry
            // only (it zeroes the pixels), writes its output in the current
            // directory and prints the file name.
            // Run inside the work directory, where the decoder drops its
            // output; the paths it receives are therefore relative to it.
            string cmd = "cd " + workDir + " && " + phamDir() + "/decode_to_bmp -received rx_" + to_string(f) + ".dat -SN " + to_string(f) + " -src 0 -camid 0 -Q " + to_string(IP.qualityFactor) + " frame_" + to_string(f) + ".bmp 2>/dev/null";
            FILE *pipe = popen(cmd.c_str(), "r");
            char buf[512];
            string outName;

            if (pipe)
            {
                if (fgets(buf, sizeof(buf), pipe))
                {
                    outName = buf;
                    outName.erase(outName.find_last_not_of(" \n\r") + 1);
                }

                pclose(pipe);
            }

            if (!outName.empty() && outName != "error")
            {
                Mat img = imread(workDir + '/' + outName, IMREAD_GRAYSCALE);

                if (!img.empty())
                {
                    currentFrame = img;
                    decoded = true;
                }

                remove((workDir + '/' + outName).c_str());
            }
        }

        // A frame with nothing decodable keeps the previous one on screen,
        // as the MMPEG decoder does when a frame is entirely lost.
        (void)decoded;

        imwrite(imagePath + "/frame_" + to_string(f) + ".png", currentFrame);
    }

    return EXIT_SUCCESS;
}
