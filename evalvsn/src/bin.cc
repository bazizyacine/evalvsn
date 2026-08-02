#include "evalvsn.h"

prioritiesInfo binaryEncoder(inputParameters IP, prioritiesInfo PI)
{
    if (!(IP.encoder.compare("MMPEG")))
    {
        if (!(IP.binLevel.compare("ST")))
        {
            PI = staticEncoder(IP, PI);
        }
        else if (!(IP.binLevel.compare("EG")))
        {
            PI = expGolombEncoder(PI);
        }
    }
    else if (!(IP.encoder.compare("SC")))
    {
        PI = staticEncoder(IP, PI);
    }

    return PI;
}

prioritiesInfo binaryDecoder(inputParameters IP, prioritiesInfo PI)
{
    if (!(IP.binLevel.compare("ST")))
    {
        PI = staticDecoder(IP, PI);
    }
    else if (!(IP.binLevel.compare("EG")))
    {
        PI = expGolombDecoder(PI);
    }

    return PI;
}

string dec2bin(double dec, int bits)
{
    string bin;
    int r;

    bin.assign(bits, '0');

    for (int i = bits - 1; i >= 0; i--)
    {
        r = floor(fabs(dec) / 2.0);

        bin.replace(i, 1, to_string(((int)fabs(dec)) - (2 * r)));

        dec = r;
    }

    return bin;
}

double bin2dec(string bin, int bits)
{
    double dec = 0;

    for (int i = 0; i <= bits - 1; i++)
    {
        dec += atoi(bin.substr(i, 1).c_str()) * pow(2, bits - i - 1);
    }

    return dec;
}
