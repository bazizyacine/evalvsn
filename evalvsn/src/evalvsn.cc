#include "evalvsn.h"

void printUsage(const char *progName)
{
    cerr << "Usage: " << progName
         << " -ip input_parameters_file"
         << " -vf video_file"
         << " -od output_directory"
         << " [-mpl bytes]"
         << " [-decode]"
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

    // Parse arguments. -decode takes no value: it selects the decoding pass,
    // which used to be asked through the rcvFile column of a second,
    // otherwise identical ipFile (R32).
    bool decode = false;

    for (int i = 1; i < argc;)
    {
        if (strcmp(argv[i], "-decode") == 0)
        {
            decode = true;
            i += 1;
        }
        else if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc)
        {
            ipPath = argv[i + 1];
            i += 2;
        }
        else if (strcmp(argv[i], "-vf") == 0 && i + 1 < argc)
        {
            videoFile = argv[i + 1];
            i += 2;
        }
        else if (strcmp(argv[i], "-od") == 0 && i + 1 < argc)
        {
            outputDirectory = argv[i + 1];
            i += 2;
        }
        else if (strcmp(argv[i], "-mpl") == 0 && i + 1 < argc && isValidInt(argv[i + 1]) && atoi(argv[i + 1]) > 2)
        {
            IP.mpl = atoi(argv[i + 1]);
            i += 2;
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

    // The source family is read from the video itself (R39): resolution,
    // frame count and duration belong to the file, and a declaration that
    // contradicted it used to pass without an error. What the file cannot
    // tell is the capture cadence - FPS stays a column, and joins the
    // capture calendar when the tranche-5 bricks land.
    VideoCapture probe(videoFile);

    if (!probe.isOpened())
    {
        cerr << "Could not probe video file: " << videoFile << '\n';
        return EXIT_FAILURE;
    }

    int srcWidth = (int)probe.get(CAP_PROP_FRAME_WIDTH);
    int srcHeight = (int)probe.get(CAP_PROP_FRAME_HEIGHT);
    int srcFrames = (int)probe.get(CAP_PROP_FRAME_COUNT);
    double srcFps = probe.get(CAP_PROP_FPS);

    probe.release();

    if (srcWidth <= 0 || srcHeight <= 0 || srcFrames <= 0 || srcFps <= 0)
    {
        cerr << "Video file reports no usable geometry: " << videoFile << '\n';
        return EXIT_FAILURE;
    }

    mkDir(outputDirectory);

    while (getline(ipFile, line, '\n'))
    {
        if (line.empty() || line[0] == '#' || line[0] == ' ')
            continue;

        if (getIP(IP, line, errParam))
        {
            // The reception trace is no longer an ipFile column: the chain
            // only ever wrote one name, and which pass runs is the command
            // line's business now.
            IP.rcvFile = decode ? "rt-packet.dat" : "";

            // The derived source family, height x width as the column always
            // wrote it.
            IP.imageResolution = to_string(srcHeight) + 'x' + to_string(srcWidth);
            IP.imagesNb = srcFrames;
            IP.videoDuration = (int)llround(srcFrames / srcFps);

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
