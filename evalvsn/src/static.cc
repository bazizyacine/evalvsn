#include "evalvsn.h"

prioritiesInfo staticEncoder(inputParameters IP, prioritiesInfo PI)
{
    double r;
    int occurrenceBits, pixelBits, rleLevel, q, levelSize, levelOffset;

    levelOffset = 0;

    if (!(IP.encoder.compare("MMPEG")))
    {
        if (!(IP.DCT.compare("CLA")))
        {
            if (IP.qualityCoef >= 1)
            {
                occurrenceBits = 8;
                pixelBits = 8;
            }
            else if (IP.qualityCoef < 1)
            {
                occurrenceBits = 8;
                pixelBits = 16;
            }
        }
        else if (!(IP.DCT.compare("LLM")) || !(IP.DCT.compare("BIN")))
        {
            if (IP.qualityCoef >= 8)
            {
                occurrenceBits = 8;
                pixelBits = 8;
            }
            else if (IP.qualityCoef < 8)
            {
                occurrenceBits = 8;
                pixelBits = 16;
            }
        }
    }

    if (!(IP.encoder.compare("MMPEG")))
    {
        rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

        if (PI.dataPriority >= rleLevel)
        {
            for (int i = 0; i < PI.level.cols - 1; i += 2)
            {
                if (PI.level.at<double>(i) > 255)
                {
                    q = PI.level.at<double>(i) / 255;

                    r = ((int)PI.level.at<double>(i)) % 255;

                    for (int j = 1; j <= q; j++)
                    {
                        PI.data += dec2bin(255, occurrenceBits);
                        PI.data += (PI.level.at<double>(i + 1) >= 0) ? "0" : "1";
                        PI.data += dec2bin(PI.level.at<double>(i + 1), pixelBits - 1);
                    }

                    PI.data += dec2bin(r, occurrenceBits);
                    PI.data += (PI.level.at<double>(i + 1) >= 0) ? "0" : "1";
                    PI.data += dec2bin(PI.level.at<double>(i + 1), pixelBits - 1);

                    for (int j = 1; j <= q + 1; j++)
                    {
                        levelSize = occurrenceBits;
                        PI.levelSize.push_back(levelSize);
                        PI.levelOffset.push_back(levelOffset);
                        levelOffset += levelSize;
                        levelSize = pixelBits;
                        PI.levelSize.push_back(levelSize);
                        PI.levelOffset.push_back(levelOffset);
                        levelOffset += levelSize;
                    }
                }
                else if (PI.level.at<double>(i) <= 255)
                {
                    PI.data += dec2bin(PI.level.at<double>(i), occurrenceBits);
                    PI.data += (PI.level.at<double>(i + 1) >= 0) ? "0" : "1";
                    PI.data += dec2bin(PI.level.at<double>(i + 1), pixelBits - 1);

                    levelSize = occurrenceBits;
                    PI.levelSize.push_back(levelSize);
                    PI.levelOffset.push_back(levelOffset);
                    levelOffset += levelSize;
                    levelSize = pixelBits;
                    PI.levelSize.push_back(levelSize);
                    PI.levelOffset.push_back(levelOffset);
                    levelOffset += levelSize;
                }
            }
        }
        else if (PI.dataPriority < rleLevel)
        {
            levelSize = pixelBits;

            for (int i = 0; i <= PI.level.cols - 1; i++)
            {
                PI.data += (PI.level.at<double>(i) >= 0) ? "0" : "1";
                PI.data += dec2bin(PI.level.at<double>(i), pixelBits - 1);

                PI.levelSize.push_back(levelSize);
                PI.levelOffset.push_back(levelOffset);
                levelOffset += levelSize;
            }
        }
    }

    return PI;
}

prioritiesInfo staticDecoder(inputParameters IP, prioritiesInfo PI)
{
    int occurrenceBits, pixelBits, rleLevel, levelSize, levelOffset;

    levelOffset = 0;

    if (!(IP.encoder.compare("MMPEG")))
    {
        if (!(IP.DCT.compare("CLA")))
        {
            if (IP.qualityCoef >= 1)
            {
                occurrenceBits = 8;
                pixelBits = 8;
            }
            else if (IP.qualityCoef < 1)
            {
                occurrenceBits = 8;
                pixelBits = 16;
            }
        }
        else if (!(IP.DCT.compare("LLM")) || !(IP.DCT.compare("BIN")))
        {
            if (IP.qualityCoef >= 8)
            {
                occurrenceBits = 8;
                pixelBits = 8;
            }
            else if (IP.qualityCoef < 8)
            {
                occurrenceBits = 8;
                pixelBits = 16;
            }
        }
    }

    if (!(IP.encoder.compare("MMPEG")))
    {
        rleLevel = stoi(IP.rleLevel.substr(3, ((int)IP.rleLevel.length()) - 3));

        if (PI.dataPriority >= rleLevel)
        {
            for (int i = 0; i < ((int)PI.data.length()); i += occurrenceBits + pixelBits)
            {
                PI.level.push_back(bin2dec(PI.data.substr(i, occurrenceBits), occurrenceBits));

                if (PI.data[i + occurrenceBits] == '0')
                {
                    PI.level.push_back(bin2dec(PI.data.substr(i + occurrenceBits + 1, pixelBits - 1), pixelBits - 1));
                }
                else if (PI.data[i + occurrenceBits] == '1')
                {
                    PI.level.push_back(-(bin2dec(PI.data.substr(i + occurrenceBits + 1, pixelBits - 1), pixelBits - 1)));
                }

                levelSize = occurrenceBits;
                PI.levelSize.push_back(levelSize);
                PI.levelOffset.push_back(levelOffset);
                levelOffset += levelSize;
                levelSize = pixelBits;
                PI.levelSize.push_back(levelSize);
                PI.levelOffset.push_back(levelOffset);
                levelOffset += levelSize;
            }
        }
        else if (PI.dataPriority < rleLevel)
        {
            levelSize = pixelBits;

            for (int i = 0; i < ((int)PI.data.length()); i += pixelBits)
            {
                if (PI.data[i] == '0')
                {
                    PI.level.push_back(bin2dec(PI.data.substr(i + 1, pixelBits - 1), pixelBits - 1));
                }
                else if (PI.data[i] == '1')
                {
                    PI.level.push_back(-(bin2dec(PI.data.substr(i + 1, pixelBits - 1), pixelBits - 1)));
                }

                PI.levelSize.push_back(levelSize);
                PI.levelOffset.push_back(levelOffset);
                levelOffset += levelSize;
            }
        }
    }

    PI.level = PI.level.t();

    return PI;
}
