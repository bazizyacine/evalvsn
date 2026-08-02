#include "evalvsn.h"

void avi2png(inputParameters IP, string videoFile, string outputDirectory)
{
    string videoName, stPath;

    videoName = videoFile.substr(videoFile.find_last_of('/') + 1, videoFile.find_last_of('.') - videoFile.find_last_of('/') - 1);

    stPath = outputDirectory + '/' + videoName + '-' + IP.simId;

    mkDir(stPath + '/' + "allImages");

    system(("ffmpeg -i " + videoFile + ' ' + stPath + '/' + "allImages" + '/' + videoName + '_' + "%d" + ".png").c_str());
}

int videoEncoder(inputParameters IP, string videoFile, string outputDirectory)
{
    Mat capturedImage, grayImage;
    Mat_<double> currentImage, previousImage, currentFrame, MFrame, DFrame, encodedFrame;
    vector<prioritiesInfo> vecPI;
    vector<framesInfo> vecFI;
    prioritiesInfo PI;
    framesInfo FI;
    ofstream priorityTrace, frameTrace;
    string videoName, stPath, priorityFormat, frameFormat, priorityName, frameName, imagePath, imageName;
    double mse;
    int captureStep, frameResolutionHeight, frameResolutionWidth, frameZeros;

    videoName = videoFile.substr(videoFile.find_last_of('/') + 1, videoFile.find_last_of('.') - videoFile.find_last_of('/') - 1);

    stPath = outputDirectory + '/' + videoName + '-' + IP.simId;

    priorityName = stPath + '/' + "st" + '-' + "priority" + '.' + IP.payloadFormat;

    priorityTrace.open(priorityName, ofstream::out);

    priorityFormat = "#sendTime\tframeNb\tframeType\tdataPriority\tdataSize\tdata";

    priorityTrace << priorityFormat << '\n';

    priorityTrace.close();

    frameName = stPath + '/' + "st" + '-' + "frame" + '.' + IP.payloadFormat;

    frameTrace.open(frameName, ofstream::out);

    frameFormat = "#frameNb\tframeType\tframeSize\trefPSNR\trefSSIM\tcompRatio";

    frameTrace << frameFormat << '\n';

    frameTrace.close();

    mkDir(stPath + '/' + "capturedImages");

    captureStep = ceil(IP.imagesNb / (IP.videoDuration * IP.FPS));

    frameResolutionHeight = stoi(IP.imageResolution.substr(0, IP.imageResolution.find_last_of('x')));

    frameResolutionWidth = stoi(IP.imageResolution.substr(IP.imageResolution.find_last_of('x') + 1, ((int)IP.imageResolution.length())));

    PI.sendTime = 0;

    PI.frameNb = FI.frameNb = 0;

    for (int i = 1; i <= IP.imagesNb; i += captureStep)
    {
        FI.frameNb++;

        PI.frameNb++;

        imagePath = stPath + '/' + "allImages";

        imageName = imagePath + '/' + videoName + '_' + to_string(i) + ".png";

        capturedImage = imread(imageName, 1);

        resize(capturedImage, capturedImage, Size(frameResolutionWidth, frameResolutionHeight));

        if (capturedImage.empty())
        {
            break;
        }

        cvtColor(capturedImage, grayImage, COLOR_BGR2GRAY);

        imagePath = stPath + '/' + "capturedImages";

        imageName = imagePath + '/' + "frame" + '_' + to_string(FI.frameNb) + ".png";

        imwrite(imageName, grayImage);

        currentFrame = grayImage.clone();

        if (FI.frameNb == 1)
        {
            previousImage = currentFrame.clone();
        }
        else if (FI.frameNb != 1)
        {
            previousImage = currentImage.clone();
        }

        currentImage = currentFrame.clone();

        mse = mseEvaluate(currentImage, previousImage);

        if (mse > IP.threshold || FI.frameNb == 1)
        {
            if (!(IP.encoder.compare("MMPEG")))
            {
                if (FI.frameNb == 1)
                {
                    frameZeros = frameZerosEvaluate(currentFrame);
                }
                else if (FI.frameNb != 1)
                {
                    frameZeros = frameZerosEvaluate(currentFrame - MFrame);
                }

                if (frameZeros <= frameResolutionHeight * frameResolutionWidth * IP.gopCoef || FI.frameNb == 1)
                {
                    PI.frameType = FI.frameType = "M";
                    MFrame = currentFrame.clone();
                    encodedFrame = MFrame - 128;
                    vecPI = frameEncoder(IP, encodedFrame, PI);
                }
                else if (frameZeros > frameResolutionHeight * frameResolutionWidth * IP.gopCoef && FI.frameNb != 1)
                {
                    PI.frameType = FI.frameType = "D";
                    DFrame = currentFrame - MFrame;
                    vecPI = frameEncoder(IP, DFrame, PI);
                }
            }
            else if (!(IP.encoder.compare("SC")))
            {
                PI.frameType = FI.frameType = "SC";
                encodedFrame = currentFrame.clone();
                vecPI = frameEncoder(IP, encodedFrame, PI);
            }
        }
        else if (mse <= IP.threshold && FI.frameNb != 1)
        {
            FI.frameType = "N";
            vecPI.clear();
        }

        vecFI.push_back(FI);

        traceEncoder(IP, vecPI, vecFI, priorityName, frameName);

        vecFI.clear();

        PI.sendTime += 1.0 / IP.FPS;
    }

    return EXIT_SUCCESS;
}

int videoDecoder(inputParameters IP, string videoFile, string outputDirectory)
{
    Mat_<uint8_t> currentFrame, previousFrame;
    Mat_<double> MFrame;
    vector<prioritiesInfo> levelsInfo, decodedFrames(ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS))));
    vector<packetsInfo> vecPK;
    set<int> seenSeqNb;
    prioritiesInfo PI;
    packetsInfo PK;
    ifstream priorityTrace, inPacketTrace;
    ofstream outPacketTrace;
    stringstream lineStream;
    string videoName, stPath, priorityName, rtPath, stPacketName, rtPacketNameIn, rtPacketNameOut, imagePath, imageName, line;
    int frameResolutionHeight, frameResolutionWidth;

    videoName = videoFile.substr(videoFile.find_last_of('/') + 1, videoFile.find_last_of('.') - videoFile.find_last_of('/') - 1);

    stPath = outputDirectory + '/' + videoName + '-' + IP.simId;

    priorityName = stPath + '/' + "st" + '-' + "priority" + '.' + IP.payloadFormat;

    priorityTrace.open(priorityName, ifstream::in);

    if (!(priorityTrace))
    {
        cerr << "Could not open priority trace file: " << priorityName << '\n';
        return EXIT_FAILURE;
    }

    priorityTrace.close();

    if (!(IP.rcvFile.empty()))
    {
        stPacketName = stPath + '/' + "st" + '-' + "packet" + '.' + IP.payloadFormat;

        rtPacketNameIn = stPath + '/' + IP.rcvFile;

        rtPath = "sim";

        rtPacketNameOut = stPath + '/' + rtPath + '/' + IP.rcvFile;

        mkDir(stPath + '/' + rtPath);

        inPacketTrace.open(stPacketName, ifstream::in);

        if (!(inPacketTrace))
        {
            cerr << "Could not open packet trace file: " << stPacketName << '\n';
            return EXIT_FAILURE;
        }

        inPacketTrace.close();

        if (!(ifstream(rtPacketNameIn)))
        {
            cerr << "Could not open packet trace file: " << rtPacketNameIn << '\n';
            return EXIT_FAILURE;
        }

        inPacketTrace.open(rtPacketNameIn, ifstream::in);
        outPacketTrace.open(rtPacketNameOut, ofstream::out);

        while (getline(inPacketTrace, line, '\n'))
        {
            if (!line.empty() && line[0] == '#')
            {
                outPacketTrace << line << '\n';
                continue;
            }

            lineStream << line;

            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            if (seenSeqNb.find(PK.seqNb) == seenSeqNb.end())
            {
                seenSeqNb.insert(PK.seqNb);
                outPacketTrace << line << '\n';
            }

            lineStream.clear();
        }

        inPacketTrace.close();
        outPacketTrace.close();
    }

    if (IP.rcvFile.empty())
    {
        mkDir(stPath + '/' + "referenceImages");
    }
    else if (!(IP.rcvFile.empty()))
    {
        mkDir(stPath + '/' + rtPath + '/' + "decodedImages");
    }

    frameResolutionHeight = stoi(IP.imageResolution.substr(0, IP.imageResolution.find_last_of('x')));

    frameResolutionWidth = stoi(IP.imageResolution.substr(IP.imageResolution.find_last_of('x') + 1, ((int)IP.imageResolution.length())));

    for (int i = 0; i <= ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS))) - 1; i++)
    {
        decodedFrames[i].level = Mat::zeros(frameResolutionHeight, frameResolutionWidth, CV_64F);
    }

    if (!(IP.encoder.compare("MMPEG")))
    {
        MFrame = Mat::zeros(frameResolutionHeight, frameResolutionWidth, CV_64F);
    }

    if (!(IP.rcvFile.empty()))
    {
        vecPK = lostPackets(IP, stPacketName, rtPacketNameOut);
    }

    levelsInfo = traceDecoder(IP, vecPK, priorityName);

    for (int l = 0; l <= ((int)levelsInfo.size()) - 1; l++)
    {
        PI.blockNb = 0;

        for (int i = 0; i <= frameResolutionHeight - 1; i += 8)
        {
            for (int j = 0; j <= frameResolutionWidth - 1; j += 8)
            {
                PI.blockNb++;

                if (levelsInfo[l].blockNb == PI.blockNb)
                {
                    decodedFrames[levelsInfo[l].frameNb - 1].frameNb = levelsInfo[l].frameNb;
                    decodedFrames[levelsInfo[l].frameNb - 1].frameType = levelsInfo[l].frameType;
                    decodedFrames[levelsInfo[l].frameNb - 1].level(Range(i, i + 8), Range(j, j + 8)) = decodedFrames[levelsInfo[l].frameNb - 1].level(Range(i, i + 8), Range(j, j + 8)) + zigzagDecoder(levelsInfo[l].level, 8, 8);
                }
            }
        }
    }

    for (int i = 0; i <= ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS))) - 1; i++)
    {
        PI.frameNb = i + 1;

        if (decodedFrames[i].frameNb == PI.frameNb)
        {
            if (!(IP.encoder.compare("MMPEG")))
            {
                if (!(decodedFrames[i].frameType.compare("M")))
                {
                    decodedFrames[i].level = frameDecoder(IP, decodedFrames[i].level) + 128;
                    MFrame = decodedFrames[i].level;
                }
                else if (!(decodedFrames[i].frameType.compare("D")))
                {
                    decodedFrames[i].level = frameDecoder(IP, decodedFrames[i].level) + MFrame;
                }
            }
            else if (!(IP.encoder.compare("SC")))
            {
                if (!(decodedFrames[i].frameType.compare("SC")))
                {
                    decodedFrames[i].level = decodedFrames[i].level;
                }
            }

            if (PI.frameNb == 1)
            {
                currentFrame = decodedFrames[i].level.clone();
            }
            else if (PI.frameNb != 1)
            {
                previousFrame = currentFrame.clone();
                currentFrame = decodedFrames[i].level.clone();
            }
        }

        if (IP.rcvFile.empty())
        {
            imagePath = stPath + '/' + "referenceImages";
        }
        else if (!(IP.rcvFile.empty()))
        {
            imagePath = stPath + '/' + rtPath + '/' + "decodedImages";
        }

        imageName = imagePath + '/' + "frame" + '_' + to_string(PI.frameNb) + ".png";

        imwrite(imageName, currentFrame);
    }

    return EXIT_SUCCESS;
}

void png2avi(inputParameters IP, string videoFile, string outputDirectory)
{
    string videoName, stPath, rtPath;

    videoName = videoFile.substr(videoFile.find_last_of('/') + 1, videoFile.find_last_of('.') - videoFile.find_last_of('/') - 1);

    stPath = outputDirectory + '/' + videoName + '-' + IP.simId;

    if (IP.rcvFile.empty())
    {
        system(("ffmpeg -y -r " + to_string(IP.FPS) + " -i " + stPath + '/' + "capturedImages" + '/' + "frame" + '_' + "%d" + ".png" + ' ' + stPath + '/' + "capturedImages" + '/' + "capturedVideo" + ".avi").c_str());
        system(("ffmpeg -y -r " + to_string(IP.FPS) + " -i " + stPath + '/' + "referenceImages" + '/' + "frame" + '_' + "%d" + ".png" + ' ' + stPath + '/' + "referenceImages" + '/' + "referenceVideo" + ".avi").c_str());
    }
    else if (!(IP.rcvFile.empty()))
    {
        rtPath = "sim";
        system(("ffmpeg -y -r " + to_string(IP.FPS) + " -i " + stPath + '/' + rtPath + '/' + "decodedImages" + '/' + "frame" + '_' + "%d" + ".png" + ' ' + stPath + '/' + rtPath + '/' + "decodedImages" + '/' + "decodedVideo" + ".avi").c_str());
    }
}
