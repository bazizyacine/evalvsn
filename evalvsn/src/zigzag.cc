#include "evalvsn.h"

Mat_<double> zigzagEncoder(Mat_<double> blockQTF)
{
    Mat_<double> blockZigZag;
    int h = 0, v = 0, vmin = 0, hmin = 0, vmax = blockQTF.rows - 1, hmax = blockQTF.cols - 1;

    while ((v <= vmax) && (h <= hmax))
    {
        if (((h + v) % 2) == 0)
        {
            if (v == vmin)
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                if (h == hmax)
                {
                    v = v + 1;
                }
                else if (h != hmax)
                {
                    h = h + 1;
                }
            }
            else if ((h == hmax) && (v < vmax))
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                v = v + 1;
            }
            else if ((v > vmin) && (h < hmax))
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                v = v - 1;
                h = h + 1;
            }
        }
        else if (((h + v) % 2) != 0)
        {
            if ((v == vmax) && (h <= hmax))
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                h = h + 1;
            }
            else if (h == hmin)
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                if (v == vmax)
                {
                    h = h + 1;
                }
                else if (v != vmax)
                {
                    v = v + 1;
                }
            }
            else if ((v < vmax) && (h > hmin))
            {
                blockZigZag.push_back(blockQTF.at<double>(v, h));
                v = v + 1;
                h = h - 1;
            }
        }

        if ((v == vmax) && (h == hmax))
        {
            blockZigZag.push_back(blockQTF.at<double>(v, h));
            break;
        }
    }

    return blockZigZag.t();
}

Mat_<double> zigzagDecoder(Mat_<double> blockZigZag, int vmax, int hmax)
{
    Mat_<double> blockQTF = Mat::zeros(vmax, hmax, CV_64F);
    int i = 0, h = 0, v = 0, vmin = 0, hmin = 0;

    while ((v <= vmax - 1) && (h <= hmax - 1))
    {
        if (((h + v) % 2) == 0)
        {
            if (v == vmin)
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);

                if (h == hmax - 1)
                {
                    v = v + 1;
                }
                else if (h != hmax - 1)
                {
                    h = h + 1;
                }

                i = i + 1;
            }
            else if ((h == hmax - 1) && (v < vmax - 1))
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);
                v = v + 1;
                i = i + 1;
            }
            else if ((v > vmin) && (h < hmax - 1))
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);
                v = v - 1;
                h = h + 1;
                i = i + 1;
            }
        }
        else if (((h + v) % 2) != 0)
        {
            if ((v == vmax - 1) && (h <= hmax - 1))
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);
                h = h + 1;
                i = i + 1;
            }
            else if (h == hmin)
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);

                if (v == vmax - 1)
                {
                    h = h + 1;
                }
                else if (v != vmax - 1)
                {
                    v = v + 1;
                }

                i = i + 1;
            }
            else if ((v < vmax - 1) && (h > hmin))
            {
                blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);
                v = v + 1;
                h = h - 1;
                i = i + 1;
            }
        }

        if ((v == vmax - 1) && (h == hmax - 1))
        {
            blockQTF.at<double>(v, h) = blockZigZag.at<double>(i);
            break;
        }
    }

    return blockQTF;
}
