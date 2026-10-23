#include "evalvsn.h"

// The packets of a priority level (decision d53, 16 October 2026). Each
// packet starts on a code word and carries, in its first two bytes, the
// index of its first coefficient in the level: it decodes alone, and a lost
// packet costs the coefficients it carries and no others, as a slice of
// H.264 or a restart interval of JPEG. The former stream was cut every
// 74 bytes in the middle of the code words, and the decoder read the
// sender's whole stream to place what survived (finding X-27).

static const int kPacketHeaderBits = 16;

// A run of code words that must travel together: one coefficient outside
// run-length coding, one (run, value) pair inside it, which covers `coefs`
// coefficients of the level.
struct CodeGroup
{
    int firstCodeWord;
    int codeWords;
    long coefs;
};

vector<string> packetEncoder(inputParameters IP, prioritiesInfo PI, Mat_<double> elems, bool rle)
{
    vector<CodeGroup> groups;
    if (rle)
    {
        int cw = 0;
        for (int p = 0; p + 1 < elems.cols; p += 2)
        {
            long run = (long)elems.at<double>(p);
            // The static coder splits a run over 255 into pairs of 255 and
            // a remainder, two code words each (static.cc).
            if (!(IP.binLevel.compare("ST")) && run > 255)
            {
                for (long k = 0; k < run / 255; k++)
                {
                    groups.push_back({cw, 2, 255});
                    cw += 2;
                }
                groups.push_back({cw, 2, run % 255});
                cw += 2;
            }
            else
            {
                groups.push_back({cw, 2, run});
                cw += 2;
            }
        }
    }
    else
    {
        for (int j = 0; j < elems.cols; j++)
            groups.push_back({j, 1, 1});
    }

    int codeWords = groups.empty() ? 0 : groups.back().firstCodeWord + groups.back().codeWords;
    if (codeWords != (int)PI.levelSize.size())
    {
        cerr << "packetEncoder: " << codeWords << " code words grouped, " << PI.levelSize.size()
             << " coded (frame " << PI.frameNb << ", level " << PI.dataPriority << ")\n";
        exit(EXIT_FAILURE);
    }

    const long budget = (long)IP.mpl * 8 - kPacketHeaderBits;
    vector<string> units;
    long coef = 0;
    size_t g = 0;
    while (g < groups.size())
    {
        long start = coef;
        string bits;
        while (g < groups.size())
        {
            int first = groups[g].firstCodeWord;
            int last = first + groups[g].codeWords - 1;
            int a = PI.levelOffset[first];
            int b = PI.levelOffset[last] + PI.levelSize[last];
            if (!bits.empty() && (long)bits.size() + (b - a) > budget)
                break;
            if (bits.empty() && (b - a) > budget)
            {
                cerr << "packetEncoder: a code group of " << (b - a) << " bits exceeds the packet\n";
                exit(EXIT_FAILURE);
            }
            bits += PI.data.substr(a, b - a);
            coef += groups[g].coefs;
            g++;
        }
        if (start > 65535)
        {
            cerr << "packetEncoder: coefficient index " << start << " needs more than two bytes\n";
            exit(EXIT_FAILURE);
        }
        prioritiesInfo unit;
        // The padding is of ones: a run of ones never completes an
        // Exp-Golomb code word (a zero ends its prefix), and the static
        // code words are whole bytes.
        unit.data = dec2bin(start, kPacketHeaderBits) + bits;
        unit.data.append((8 - unit.data.size() % 8) % 8, '1');
        units.push_back(hexEncoder(unit).data);
    }
    return units;
}

// The coefficients of a level rebuilt from the packets that arrived; the
// coefficients of the lost packets stay at zero.
Mat_<double> packetDecoder(inputParameters IP, int dataPriority, const vector<string> &units)
{
    int rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));
    bool rle = !(IP.encoder.compare("MMPEG")) && dataPriority >= rleLevel;
    vector<double> coefs;
    for (const string &u : units)
    {
        prioritiesInfo P;
        P.dataPriority = dataPriority;
        P.data = u;
        P = hexDecoder(P);
        if ((int)P.data.size() < kPacketHeaderBits)
            continue;
        long idx = (long)bin2dec(P.data.substr(0, kPacketHeaderBits), kPacketHeaderBits);
        P.data = P.data.substr(kPacketHeaderBits);
        P = binaryDecoder(IP, P);
        if (rle)
        {
            for (int p = 0; p + 1 < P.level.cols; p += 2)
            {
                long run = (long)P.level.at<double>(p);
                double value = P.level.at<double>(p + 1);
                for (long k = 0; k < run; k++, idx++)
                {
                    if (idx >= (long)coefs.size())
                        coefs.resize(idx + 1, 0.0);
                    coefs[idx] = value;
                }
            }
        }
        else
        {
            for (int k = 0; k < P.level.cols; k++, idx++)
            {
                if (idx >= (long)coefs.size())
                    coefs.resize(idx + 1, 0.0);
                coefs[idx] = P.level.at<double>(k);
            }
        }
    }
    Mat_<double> level(1, (int)coefs.size());
    for (int k = 0; k < (int)coefs.size(); k++)
        level.at<double>(k) = coefs[k];
    return level;
}
