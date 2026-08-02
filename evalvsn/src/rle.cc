#include "evalvsn.h"

prioritiesInfo rleEncoder(prioritiesInfo PI)
{
    Mat_<double> RLE;
    double occurrence,lastOccurrence,previousOccurrence = -1;
    
    for (int i = 0;i <= PI.level.cols - 1;i++)
    {
        if (i <  PI.level.cols - 1)
        {
            if (PI.level.at<double>(i) != PI.level.at<double>(i + 1))
            {
                lastOccurrence = i;
                occurrence = lastOccurrence - previousOccurrence;
                previousOccurrence = lastOccurrence;
                RLE.push_back(occurrence);
                RLE.push_back(PI.level.at<double>(lastOccurrence));
            }
        }
        else if (i == PI.level.cols - 1 && PI.level.at<double>(i) != 0)
        {
            lastOccurrence = i;
            occurrence = lastOccurrence - previousOccurrence;
            previousOccurrence = lastOccurrence;
            RLE.push_back(occurrence);
            RLE.push_back(PI.level.at<double>(lastOccurrence));
        }
    }
    
    PI.level = RLE.t();
            
    return PI;
}

prioritiesInfo rleDecoder(prioritiesInfo PI)
{
    Mat_<double> level;
    
    for (int i = 0;i <= PI.level.cols - 1;i += 2)
    {
        for (int occurrence = 1;occurrence <= PI.level.at<double>(i);occurrence++)
        {
            level.push_back(PI.level.at<double>(i + 1));
        }
    }
    
    PI.level = level.t();
    
    return PI;
}
