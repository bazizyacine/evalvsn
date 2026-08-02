#include "evalvsn.h"

void printUsage(const char *progName)
{
    cerr << "Usage: " << progName
         << " -ip input_parameters_file"
         << " -vf video_file"
         << " -od output_directory"
         << " [-h | --help]"
         << endl;
}

int main(int argc, char *argv[])
{
    inputParameters IP;
    ifstream ipFile;
    string ipPath, videoFile, outputDirectory, line, errParam;

    if (argc < 2)
    {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Handle help flag first
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    if (argc < 7)
    {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse arguments
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            ipPath = argv[i + 1];
        }
        else if (strcmp(argv[i], "-vf") == 0)
        {
            videoFile = argv[i + 1];
        }
        else if (strcmp(argv[i], "-od") == 0)
        {
            outputDirectory = argv[i + 1];
        }
        else
        {
            cerr << "Unknown option: " << argv[i] << endl;
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    IP.payloadFormat = "dat";

    ipFile.open(ipPath, ifstream::in);

    if (!(ipFile))
    {
        cerr << "Could not open input parameters file: " << ipPath << '\n';
        return EXIT_FAILURE;
    }

    if (!(ifstream(videoFile)))
    {
        cerr << "Could not open video file: " << videoFile << '\n';
        return EXIT_FAILURE;
    }

    mkDir(outputDirectory);

    while (getline(ipFile, line, '\n'))
    {
        if (line.empty() || line[0] == '#' || line[0] == ' ')
            continue;

        if (getIP(IP, line, errParam))
        {
            if (IP.rcvFile.empty())
            {
                avi2png(IP, videoFile, outputDirectory);

                if (videoEncoder(IP, videoFile, outputDirectory))
                {
                    cerr << "Error during video encoding !" << '\n';
                    return EXIT_FAILURE;
                }
            }

            if (videoDecoder(IP, videoFile, outputDirectory))
            {
                cerr << "Error during video decoding !" << '\n';
                return EXIT_FAILURE;
            }

            if (traceEvaluate(IP, videoFile, outputDirectory))
            {
                cerr << "Error during video evaluation !" << '\n';
                return EXIT_FAILURE;
            }

            png2avi(IP, videoFile, outputDirectory);
        }
        else
        {
            cerr << "Invalid parameter in input file: " << errParam << endl;
            return EXIT_FAILURE;
        }
    }

    ipFile.close();

    return EXIT_SUCCESS;
}
