#include "evalvsn.h"

prioritiesInfo expGolombEncoder(prioritiesInfo PI)
{
    string bin;
    double groupID,index;
    int levelSize,levelOffset;
    
    levelOffset = 0;
    
    for (int i = 0;i <= PI.level.cols - 1;i++)
    {
        PI.level.at<double>(i) = negativeEncoder(PI.level.at<double>(i));
        
        if (PI.level.at<double>(i) == 0)
        {
            bin = dec2bin(PI.level.at<double>(i),1);
        }
        else if (PI.level.at<double>(i) != 0)
        {
            groupID = getGroupID(PI.level.at<double>(i));
            index = PI.level.at<double>(i) - ((1 << ((int) groupID)) - 1);
            bin = unaryEncoder(groupID) + dec2bin(index,groupID);
        }
        
        PI.data += bin;
        
        levelSize = ((int) bin.length());
        PI.levelSize.push_back(levelSize);
        PI.levelOffset.push_back(levelOffset);
        levelOffset += levelSize;
    }
    
    return PI;
}

prioritiesInfo expGolombDecoder(prioritiesInfo PI)
{
    double groupID,base,index;
    int begin = 0,levelSize,levelOffset;
    
    levelOffset = 0;
    
    for (int i = 0;i < ((int) PI.data.length());i++)
    {
        if (PI.data[i] == '0')
        {
            levelSize = 2 * (i - begin) + 1;
            
            PI.levelSize.push_back(levelSize);
            
            PI.levelOffset.push_back(levelOffset);
            
            levelOffset += levelSize;
            
            groupID = unaryDecoder(PI.data.substr(begin,i - begin));
            
            if (groupID == 0)
            {
                PI.level.push_back(negativeDecoder(groupID));
            }
            else if (groupID != 0)
            {
                base = (1 << ((int) groupID)) - 1;
                index = bin2dec(PI.data.substr(i + 1,groupID),groupID);
                PI.level.push_back(negativeDecoder(base + index));
            }
            
            begin = groupID + i + 1;
            
            i = begin - 1;
        }
    }
    
    PI.level = PI.level.t();
        
    return PI;
}

double negativeEncoder(double dec)
{
    double ne;
    
    if (dec >= 0)
    {
        ne = 2 * dec;
    }
    else if (dec < 0)
    {
        ne = -2 * dec - 1;
    }
    
    return ne;
}

double negativeDecoder(double ne)
{
    double dec;
    
    if (((int) ne) % 2 == 0)
    {
        dec = ne / 2;
    }
    else if (((int) ne) % 2 != 0)
    {
        dec = (ne + 1) / (-2);
    }
    
    return dec;
}

string unaryEncoder(double ne)
{
    string ue;
    
    while (ne > 0)
    {
        ue += dec2bin(1,1);
        ne--;
    }
    
    ue += dec2bin(0,1);
    
    return ue;
}

double unaryDecoder(string ue)
{
    double ne = 0;
    
    for (int i = 0;i < ((int) ue.length());i++)
    {
        if (bin2dec(ue.substr(i,1),1) == 1)
        {
            ne++;
        }
    }
    
    return ne;
}

double getGroupID(double ne)
{
    double groupID = 2;
    
    while (ne > ((1 << ((int) groupID)) - 2))
    {
        groupID++;
    }
    
    return groupID - 1;
}
