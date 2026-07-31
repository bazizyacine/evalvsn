#include "evalvsn.h"

bool isValidSimId(const string &simId)
{
    return regex_match(simId, regex(R"(^(?!\.{1,2}$)[^\\/:*?"<>|][^\\/:*?"<>|]*[^\\/:*?"<>|\. ]$)"));
}

bool isValidResolution(const string &res)
{
    return regex_match(res, regex("^[0-9]+x[0-9]+$"));
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

bool isValidRleLevel(const string &rle)
{
    return regex_match(rle, regex("^RLE([1-9]|1[0-3])$"));
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

bool isValidFile(const string &f)
{
    return f.empty() || regex_match(f, regex(R"(^(?!\.{1,2}$)[^\\/:*?"<>|][^\\/:*?"<>|]*[^\\/:*?"<>|\. ]$)"));
}

bool getIP(inputParameters &IP, const string &line, string &errParam)
{
    stringstream lineStream(line);

    string imagesNbStr, videoDurationStr, FPSStr, thresholdStr, gopCoefStr, qualityCoefStr, priorityNbStr, zoneSizeStr;

    lineStream >> IP.simId >> IP.encoder;

    if (!(IP.encoder.compare("MMPEG")))
    {
        lineStream >> IP.imageResolution >> imagesNbStr >> videoDurationStr >> FPSStr >> thresholdStr >> gopCoefStr >> qualityCoefStr >> priorityNbStr >> IP.DCT >> zoneSizeStr >> IP.rleLevel >> IP.binLevel >> IP.rcvFile;

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
        if (!isValidResolution(IP.imageResolution))
        {
            errParam = "imageResolution";
            return false;
        }
        if (!isValidInt(imagesNbStr))
        {
            errParam = "imagesNb";
            return false;
        }
        if (!isValidInt(videoDurationStr))
        {
            errParam = "videoDuration";
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
        if (!isValidFile(IP.rcvFile))
        {
            errParam = "rcvFile";
            return false;
        }

        IP.imagesNb = stoi(imagesNbStr);
        IP.videoDuration = stoi(videoDurationStr);
        IP.FPS = stod(FPSStr);
        IP.threshold = stoi(thresholdStr);
        IP.gopCoef = stod(gopCoefStr);
        IP.qualityCoef = stod(qualityCoefStr);
        IP.priorityNb = stoi(priorityNbStr);
        IP.zoneSize = stoi(zoneSizeStr);
    }
    else if (!(IP.encoder.compare("SC")))
    {
        lineStream >> IP.imageResolution >> imagesNbStr >> videoDurationStr >> FPSStr >> thresholdStr >> priorityNbStr >> IP.binLevel >> IP.rcvFile;

        if (!isValidSimId(IP.simId))
        {
            errParam = "simId";
            return false;
        }
        if (IP.encoder != "SC")
        {
            errParam = "Encoder";
            return false;
        }
        if (!isValidResolution(IP.imageResolution))
        {
            errParam = "imageResolution";
            return false;
        }
        if (!isValidInt(imagesNbStr))
        {
            errParam = "imagesNb";
            return false;
        }
        if (!isValidInt(videoDurationStr))
        {
            errParam = "videoDuration";
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
        if (!isValidPriorityNb(priorityNbStr))
        {
            errParam = "priorityNb";
            return false;
        }
        if (!isValidBinLevel(IP.binLevel))
        {
            errParam = "binLevel";
            return false;
        }
        if (!isValidFile(IP.rcvFile))
        {
            errParam = "rcvFile";
            return false;
        }

        IP.imagesNb = stoi(imagesNbStr);
        IP.videoDuration = stoi(videoDurationStr);
        IP.FPS = stod(FPSStr);
        IP.threshold = stoi(thresholdStr);
        IP.priorityNb = stoi(priorityNbStr);
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

        levelZeros = levelZerosEvaluate(PI.level);

        if (levelZeros != PI.level.cols)
        {
            if (!(IP.encoder.compare("MMPEG")))
            {
                rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

                if (PI.dataPriority >= rleLevel)
                {
                    PI = rleEncoder(PI);
                }
            }

            PI = binaryEncoder(IP, PI);

            if (!(IP.binLevel.compare("EG")))
            {
                PI = digitEncoder(PI);
            }

            PI = hexEncoder(PI);

            PI.dataSize = ceil(((int)PI.data.length()) / 2);

            FI.frameSize += PI.dataSize;

            if (PI.dataPriority)
            {
                priorityTrace << PI.sendTime << '\t' << PI.frameNb << '\t' << PI.frameType << '\t' << PI.dataPriority << '\t' << PI.dataSize << '\t' << PI.data << '\n';
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

vector<prioritiesInfo> traceDecoder(inputParameters IP, vector<packetsInfo> vecPK, string priorityName)
{
    vector<prioritiesInfo> vecPI, levelsInfo;
    prioritiesInfo PI;
    ifstream priorityTrace;
    stringstream lineStream;
    string line;
    int rleLevel;

    priorityTrace.open(priorityName, ifstream::in);

    getline(priorityTrace, line, '\n');

    while (getline(priorityTrace, line, '\n'))
    {
        lineStream << line;

        lineStream >> PI.sendTime >> PI.frameNb >> PI.frameType >> PI.dataPriority >> PI.dataSize >> PI.data;

        vecPI.push_back(PI);

        lineStream.clear();
    }

    priorityTrace.close();

    for (int i = 0; i <= ((int)vecPI.size()) - 1; i++)
    {
        vecPI[i] = hexDecoder(vecPI[i]);

        if (!(IP.binLevel.compare("EG")))
        {
            vecPI[i] = digitDecoder(vecPI[i]);
        }

        vecPI[i] = binaryDecoder(IP, vecPI[i]);

        if (!(IP.rcvFile.empty()))
        {
            vecPI[i] = lostLevels(IP, vecPI[i], vecPK);
        }

        if (!(IP.encoder.compare("MMPEG")))
        {
            rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

            if (vecPI[i].dataPriority >= rleLevel)
            {
                vecPI[i] = rleDecoder(vecPI[i]);
            }

            levelsInfo = priorityDecoder(IP, levelsInfo, vecPI[i]);
        }
        else if (!(IP.encoder.compare("SC")))
        {
            PI.blockNb = 0;

            for (int j = 0; j < vecPI[i].level.cols - 1; j += 64)
            {
                PI.blockNb++;

                PI.level = vecPI[i].level(Range::all(), Range(j, j + 64));

                levelsInfo.push_back({vecPI[i].sendTime, vecPI[i].frameNb, vecPI[i].frameType, vecPI[i].dataPriority, vecPI[i].dataSize, vecPI[i].data, PI.blockNb, PI.level, vecPI[i].levelSize, vecPI[i].levelOffset});
            }
        }
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
            lineStream << line;

            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            stVecPK.push_back(PK);

            lineStream.clear();
        }
    }

    packetTrace.close();

    packetTrace.open(rtPacketName, ifstream::in);

    while (getline(packetTrace, line, '\n'))
    {
        if (line[0] != '#' && line[0] != ' ' && !(line.empty()) && (count(line.begin(), line.end(), '\t') + 1) == 7)
        {
            lineStream << line;

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

prioritiesInfo lostLevels(inputParameters IP, prioritiesInfo PI, vector<packetsInfo> vecPK)
{
    int rleLevel;

    for (int i = 0; i <= ((int)vecPK.size()) - 1; i++)
    {
        if (PI.frameNb == vecPK[i].frameNb && PI.dataPriority == vecPK[i].prioNb)
        {
            for (int j = 0; j <= ((int)PI.levelSize.size()) - 1; j++)
            {
                if (!(IP.encoder.compare("MMPEG")))
                {
                    rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

                    if (PI.dataPriority >= rleLevel)
                    {
                        if (j % 2 != 0)
                        {
                            if ((PI.levelOffset[j] + PI.levelSize[j]) <= (8 * (vecPK[i].offset + vecPK[i].pSize)))
                            {
                                PI.level.at<double>(j) = 0;
                            }
                        }
                    }
                    else if (PI.dataPriority < rleLevel)
                    {
                        if ((PI.levelOffset[j] + PI.levelSize[j]) <= (8 * (vecPK[i].offset + vecPK[i].pSize)))
                        {
                            PI.level.at<double>(j) = 0;
                        }
                    }
                }
                else if (!(IP.encoder.compare("SC")))
                {
                    if ((PI.levelOffset[j] + PI.levelSize[j]) <= (8 * (vecPK[i].offset + vecPK[i].pSize)))
                    {
                        PI.level.at<double>(j) = 0;
                    }
                }
            }
        }
    }

    return PI;
}
