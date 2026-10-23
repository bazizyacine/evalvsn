#include "evalvsn.h"

// A simId names a directory: no path separator or shell-hostile character,
// not "." or "..", not ending in a dot or a space - and one character is
// enough (the former form demanded two, the test bench's first finding).
bool isValidSimId(const string &simId)
{
    return regex_match(simId, regex(R"(^(?!\.{1,2}$)[^\\/:*?"<>|]*[^\\/:*?"<>|\. ]$)"));
}

bool isValidInt(const string &s)
{
    return regex_match(s, regex("^[0-9]+$"));
}

bool isValidDouble(const string &s)
{
    return regex_match(s, regex("^[0-9]*\\.?[0-9]+$"));
}

bool isValidDCT(const string &dct)
{
    return dct == "CLA" || dct == "LLM" || dct == "BIN";
}

bool isValidZoneSize(const string &zs)
{
    int z = stoi(zs);
    return z >= 2 && z <= 8;
}

// RLEk applies the run-length coding from level k on; RLE1 codes every
// level and RLE14, one past the thirteenth, none of them - the end point of
// the RLE axis of the comparative study (14 September 2026).
bool isValidRleLevel(const string &rle)
{
    return regex_match(rle, regex("^RLE([1-9]|1[0-4])$"));
}

bool isValidBinLevel(const string &bl)
{
    return bl == "EG" || bl == "ST";
}

bool isValidPriorityNb(const string &pn)
{
    int p = stoi(pn);
    return p >= 1 && p <= 13;
}

bool getIP(inputParameters &IP, const string &line, string &errParam)
{
    stringstream lineStream(line);

    string FPSStr, thresholdStr, gopCoefStr, qualityCoefStr, priorityNbStr, zoneSizeStr, claStr;

    lineStream >> IP.simId >> IP.encoder;

    if (!(IP.encoder.compare("MMPEG")))
    {
        lineStream >> FPSStr >> thresholdStr >> gopCoefStr >> qualityCoefStr >> priorityNbStr >> claStr >> IP.rleLevel >> IP.binLevel;

        // DCT and zoneSize travel as one field - CLA8, the notation the
        // README already used (R32): the alphabetic prefix names the
        // transform, the digits give the zone size. rcvFile is no longer a
        // column either: decoding is asked with -decode on the command line.
        size_t claDigit = claStr.find_first_of("0123456789");

        if (claDigit == string::npos || claDigit == 0)
        {
            errParam = "DCT+zoneSize";
            return false;
        }

        IP.DCT = claStr.substr(0, claDigit);
        zoneSizeStr = claStr.substr(claDigit);

        if (!isValidSimId(IP.simId))
        {
            errParam = "simId";
            return false;
        }
        if (IP.encoder != "MMPEG")
        {
            errParam = "Encoder";
            return false;
        }
        if (!isValidDouble(FPSStr))
        {
            errParam = "FPS";
            return false;
        }
        if (!isValidInt(thresholdStr))
        {
            errParam = "Threshold";
            return false;
        }
        if (!isValidDouble(gopCoefStr))
        {
            errParam = "gopCoef";
            return false;
        }
        if (!isValidDouble(qualityCoefStr))
        {
            errParam = "qualityCoef";
            return false;
        }
        if (!isValidPriorityNb(priorityNbStr))
        {
            errParam = "priorityNb";
            return false;
        }
        if (!isValidDCT(IP.DCT))
        {
            errParam = "DCT";
            return false;
        }
        if (!isValidZoneSize(zoneSizeStr))
        {
            errParam = "zoneSize";
            return false;
        }
        // The zonal transforms keep the triangle of the first zoneSize
        // diagonals of the block, one level per diagonal from the second on:
        // zoneSize - 1 levels at most (7 for LLM8 and BIN8), where CLA, the
        // full 8x8 DCT, carries 13. More levels than that were silently
        // capped; the encoder refuses them since the test bench of 14
        // September 2026 (the author's choice: the transforms are compared
        // at seven levels).
        if (IP.DCT.compare("CLA") != 0 && stoi(priorityNbStr) > stoi(zoneSizeStr) - 1)
        {
            errParam = "priorityNb (at most zoneSize - 1 levels for " + IP.DCT + ")";
            return false;
        }
        if (!isValidRleLevel(IP.rleLevel))
        {
            errParam = "rleLevel";
            return false;
        }
        if (!isValidBinLevel(IP.binLevel))
        {
            errParam = "binLevel";
            return false;
        }

        IP.FPS = stod(FPSStr);
        IP.threshold = stoi(thresholdStr);
        IP.gopCoef = stod(gopCoefStr);
        IP.qualityCoef = stod(qualityCoefStr);
        IP.priorityNb = stoi(priorityNbStr);
        IP.zoneSize = stoi(zoneSizeStr);
    }
    else if (!(IP.encoder.compare("JPEG")))
    {
        // The Pham witness: simId JPEG FPS Threshold QF MSS. QF is the
        // ucam-II quality factor (the reference deposit runs at 20), MSS the
        // packet size the encoder packetises to, in bytes, which the
        // network packetiser must not have to split: keep it at most the
        // set's payload minus two.
        string qfStr, mssStr;

        lineStream >> FPSStr >> thresholdStr >> qfStr >> mssStr;

        if (!isValidSimId(IP.simId))
        {
            errParam = "simId";
            return false;
        }
        if (!isValidDouble(FPSStr))
        {
            errParam = "FPS";
            return false;
        }
        if (!isValidInt(thresholdStr))
        {
            errParam = "Threshold";
            return false;
        }
        if (!isValidInt(qfStr) || stoi(qfStr) < 1 || stoi(qfStr) > 100)
        {
            errParam = "QF";
            return false;
        }
        if (!isValidInt(mssStr) || stoi(mssStr) < 16)
        {
            errParam = "MSS";
            return false;
        }

        IP.FPS = stod(FPSStr);
        IP.threshold = stoi(thresholdStr);
        IP.qualityFactor = stoi(qfStr);
        IP.mss = stoi(mssStr);
        // One priority level, every frame a still: the accounting of the
        // evaluation reads these as the MMPEG grammar does.
        IP.priorityNb = 1;
        IP.gopCoef = 0;
        IP.qualityCoef = 0;
        IP.DCT = "BIN";
        IP.zoneSize = 8;
        IP.rleLevel = "RLE1";
        IP.binLevel = "EG";
    }
    else
    {
        errParam = "Encoder";
        return false;
    }

    return true;
}

void mkDir(string pathName)
{
    stringstream pathStream(pathName);
    string token, dirName;

    while (getline(pathStream, token, '/'))
    {
        dirName += token + '/';
        mkdir(dirName.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
}

void traceEncoder(inputParameters IP, vector<prioritiesInfo> vecPI, vector<framesInfo> vecFI, string priorityName, string frameName)
{
    prioritiesInfo PI;
    framesInfo FI;
    ofstream priorityTrace, frameTrace;
    int levelZeros, rleLevel;

    priorityTrace.open(priorityName, ofstream::app);

    PI.dataSize = FI.frameSize = PI.dataPriority = 0;

    for (int i = 1; i <= IP.priorityNb; i++)
    {
        Mat_<double> levels;

        for (int j = 0; j <= ((int)vecPI.size()) - 1; j++)
        {
            if (vecPI[j].dataPriority == i)
            {
                PI.sendTime = vecPI[j].sendTime;
                PI.frameNb = vecPI[j].frameNb;
                PI.frameType = vecPI[j].frameType;
                PI.dataPriority = vecPI[j].dataPriority;
                PI.blockNb = vecPI[j].blockNb;

                if (levels.empty())
                {
                    levels = vecPI[j].level;
                }
                else if (!(levels.empty()))
                {
                    hconcat(levels, vecPI[j].level, levels);
                }
            }
        }

        PI.level = levels;
        bool rle = false;

        levelZeros = levelZerosEvaluate(PI.level);

        if (levelZeros != PI.level.cols)
        {
            if (!(IP.encoder.compare("MMPEG")))
            {
                rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

                if (PI.dataPriority >= rleLevel)
                {
                    PI = rleEncoder(PI);
                    rle = true;
                }
            }

            Mat_<double> elems = PI.level.clone();

            // The code-word tables are the level's own: PI serves every
            // level of the frame in turn.
            PI.levelSize.clear();
            PI.levelOffset.clear();
            PI.data.clear();
            PI = binaryEncoder(IP, PI);

            // One line per packet (d53): each packet of the level, cut on
            // the code words, its coefficient index in its first two bytes.
            // st-packetGenerate.sh then sends each line as one packet.
            for (const string &unit : packetEncoder(IP, PI, elems, rle))
            {
                PI.dataSize = ((int)unit.length()) / 2;
                FI.frameSize += PI.dataSize;
                if (PI.dataPriority)
                {
                    priorityTrace << PI.sendTime << '\t' << PI.frameNb << '\t' << PI.frameType << '\t' << PI.dataPriority << '\t' << PI.dataSize << '\t' << unit << '\n';
                }
            }

            PI.dataPriority = PI.dataSize = 0;

            PI.data.clear();
        }
    }

    priorityTrace.close();

    frameTrace.open(frameName, ofstream::app);

    for (int i = 0; i <= ((int)vecFI.size()) - 1; i++)
    {
        frameTrace << vecFI[i].frameNb << '\t' << vecFI[i].frameType << '\t' << FI.frameSize << '\n';
    }

    frameTrace.close();
}

vector<prioritiesInfo> traceDecoder(inputParameters IP, string unitsName, bool received)
{
    // The packets of each frame and level: every packet of st-priority.dat
    // for the reference decoding, the payloads that reached the sink (the
    // deduplicated reception trace) for the decoding after the network. The
    // decoder reads nothing else of the sender (d53, finding X-27).
    map<pair<int, int>, vector<string>> units;
    map<pair<int, int>, prioritiesInfo> heads;
    ifstream trace(unitsName, ifstream::in);
    stringstream lineStream;
    string line;
    while (getline(trace, line, '\n'))
    {
        if (line.empty() || line[0] == '#')
            continue;
        prioritiesInfo PI;
        string data;
        lineStream.str(line);
        lineStream.clear();
        if (received)
        {
            packetsInfo PK;
            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset >> data;
            PI.sendTime = PK.sTime;
            PI.frameNb = PK.frameNb;
            PI.frameType = PK.frameType;
            PI.dataPriority = PK.prioNb;
        }
        else
        {
            lineStream >> PI.sendTime >> PI.frameNb >> PI.frameType >> PI.dataPriority >> PI.dataSize >> data;
        }
        if (lineStream.fail() || data.empty() || data == "-" || PI.dataPriority < 1)
            continue;
        pair<int, int> key(PI.frameNb, PI.dataPriority);
        if (heads.find(key) == heads.end())
            heads[key] = PI;
        units[key].push_back(data);
    }
    trace.close();

    vector<prioritiesInfo> levelsInfo;
    for (auto &h : heads)
    {
        prioritiesInfo PI = h.second;
        PI.level = packetDecoder(IP, PI.dataPriority, units[h.first]);
        levelsInfo = priorityDecoder(IP, levelsInfo, PI);
    }
    return levelsInfo;
}

vector<packetsInfo> lostPackets(inputParameters IP, string stPacketName, string rtPacketName)
{
    vector<packetsInfo> stVecPK, rtVecPK, vecPK;
    packetsInfo PK;
    ifstream packetTrace;
    stringstream lineStream;
    string line;
    bool lost = true;

    packetTrace.open(stPacketName, ifstream::in);

    while (getline(packetTrace, line, '\n'))
    {
        if (line[0] != '#' && line[0] != ' ' && !(line.empty()))
        {
            lineStream.str(line);
            lineStream.clear();

            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            stVecPK.push_back(PK);

            lineStream.clear();
        }
    }

    packetTrace.close();

    packetTrace.open(rtPacketName, ifstream::in);

    while (getline(packetTrace, line, '\n'))
    {
        // Exactly eight fields: one trace format, the author ruled, the
        // payload always last.
        if (line[0] != '#' && line[0] != ' ' && !(line.empty()) && (count(line.begin(), line.end(), '\t') + 1) == 8)
        {
            lineStream.str(line);
            lineStream.clear();

            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            rtVecPK.push_back(PK);

            lineStream.clear();
        }
    }

    packetTrace.close();

    for (int i = 0; i <= ((int)stVecPK.size()) - 1; i++)
    {
        for (int j = 0; j <= ((int)rtVecPK.size()) - 1; j++)
        {
            if (stVecPK[i].seqNb == rtVecPK[j].seqNb)
            {
                lost = false;
                break;
            }
            else if (stVecPK[i].seqNb != rtVecPK[j].seqNb)
            {
                lost = true;
            }
        }

        if (lost)
        {
            vecPK.push_back({stVecPK[i].sTime, stVecPK[i].seqNb, stVecPK[i].pSize, stVecPK[i].frameNb, stVecPK[i].frameType, stVecPK[i].prioNb, stVecPK[i].offset});
        }
    }

    return vecPK;
}
