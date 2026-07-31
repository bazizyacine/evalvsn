#include "evalvsn.h"

Mat_<double> dctEncoder(inputParameters IP, Mat_<double> block)
{
    Mat_<double> blockDCT;

    if (!(IP.DCT.compare("CLA")))
    {
        blockDCT = claEncoder(block);
    }
    else if (!(IP.DCT.compare("LLM")))
    {
        blockDCT = llmEncoder(IP, block);
    }
    else if (!(IP.DCT.compare("BIN")))
    {
        blockDCT = binEncoder(IP, block);
    }

    return blockDCT;
}

Mat_<double> claEncoder(Mat_<double> block)
{
    Mat_<double> blockDCT = Mat::zeros(block.rows, block.cols, CV_64F);
    double Cx, Cy, SUMij;

    for (int x = 0; x <= block.rows - 1; x++)
    {
        for (int y = 0; y <= block.cols - 1; y++)
        {
            SUMij = 0;
            for (int i = 0; i <= block.rows - 1; i++)
            {
                for (int j = 0; j <= block.cols - 1; j++)
                {
                    SUMij = SUMij + block.at<double>(i, j) * cos(((2.0 * j + 1.0) * y * CV_PI) / (2.0 * block.cols)) * cos(((2.0 * i + 1.0) * x * CV_PI) / (2.0 * block.rows));
                }
            }
            if (x == 0 && y == 0)
            {
                Cx = sqrt(1.0 / block.rows);
                Cy = sqrt(1.0 / block.cols);
            }
            else if (x > 0 && y == 0)
            {
                Cx = sqrt(2.0 / block.rows);
                Cy = sqrt(1.0 / block.cols);
            }
            else if (x == 0 && y > 0)
            {
                Cx = sqrt(1.0 / block.rows);
                Cy = sqrt(2.0 / block.cols);
            }
            else if (x > 0 && y > 0)
            {
                Cx = sqrt(2.0 / block.rows);
                Cy = sqrt(2.0 / block.cols);
            }
            blockDCT.at<double>(x, y) = Cx * Cy * SUMij;
        }
    }

    return blockDCT;
}

Mat_<double> llmEncoder(inputParameters IP, Mat_<double> block)
{
    Mat_<double> blockDCT = Mat::zeros(IP.zoneSize, IP.zoneSize, CV_64F);
    double tmp, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16, tmp17, tmp18, tmp19, C0 = 0.541196100146197, C1 = 0.765366864730179, C2 = 1.847759065022574, C3 = 0.831469612302545, C4 = -0.275899379282943, C5 = 1.387039845322148, C6 = 0.980785280403230, C7 = -0.785694958387102, C8 = 1.175875602419359, C9 = 1.414213562373095, C10 = 1.306562964876377;

    for (int i = 0; i <= block.rows - 1; i++)
    {
        if (IP.zoneSize == 2)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp10 = tmp2 + tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
        }
        else if (IP.zoneSize == 3)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 2) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
        }
        else if (IP.zoneSize == 4)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 2) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
            block.at<double>(i, 3) = C9 * tmp17;
        }
        else if (IP.zoneSize == 5)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 4) = tmp8 - tmp10;
            block.at<double>(i, 2) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
            block.at<double>(i, 3) = C9 * tmp17;
        }
        else if (IP.zoneSize == 6)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 4) = tmp8 - tmp10;
            block.at<double>(i, 2) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
            block.at<double>(i, 3) = C9 * tmp17;
            block.at<double>(i, 5) = C9 * tmp18;
        }
        else if (IP.zoneSize == 7)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 4) = tmp8 - tmp10;
            tmp = C0 * ((int)tmp9 + tmp11);
            block.at<double>(i, 2) = tmp + (C1 * tmp9);
            block.at<double>(i, 6) = tmp - (C2 * tmp11);
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 1) = tmp16 + tmp19;
            block.at<double>(i, 3) = C9 * tmp17;
            block.at<double>(i, 5) = C9 * tmp18;
        }
        else if (IP.zoneSize == 8)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(i, 0) = tmp8 + tmp10;
            block.at<double>(i, 4) = tmp8 - tmp10;
            tmp = C0 * ((int)tmp9 + tmp11);
            block.at<double>(i, 2) = tmp + (C1 * tmp9);
            block.at<double>(i, 6) = tmp - (C2 * tmp11);
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(i, 7) = tmp19 - tmp16;
            block.at<double>(i, 1) = tmp16 + tmp19;
            block.at<double>(i, 3) = C9 * tmp17;
            block.at<double>(i, 5) = C9 * tmp18;
        }
    }

    for (int j = 0; j <= IP.zoneSize - 1; j++)
    {
        if (IP.zoneSize == 2)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp10 = tmp2 + tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
        }
        else if (IP.zoneSize == 3)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(2, j) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
        }
        else if (IP.zoneSize == 4)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(2, j) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
            block.at<double>(3, j) = C9 * tmp17;
        }
        else if (IP.zoneSize == 5)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(4, j) = tmp8 - tmp10;
            block.at<double>(2, j) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
            block.at<double>(3, j) = C9 * tmp17;
        }
        else if (IP.zoneSize == 6)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(4, j) = tmp8 - tmp10;
            block.at<double>(2, j) = C0 * tmp11 + C10 * tmp9;
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
            block.at<double>(3, j) = C9 * tmp17;
            block.at<double>(5, j) = C9 * tmp18;
        }
        else if (IP.zoneSize == 7)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(4, j) = tmp8 - tmp10;
            tmp = C0 * ((int)tmp9 + tmp11);
            block.at<double>(2, j) = tmp + (C1 * tmp9);
            block.at<double>(6, j) = tmp - (C2 * tmp11);
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(1, j) = tmp16 + tmp19;
            block.at<double>(3, j) = C9 * tmp17;
            block.at<double>(5, j) = C9 * tmp18;
        }
        else if (IP.zoneSize == 8)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp8 = tmp0 + tmp6;
            tmp9 = tmp0 - tmp6;
            tmp10 = tmp2 + tmp4;
            tmp11 = tmp2 - tmp4;
            block.at<double>(0, j) = tmp8 + tmp10;
            block.at<double>(4, j) = tmp8 - tmp10;
            tmp = C0 * ((int)tmp9 + tmp11);
            block.at<double>(2, j) = tmp + (C1 * tmp9);
            block.at<double>(6, j) = tmp - (C2 * tmp11);
            tmp = C3 * ((int)tmp1 + tmp7);
            tmp12 = tmp + (C4 * tmp1);
            tmp13 = tmp - (C5 * tmp7);
            tmp = C6 * ((int)tmp3 + tmp5);
            tmp14 = tmp + (C7 * tmp3);
            tmp15 = tmp - (C8 * tmp5);
            tmp16 = tmp12 + tmp15;
            tmp17 = tmp13 - tmp14;
            tmp18 = tmp12 - tmp15;
            tmp19 = tmp13 + tmp14;
            block.at<double>(7, j) = tmp19 - tmp16;
            block.at<double>(1, j) = tmp16 + tmp19;
            block.at<double>(3, j) = C9 * tmp17;
            block.at<double>(5, j) = C9 * tmp18;
        }
    }

    for (int x = 0; x <= IP.zoneSize - 1; x++)
    {
        for (int y = 0; y <= IP.zoneSize - 1 - x; y++)
        {
            blockDCT.at<double>(x, y) = block.at<double>(x, y);
        }
    }

    return blockDCT;
}

Mat_<double> binEncoder(inputParameters IP, Mat_<double> block)
{
    Mat_<double> blockDCT = Mat::zeros(IP.zoneSize, IP.zoneSize, CV_64F);
    double tmp, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16, tmp17, tmp18, tmp19, tmp20, tmp21, tmp22, tmp23, tmp24;

    for (int i = 0; i <= block.rows - 1; i++)
    {
        if (IP.zoneSize == 2)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp10 = tmp0 + tmp6;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp19 = tmp10 + tmp16;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
        }
        else if (IP.zoneSize == 3)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
        }
        else if (IP.zoneSize == 4)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
            block.at<double>(i, 3) = tmp24;
        }
        else if (IP.zoneSize == 5)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
            block.at<double>(i, 3) = tmp24;
            block.at<double>(i, 4) = tmp20;
        }
        else if (IP.zoneSize == 6)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
            block.at<double>(i, 3) = tmp24;
            block.at<double>(i, 4) = tmp20;
            block.at<double>(i, 5) = tmp23;
        }
        else if (IP.zoneSize == 7)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
            block.at<double>(i, 3) = tmp24;
            block.at<double>(i, 4) = tmp20;
            block.at<double>(i, 5) = tmp23;
            block.at<double>(i, 6) = tmp21;
        }
        else if (IP.zoneSize == 8)
        {
            tmp0 = block.at<double>(i, 0) + block.at<double>(i, 7);
            tmp1 = block.at<double>(i, 0) - block.at<double>(i, 7);
            tmp2 = block.at<double>(i, 1) + block.at<double>(i, 6);
            tmp3 = block.at<double>(i, 1) - block.at<double>(i, 6);
            tmp4 = block.at<double>(i, 2) + block.at<double>(i, 5);
            tmp5 = block.at<double>(i, 2) - block.at<double>(i, 5);
            tmp6 = block.at<double>(i, 3) + block.at<double>(i, 4);
            tmp7 = block.at<double>(i, 3) - block.at<double>(i, 4);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp12 = tmp7 + tmp9;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp = ((int)tmp15 >> 3);
            tmp18 = tmp12 - tmp;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(i, 0) = tmp19;
            block.at<double>(i, 1) = tmp15;
            block.at<double>(i, 2) = tmp22;
            block.at<double>(i, 3) = tmp24;
            block.at<double>(i, 4) = tmp20;
            block.at<double>(i, 5) = tmp23;
            block.at<double>(i, 6) = tmp21;
            block.at<double>(i, 7) = tmp18;
        }
    }

    for (int j = 0; j <= IP.zoneSize - 1; j++)
    {
        if (IP.zoneSize == 2)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp10 = tmp0 + tmp6;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp19 = tmp10 + tmp16;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
        }
        else if (IP.zoneSize == 3)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
        }
        else if (IP.zoneSize == 4)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
            block.at<double>(3, j) = tmp24;
        }
        else if (IP.zoneSize == 5)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
            block.at<double>(3, j) = tmp24;
            block.at<double>(4, j) = tmp20;
        }
        else if (IP.zoneSize == 6)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
            block.at<double>(3, j) = tmp24;
            block.at<double>(4, j) = tmp20;
            block.at<double>(5, j) = tmp23;
        }
        else if (IP.zoneSize == 7)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
            block.at<double>(3, j) = tmp24;
            block.at<double>(4, j) = tmp20;
            block.at<double>(5, j) = tmp23;
            block.at<double>(6, j) = tmp21;
        }
        else if (IP.zoneSize == 8)
        {
            tmp0 = block.at<double>(0, j) + block.at<double>(7, j);
            tmp1 = block.at<double>(0, j) - block.at<double>(7, j);
            tmp2 = block.at<double>(1, j) + block.at<double>(6, j);
            tmp3 = block.at<double>(1, j) - block.at<double>(6, j);
            tmp4 = block.at<double>(2, j) + block.at<double>(5, j);
            tmp5 = block.at<double>(2, j) - block.at<double>(5, j);
            tmp6 = block.at<double>(3, j) + block.at<double>(4, j);
            tmp7 = block.at<double>(3, j) - block.at<double>(4, j);
            tmp = ((int)tmp5 << 2) - tmp5;
            tmp8 = tmp3 + ((int)tmp >> 3);
            tmp = ((int)tmp8 << 2) + tmp8;
            tmp9 = ((int)tmp >> 3) - tmp5;
            tmp10 = tmp0 + tmp6;
            tmp11 = tmp0 - tmp6;
            tmp12 = tmp7 + tmp9;
            tmp13 = tmp7 - tmp9;
            tmp14 = tmp1 - tmp8;
            tmp15 = tmp1 + tmp8;
            tmp16 = tmp2 + tmp4;
            tmp17 = tmp2 - tmp4;
            tmp = ((int)tmp15 >> 3);
            tmp18 = tmp12 - tmp;
            tmp19 = tmp10 + tmp16;
            tmp = ((int)tmp19 >> 1);
            tmp20 = tmp - tmp16;
            tmp = ((int)tmp11 << 2) - tmp11;
            tmp21 = tmp17 - ((int)tmp >> 3);
            tmp = ((int)tmp21 << 2) - tmp21;
            tmp22 = tmp11 + ((int)tmp >> 3);
            tmp = ((int)tmp14 << 3) - tmp14;
            tmp23 = tmp13 + ((int)tmp >> 3);
            tmp = ((int)tmp23 >> 1);
            tmp24 = tmp14 - tmp;
            block.at<double>(0, j) = tmp19;
            block.at<double>(1, j) = tmp15;
            block.at<double>(2, j) = tmp22;
            block.at<double>(3, j) = tmp24;
            block.at<double>(4, j) = tmp20;
            block.at<double>(5, j) = tmp23;
            block.at<double>(6, j) = tmp21;
            block.at<double>(7, j) = tmp18;
        }
    }

    for (int x = 0; x <= IP.zoneSize - 1; x++)
    {
        for (int y = 0; y <= IP.zoneSize - 1 - x; y++)
        {
            blockDCT.at<double>(x, y) = block.at<double>(x, y);
        }
    }

    return blockDCT;
}

Mat_<double> dctDecoder(inputParameters IP, Mat_<double> blockDCT)
{
    Mat_<double> block;

    if (!(IP.DCT.compare("CLA")))
    {
        block = claDecoder(blockDCT);
    }
    else if (!(IP.DCT.compare("LLM")))
    {
        block = llmDecoder(blockDCT);
    }
    else if (!(IP.DCT.compare("BIN")))
    {
        block = binDecoder(blockDCT);
    }

    return block;
}

Mat_<double> claDecoder(Mat_<double> blockDCT)
{
    Mat_<double> block = Mat::zeros(blockDCT.rows, blockDCT.cols, CV_64F);
    double Cx, Cy, SUMxy;

    for (int i = 0; i <= blockDCT.rows - 1; i++)
    {
        for (int j = 0; j <= blockDCT.cols - 1; j++)
        {
            SUMxy = 0;

            for (int x = 0; x <= blockDCT.rows - 1; x++)
            {
                for (int y = 0; y <= blockDCT.cols - 1; y++)
                {
                    if (x == 0 && y == 0)
                    {
                        Cx = sqrt(1.0 / blockDCT.rows);
                        Cy = sqrt(1.0 / blockDCT.cols);
                    }
                    else if (x > 0 && y == 0)
                    {
                        Cx = sqrt(2.0 / blockDCT.rows);
                        Cy = sqrt(1.0 / blockDCT.cols);
                    }
                    else if (x == 0 && y > 0)
                    {
                        Cx = sqrt(1.0 / blockDCT.rows);
                        Cy = sqrt(2.0 / blockDCT.cols);
                    }
                    else if (x > 0 && y > 0)
                    {
                        Cx = sqrt(2.0 / blockDCT.rows);
                        Cy = sqrt(2.0 / blockDCT.cols);
                    }

                    SUMxy = SUMxy + Cx * Cy * blockDCT.at<double>(x, y) * cos(((2.0 * j + 1.0) * y * CV_PI) / (2.0 * blockDCT.cols)) * cos(((2.0 * i + 1.0) * x * CV_PI) / (2.0 * blockDCT.rows));
                }
            }

            block.at<double>(i, j) = SUMxy;
        }
    }

    return block;
}

Mat_<double> llmDecoder(Mat_<double> blockDCT)
{
    Mat_<double> block = Mat::zeros(blockDCT.rows, blockDCT.cols, CV_64F);
    double tmp, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16, tmp17, tmp18, tmp19, tmp20, tmp21, tmp22, tmp23, tmp24, tmp25, C0 = 0.541196100146197, C1 = 0.765366864730179, C2 = 1.847759065022574, C3 = 0.831469612302545, C4 = -0.275899379282943, C5 = 1.387039845322148, C6 = 0.980785280403230, C7 = -0.785694958387102, C8 = 1.175875602419359, C9 = 1.414213562373095;

    for (int i = 0; i <= blockDCT.rows - 1; i++)
    {
        tmp0 = blockDCT.at<double>(i, 1) - blockDCT.at<double>(i, 7);
        tmp1 = blockDCT.at<double>(i, 1) + blockDCT.at<double>(i, 7);
        tmp2 = blockDCT.at<double>(i, 0) + blockDCT.at<double>(i, 4);
        tmp3 = blockDCT.at<double>(i, 0) - blockDCT.at<double>(i, 4);
        tmp = C0 * (blockDCT.at<double>(i, 2) + blockDCT.at<double>(i, 6));
        tmp4 = tmp - (C2 * blockDCT.at<double>(i, 6));
        tmp5 = tmp + (C1 * blockDCT.at<double>(i, 2));
        tmp6 = tmp0 + (C9 * blockDCT.at<double>(i, 5));
        tmp7 = tmp1 - (C9 * blockDCT.at<double>(i, 3));
        tmp8 = tmp0 - (C9 * blockDCT.at<double>(i, 5));
        tmp9 = tmp1 + (C9 * blockDCT.at<double>(i, 3));
        tmp = C6 * (tmp7 + tmp8);
        tmp10 = tmp - (C8 * tmp8);
        tmp11 = tmp + (C7 * tmp7);
        tmp12 = tmp2 + tmp5;
        tmp13 = tmp3 + tmp4;
        tmp14 = tmp3 - tmp4;
        tmp15 = tmp2 - tmp5;
        tmp = C3 * (tmp6 + tmp9);
        tmp16 = tmp - (C5 * tmp9);
        tmp17 = tmp + (C4 * tmp6);
        tmp18 = tmp12 + tmp17;
        tmp19 = tmp11 + tmp13;
        tmp20 = tmp10 + tmp14;
        tmp21 = tmp15 + tmp16;
        tmp22 = tmp15 - tmp16;
        tmp23 = tmp14 - tmp10;
        tmp24 = tmp13 - tmp11;
        tmp25 = tmp12 - tmp17;
        blockDCT.at<double>(i, 0) = tmp18;
        blockDCT.at<double>(i, 1) = tmp19;
        blockDCT.at<double>(i, 2) = tmp20;
        blockDCT.at<double>(i, 3) = tmp21;
        blockDCT.at<double>(i, 4) = tmp22;
        blockDCT.at<double>(i, 5) = tmp23;
        blockDCT.at<double>(i, 6) = tmp24;
        blockDCT.at<double>(i, 7) = tmp25;
    }

    for (int j = 0; j <= blockDCT.cols - 1; j++)
    {
        tmp0 = blockDCT.at<double>(1, j) - blockDCT.at<double>(7, j);
        tmp1 = blockDCT.at<double>(1, j) + blockDCT.at<double>(7, j);
        tmp2 = blockDCT.at<double>(0, j) + blockDCT.at<double>(4, j);
        tmp3 = blockDCT.at<double>(0, j) - blockDCT.at<double>(4, j);
        tmp = C0 * (blockDCT.at<double>(2, j) + blockDCT.at<double>(6, j));
        tmp4 = tmp - (C2 * blockDCT.at<double>(6, j));
        tmp5 = tmp + (C1 * blockDCT.at<double>(2, j));
        tmp6 = tmp0 + (C9 * blockDCT.at<double>(5, j));
        tmp7 = tmp1 - (C9 * blockDCT.at<double>(3, j));
        tmp8 = tmp0 - (C9 * blockDCT.at<double>(5, j));
        tmp9 = tmp1 + (C9 * blockDCT.at<double>(3, j));
        tmp = C6 * (tmp7 + tmp8);
        tmp10 = tmp - (C8 * tmp8);
        tmp11 = tmp + (C7 * tmp7);
        tmp12 = tmp2 + tmp5;
        tmp13 = tmp3 + tmp4;
        tmp14 = tmp3 - tmp4;
        tmp15 = tmp2 - tmp5;
        tmp = C3 * (tmp6 + tmp9);
        tmp16 = tmp - (C5 * tmp9);
        tmp17 = tmp + (C4 * tmp6);
        tmp18 = tmp12 + tmp17;
        tmp19 = tmp11 + tmp13;
        tmp20 = tmp10 + tmp14;
        tmp21 = tmp15 + tmp16;
        tmp22 = tmp15 - tmp16;
        tmp23 = tmp14 - tmp10;
        tmp24 = tmp13 - tmp11;
        tmp25 = tmp12 - tmp17;
        blockDCT.at<double>(0, j) = tmp18;
        blockDCT.at<double>(1, j) = tmp19;
        blockDCT.at<double>(2, j) = tmp20;
        blockDCT.at<double>(3, j) = tmp21;
        blockDCT.at<double>(4, j) = tmp22;
        blockDCT.at<double>(5, j) = tmp23;
        blockDCT.at<double>(6, j) = tmp24;
        blockDCT.at<double>(7, j) = tmp25;
    }

    block = blockDCT / 64.0;

    return block;
}

Mat_<double> binDecoder(Mat_<double> blockDCT)
{
    Mat_<double> block = Mat::zeros(blockDCT.rows, blockDCT.cols, CV_64F);
    double tmp, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16, tmp17, tmp18, tmp19, tmp20, tmp21, tmp22, tmp23, tmp24;

    for (int i = 0; i <= blockDCT.rows - 1; i++)
    {
        tmp = ((int)blockDCT.at<double>(i, 0) >> 1);
        tmp0 = tmp - blockDCT.at<double>(i, 4);
        tmp1 = blockDCT.at<double>(i, 0) - tmp0;
        tmp = ((int)blockDCT.at<double>(i, 6) << 2) - blockDCT.at<double>(i, 6);
        tmp2 = blockDCT.at<double>(i, 2) - ((int)tmp >> 3);
        tmp = ((int)tmp2 << 2) - tmp2;
        tmp3 = blockDCT.at<double>(i, 6) + ((int)tmp >> 3);
        tmp = ((int)blockDCT.at<double>(i, 5) >> 1);
        tmp4 = blockDCT.at<double>(i, 3) + tmp;
        tmp = ((int)tmp4 << 3) - tmp4;
        tmp5 = blockDCT.at<double>(i, 5) - ((int)tmp >> 3);
        tmp = ((int)blockDCT.at<double>(i, 1) >> 3);
        tmp6 = blockDCT.at<double>(i, 7) + tmp;
        tmp7 = tmp0 + tmp3;
        tmp8 = tmp0 - tmp3;
        tmp9 = tmp1 + tmp2;
        tmp10 = tmp1 - tmp2;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 - tmp5;
        tmp13 = blockDCT.at<double>(i, 1) - tmp4;
        tmp14 = blockDCT.at<double>(i, 1) + tmp4;
        tmp = ((int)tmp13 << 2) + tmp13;
        tmp15 = ((int)tmp >> 3) - tmp12;
        tmp = ((int)tmp15 << 2) - tmp15;
        tmp16 = tmp13 - ((int)tmp >> 3);
        tmp17 = tmp10 + tmp11;
        tmp18 = tmp10 - tmp11;
        tmp19 = tmp8 + tmp15;
        tmp20 = tmp8 - tmp15;
        tmp21 = tmp7 + tmp16;
        tmp22 = tmp7 - tmp16;
        tmp23 = tmp9 + tmp14;
        tmp24 = tmp9 - tmp14;
        blockDCT.at<double>(i, 0) = tmp23;
        blockDCT.at<double>(i, 1) = tmp21;
        blockDCT.at<double>(i, 2) = tmp19;
        blockDCT.at<double>(i, 3) = tmp17;
        blockDCT.at<double>(i, 4) = tmp18;
        blockDCT.at<double>(i, 5) = tmp20;
        blockDCT.at<double>(i, 6) = tmp22;
        blockDCT.at<double>(i, 7) = tmp24;
    }

    for (int j = 0; j <= blockDCT.cols - 1; j++)
    {
        tmp = ((int)blockDCT.at<double>(0, j) >> 1);
        tmp0 = tmp - blockDCT.at<double>(4, j);
        tmp1 = blockDCT.at<double>(0, j) - tmp0;
        tmp = ((int)blockDCT.at<double>(6, j) << 2) - blockDCT.at<double>(6, j);
        tmp2 = blockDCT.at<double>(2, j) - ((int)tmp >> 3);
        tmp = ((int)tmp2 << 2) - tmp2;
        tmp3 = blockDCT.at<double>(6, j) + ((int)tmp >> 3);
        tmp = ((int)blockDCT.at<double>(5, j) >> 1);
        tmp4 = blockDCT.at<double>(3, j) + tmp;
        tmp = ((int)tmp4 << 3) - tmp4;
        tmp5 = blockDCT.at<double>(5, j) - ((int)tmp >> 3);
        tmp = ((int)blockDCT.at<double>(1, j) >> 3);
        tmp6 = blockDCT.at<double>(7, j) + tmp;
        tmp7 = tmp0 + tmp3;
        tmp8 = tmp0 - tmp3;
        tmp9 = tmp1 + tmp2;
        tmp10 = tmp1 - tmp2;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 - tmp5;
        tmp13 = blockDCT.at<double>(1, j) - tmp4;
        tmp14 = blockDCT.at<double>(1, j) + tmp4;
        tmp = ((int)tmp13 << 2) + tmp13;
        tmp15 = ((int)tmp >> 3) - tmp12;
        tmp = ((int)tmp15 << 2) - tmp15;
        tmp16 = tmp13 - ((int)tmp >> 3);
        tmp17 = tmp10 + tmp11;
        tmp18 = tmp10 - tmp11;
        tmp19 = tmp8 + tmp15;
        tmp20 = tmp8 - tmp15;
        tmp21 = tmp7 + tmp16;
        tmp22 = tmp7 - tmp16;
        tmp23 = tmp9 + tmp14;
        tmp24 = tmp9 - tmp14;
        blockDCT.at<double>(0, j) = tmp23;
        blockDCT.at<double>(1, j) = tmp21;
        blockDCT.at<double>(2, j) = tmp19;
        blockDCT.at<double>(3, j) = tmp17;
        blockDCT.at<double>(4, j) = tmp18;
        blockDCT.at<double>(5, j) = tmp20;
        blockDCT.at<double>(6, j) = tmp22;
        blockDCT.at<double>(7, j) = tmp24;
    }

    for (int i = 0; i <= blockDCT.rows - 1; i++)
    {
        for (int j = 0; j <= blockDCT.cols - 1; j++)
        {
            block.at<double>(i, j) = ((int)blockDCT.at<double>(i, j) >> 4);
        }
    }

    return block;
}
