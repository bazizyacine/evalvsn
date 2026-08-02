#include "evalvsn.h"

Mat_<double> quantificationEncoder(inputParameters IP, Mat_<double> blockDCT)
{
    Mat_<double> blockQTF = Mat::zeros(blockDCT.rows, blockDCT.cols, CV_64F), Z = (Mat_<double>(8, 8) << 16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99);

    for (int i = 0; i <= blockDCT.rows - 1; i++)
    {
        for (int j = 0; j <= blockDCT.cols - 1; j++)
        {
            blockQTF.at<double>(i, j) = round(blockDCT.at<double>(i, j) / (Z.at<double>(i, j) * IP.qualityCoef));
        }
    }

    return blockQTF;
}

Mat_<double> quantificationDecoder(inputParameters IP, Mat_<double> blockQTF)
{
    Mat_<double> blockDCT = Mat::zeros(blockQTF.rows, blockQTF.cols, CV_64F), Z = (Mat_<double>(8, 8) << 16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99);

    for (int i = 0; i <= blockQTF.rows - 1; i++)
    {
        for (int j = 0; j <= blockQTF.cols - 1; j++)
        {
            blockDCT.at<double>(i, j) = blockQTF.at<double>(i, j) * Z.at<double>(i, j) * IP.qualityCoef;
        }
    }

    return blockDCT;
}
