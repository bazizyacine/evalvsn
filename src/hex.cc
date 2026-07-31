#include "evalvsn.h"

prioritiesInfo hexEncoder(prioritiesInfo PI)
{
    map<string,string> bin2hex = {{"0000","0"},{"0001","1"},{"0010","2"},{"0011","3"},{"0100","4"},{"0101","5"},{"0110","6"},{"0111","7"},{"1000","8"},{"1001","9"},{"1010","A"},{"1011","B"},{"1100","C"},{"1101","D"},{"1110","E"},{"1111","F"}};
    string hex;
    
    for (int i = 0;i < ((int) PI.data.length());i += 4)
    {
        hex += bin2hex[PI.data.substr(i,4)];
    }
    
    PI.data = hex;
    
    return PI;
}

prioritiesInfo hexDecoder(prioritiesInfo PI)
{
    map<string,string> hex2bin = {{"0","0000"},{"1","0001"},{"2","0010"},{"3","0011"},{"4","0100"},{"5","0101"},{"6","0110"},{"7","0111"},{"8","1000"},{"9","1001"},{"A","1010"},{"B","1011"},{"C","1100"},{"D","1101"},{"E","1110"},{"F","1111"}};
    string bin;
    
    for (int i = 0;i < ((int) PI.data.length());i++)
    {
        bin += hex2bin[PI.data.substr(i,1)];
    }
    
    PI.data = bin;
    
    return PI;
}

prioritiesInfo digitEncoder(prioritiesInfo PI)
{
    string digits;
    double r;
    
    r = ((int) PI.data.length()) % 4;
    
    if (r == 0)
    {
        PI.data = dec2bin(r,4) + PI.data;
    }
    else if (r != 0)
    {
        PI.data = dec2bin(4 - r,4) + digits.assign(4 - r,'0') + PI.data;
    }
    
    return PI;
}

prioritiesInfo digitDecoder(prioritiesInfo PI)
{
    string bin;
    double begin;
    
    begin = bin2dec(PI.data.substr(0,4),4);
    
    PI.data = PI.data.substr(begin + 4,((int) PI.data.length()) - begin - 4);
    
    return PI;
}
