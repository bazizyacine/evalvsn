#include "evalvsn.h"

vector<prioritiesInfo> frameEncoder(inputParameters IP, Mat_<double> encodedFrame, prioritiesInfo PI)
{
    Mat_<double> block, blockZigZag;
    vector<prioritiesInfo> vecPI;

    PI.blockNb = 0;

    for (int i = 0; i <= encodedFrame.rows - 1; i += 8)
    {
        for (int j = 0; j <= encodedFrame.cols - 1; j += 8)
        {
            PI.blockNb++;
            block = encodedFrame(Range(i, i + 8), Range(j, j + 8));

            if (!(IP.encoder.compare("MMPEG")))
            {
                blockZigZag = zigzagEncoder(quantificationEncoder(IP, dctEncoder(IP, block)));
                vecPI = priorityEncoder(IP, blockZigZag, vecPI, PI);
            }
        }
    }

    return vecPI;
}

Mat_<double> frameDecoder(inputParameters IP, Mat_<double> decodedFrame)
{
    Mat_<double> blockQTF;

    for (int i = 0; i <= decodedFrame.rows - 1; i += 8)
    {
        for (int j = 0; j <= decodedFrame.cols - 1; j += 8)
        {
            blockQTF = dctDecoder(IP, quantificationDecoder(IP, decodedFrame(Range(i, i + 8), Range(j, j + 8))));
            blockQTF.copyTo(decodedFrame(Range(i, i + 8), Range(j, j + 8)));
        }
    }

    return decodedFrame;
}
