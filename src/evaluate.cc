#include "evalvsn.h"

int traceEvaluate(inputParameters IP, string videoFile, string outputDirectory)
{
    Mat_<double> capturedFrame, referenceFrame, decodedFrame;
    vector<framesInfo> vecFI(ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS))));
    vector<metricsInfo> stVecMI(ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS)))), rtVecMI(ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS))));
    framesInfo FI;
    packetsInfo PK;
    metricsInfo stMI, rtMI;
    ifstream inTrace;
    ofstream outTrace;
    stringstream lineStream;
    string metricsFormat, videoName, stPath, frameName, frameFormat, packetName, rtPath, metricsName, imagePath, imageName, line;
    int frameResolutionHeight, frameResolutionWidth, capturedFrameSize;

    if (!(IP.encoder.compare("MMPEG")))
    {
        metricsFormat = "#psnr\trefPSNR\tssim\trefSSIM\tpktLoss\tpktLossM\tpktLossD\tpktlossM1\tpktlossM2\tpktlossM3\tpktlossM4\tpktlossM5\tpktlossM6\tpktlossM7\tpktlossM8\tpktlossM9\tpktlossM10\tpktlossM11\tpktlossM12\tpktlossM13\tpktlossD1\tpktlossD2\tpktlossD3\tpktlossD4\tpktlossD5\tpktlossD6\tpktlossD7\tpktlossD8\tpktlossD9\tpktlossD10\tpktlossD11\tpktlossD12\tpktlossD13";
    }
    else if (!(IP.encoder.compare("SC")))
    {
        metricsFormat = "#psnr\trefPSNR\tssim\trefSSIM\tpktLoss";
    }

    videoName = videoFile.substr(videoFile.find_last_of('/') + 1, videoFile.find_last_of('.') - videoFile.find_last_of('/') - 1);

    stPath = outputDirectory + '/' + videoName + '-' + IP.simId;

    if (IP.rcvFile.empty())
    {
        frameResolutionHeight = stoi(IP.imageResolution.substr(0, IP.imageResolution.find_last_of('x')));

        frameResolutionWidth = stoi(IP.imageResolution.substr(IP.imageResolution.find_last_of('x') + 1, ((int)IP.imageResolution.length())));

        capturedFrameSize = frameResolutionHeight * frameResolutionWidth;

        frameName = stPath + '/' + "st" + '-' + "frame" + '.' + IP.payloadFormat;

        inTrace.open(frameName, ifstream::in);

        if (!(inTrace))
        {
            cerr << "Could not open frame trace file: " << frameName << '\n';
            return EXIT_FAILURE;
        }

        getline(inTrace, frameFormat, '\n');

        while (getline(inTrace, line, '\n'))
        {
            lineStream << line;

            lineStream >> FI.frameNb >> FI.frameType >> FI.frameSize >> FI.refPSNR >> FI.refSSIM >> FI.compRatio;

            vecFI[FI.frameNb - 1] = FI;

            lineStream.clear();
        }

        inTrace.close();

        outTrace.open(frameName, ofstream::out);

        outTrace << frameFormat << '\n';

        for (int i = 0; i <= ((int)vecFI.size()) - 1; i++)
        {
            FI.frameNb = i + 1;

            imagePath = stPath + '/' + "capturedImages";

            imageName = imagePath + '/' + "frame" + '_' + to_string(FI.frameNb) + ".png";

            capturedFrame = imread(imageName, 0);

            imagePath = stPath + '/' + "referenceImages";

            imageName = imagePath + '/' + "frame" + '_' + to_string(FI.frameNb) + ".png";

            referenceFrame = imread(imageName, 0);

            vecFI[i].refPSNR = psnrEvaluate(capturedFrame, referenceFrame);

            vecFI[i].refSSIM = ssimEvaluate(capturedFrame, referenceFrame);

            vecFI[i].compRatio = ratioEvaluate(capturedFrameSize, vecFI[i].frameSize);

            outTrace << vecFI[i].frameNb << '\t' << vecFI[i].frameType << '\t' << vecFI[i].frameSize << '\t' << vecFI[i].refPSNR << '\t' << vecFI[i].refSSIM << '\t';

            if (!(vecFI[i].frameType.compare("M")) || !(vecFI[i].frameType.compare("D")) || !(vecFI[i].frameType.compare("SC")))
            {
                outTrace << vecFI[i].compRatio << '\n';
            }
            else if (!(vecFI[i].frameType.compare("N")))
            {
                outTrace << "N/A" << '\n';
            }
        }

        outTrace.close();
    }
    else if (!(IP.rcvFile.empty()))
    {
        packetName = stPath + '/' + "st" + '-' + "packet" + '.' + IP.payloadFormat;

        inTrace.open(packetName, ifstream::in);

        if (!(inTrace))
        {
            cerr << "Could not open packet trace file: " << packetName << '\n';
            return EXIT_FAILURE;
        }

        getline(inTrace, line, '\n');

        while (getline(inTrace, line, '\n'))
        {
            lineStream << line;

            lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

            stVecMI[PK.frameNb - 1].frameNb = PK.frameNb;

            stVecMI[PK.frameNb - 1].pktLoss++;

            if (!(IP.encoder.compare("MMPEG")))
            {
                if (!(PK.frameType.compare("M")))
                {
                    stVecMI[PK.frameNb - 1].pktLossM++;
                    stVecMI[PK.frameNb - 1].pktlossPriority.at<double>(0, PK.prioNb - 1)++;
                }
                else if (!(PK.frameType.compare("D")))
                {
                    stVecMI[PK.frameNb - 1].pktLossD++;
                    stVecMI[PK.frameNb - 1].pktlossPriority.at<double>(1, PK.prioNb - 1)++;
                }
            }

            lineStream.clear();
        }

        inTrace.close();

        rtPath = "sim";

        packetName = stPath + '/' + rtPath + '/' + IP.rcvFile;

        inTrace.open(packetName, ifstream::in);

        if (!(inTrace))
        {
            cerr << "Could not open packet trace file: " << packetName << '\n';
            return EXIT_FAILURE;
        }

        while (getline(inTrace, line, '\n'))
        {
            if (line[0] != '#' && line[0] != ' ' && !(line.empty()) && (count(line.begin(), line.end(), '\t') + 1) == 7)
            {
                lineStream << line;

                lineStream >> PK.sTime >> PK.seqNb >> PK.pSize >> PK.frameNb >> PK.frameType >> PK.prioNb >> PK.offset;

                rtVecMI[PK.frameNb - 1].frameNb = PK.frameNb;

                rtVecMI[PK.frameNb - 1].pktLoss++;

                if (!(IP.encoder.compare("MMPEG")))
                {
                    if (!(PK.frameType.compare("M")))
                    {
                        rtVecMI[PK.frameNb - 1].pktLossM++;
                        rtVecMI[PK.frameNb - 1].pktlossPriority.at<double>(0, PK.prioNb - 1)++;
                    }
                    else if (!(PK.frameType.compare("D")))
                    {
                        rtVecMI[PK.frameNb - 1].pktLossD++;
                        rtVecMI[PK.frameNb - 1].pktlossPriority.at<double>(1, PK.prioNb - 1)++;
                    }
                }

                lineStream.clear();
            }
        }

        inTrace.close();

        frameName = stPath + '/' + rtPath + '/' + "rt" + '-' + "frame" + '.' + IP.payloadFormat;

        outTrace.open(frameName, ofstream::out);

        if (!(IP.encoder.compare("MMPEG")))
        {
            frameFormat = "#frameNb\tpsnr\tssim\tpktLoss\tpktloss1\tpktloss2\tpktloss3\tpktloss4\tpktloss5\tpktloss6\tpktloss7\tpktloss8\tpktloss9\tpktloss10\tpktloss11\tpktloss12\tpktloss13";
        }
        else if (!(IP.encoder.compare("SC")))
        {
            frameFormat = "#frameNb\tpsnr\tssim\tpktLoss";
        }

        outTrace << frameFormat << '\n';

        for (int i = 0; i <= ((int)stVecMI.size()) - 1; i++)
        {
            outTrace << stVecMI[i].frameNb << '\t';

            imagePath = stPath + '/' + "capturedImages";

            imageName = imagePath + '/' + "frame" + '_' + to_string(stVecMI[i].frameNb) + ".png";

            capturedFrame = imread(imageName, 0);

            imagePath = stPath + '/' + rtPath + '/' + "decodedImages";

            imageName = imagePath + '/' + "frame" + '_' + to_string(stVecMI[i].frameNb) + ".png";

            decodedFrame = imread(imageName, 0);

            imagePath = stPath + '/' + "referenceImages";

            imageName = imagePath + '/' + "frame" + '_' + to_string(stVecMI[i].frameNb) + ".png";

            referenceFrame = imread(imageName, 0);

            rtVecMI[i].psnr = psnrEvaluate(capturedFrame, decodedFrame);

            rtMI.psnr += rtVecMI[i].psnr;

            rtVecMI[i].ssim = ssimEvaluate(capturedFrame, decodedFrame);

            rtMI.ssim += rtVecMI[i].ssim;

            stVecMI[i].psnr = psnrEvaluate(capturedFrame, referenceFrame);

            FI.refPSNR += stVecMI[i].psnr;

            stVecMI[i].ssim = ssimEvaluate(capturedFrame, referenceFrame);

            FI.refSSIM += stVecMI[i].ssim;

            outTrace << rtVecMI[i].psnr << '\t' << rtVecMI[i].ssim << '\t';

            if (stVecMI[i].pktLoss)
            {
                stMI.pktLoss += stVecMI[i].pktLoss;
                rtMI.pktLoss += rtVecMI[i].pktLoss;
                rtVecMI[i].pktLoss = ((stVecMI[i].pktLoss - rtVecMI[i].pktLoss) / stVecMI[i].pktLoss) * 100;
            }

            if (!(IP.encoder.compare("MMPEG")))
            {
                if (stVecMI[i].pktLossM)
                {
                    stMI.pktLossM += stVecMI[i].pktLossM;
                    rtMI.pktLossM += rtVecMI[i].pktLossM;
                    rtVecMI[i].pktLossM = ((stVecMI[i].pktLossM - rtVecMI[i].pktLossM) / stVecMI[i].pktLossM) * 100;
                    outTrace << rtVecMI[i].pktLossM << '\t';

                    for (int j = 0; j <= stVecMI[i].pktlossPriority.cols - 1; j++)
                    {
                        if (stVecMI[i].pktlossPriority.at<double>(0, j))
                        {
                            stMI.pktlossPriority.at<double>(0, j) += stVecMI[i].pktlossPriority.at<double>(0, j);
                            rtMI.pktlossPriority.at<double>(0, j) += rtVecMI[i].pktlossPriority.at<double>(0, j);
                            rtVecMI[i].pktlossPriority.at<double>(0, j) = ((stVecMI[i].pktlossPriority.at<double>(0, j) - rtVecMI[i].pktlossPriority.at<double>(0, j)) / stVecMI[i].pktlossPriority.at<double>(0, j)) * 100;
                            outTrace << rtVecMI[i].pktlossPriority.at<double>(0, j) << '\t';
                        }
                        else if (!(stVecMI[i].pktlossPriority.at<double>(0, j)))
                        {
                            outTrace << "na" << '\t';
                        }
                    }
                }

                if (stVecMI[i].pktLossD)
                {
                    stMI.pktLossD += stVecMI[i].pktLossD;
                    rtMI.pktLossD += rtVecMI[i].pktLossD;
                    rtVecMI[i].pktLossD = ((stVecMI[i].pktLossD - rtVecMI[i].pktLossD) / stVecMI[i].pktLossD) * 100;
                    outTrace << rtVecMI[i].pktLossD << '\t';

                    for (int j = 0; j <= stVecMI[i].pktlossPriority.cols - 1; j++)
                    {
                        if (stVecMI[i].pktlossPriority.at<double>(1, j))
                        {
                            stMI.pktlossPriority.at<double>(1, j) += stVecMI[i].pktlossPriority.at<double>(1, j);
                            rtMI.pktlossPriority.at<double>(1, j) += rtVecMI[i].pktlossPriority.at<double>(1, j);
                            rtVecMI[i].pktlossPriority.at<double>(1, j) = ((stVecMI[i].pktlossPriority.at<double>(1, j) - rtVecMI[i].pktlossPriority.at<double>(1, j)) / stVecMI[i].pktlossPriority.at<double>(1, j)) * 100;
                            outTrace << rtVecMI[i].pktlossPriority.at<double>(1, j) << '\t';
                        }
                        else if (!(stVecMI[i].pktlossPriority.at<double>(1, j)))
                        {
                            outTrace << "na" << '\t';
                        }
                    }
                }
            }

            outTrace << '\n';
        }

        outTrace.close();

        metricsName = stPath + '/' + rtPath + '/' + "mt" + '.' + IP.payloadFormat;

        outTrace.open(metricsName, ofstream::out);

        outTrace << metricsFormat << '\n';

        rtMI.psnr = rtMI.psnr / ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS)));

        FI.refPSNR = FI.refPSNR / ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS)));

        rtMI.ssim = rtMI.ssim / ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS)));

        FI.refSSIM = FI.refSSIM / ceil(IP.imagesNb / ceil(IP.imagesNb / (IP.videoDuration * IP.FPS)));

        outTrace << rtMI.psnr << '\t' << FI.refPSNR << '\t' << rtMI.ssim << '\t' << FI.refSSIM << '\t';

        if (stMI.pktLoss)
        {
            rtMI.pktLoss = ((stMI.pktLoss - rtMI.pktLoss) / stMI.pktLoss) * 100;
            outTrace << rtMI.pktLoss << '\t';
        }
        else if (!(stMI.pktLoss))
        {
            outTrace << "na" << '\t';
        }

        if (!(IP.encoder.compare("MMPEG")))
        {
            if (stMI.pktLossM)
            {
                rtMI.pktLossM = ((stMI.pktLossM - rtMI.pktLossM) / stMI.pktLossM) * 100;
                outTrace << rtMI.pktLossM << '\t';
            }
            else if (!(stMI.pktLossM))
            {
                outTrace << "na" << '\t';
            }

            if (stMI.pktLossD)
            {
                rtMI.pktLossD = ((stMI.pktLossD - rtMI.pktLossD) / stMI.pktLossD) * 100;
                outTrace << rtMI.pktLossD << '\t';
            }
            else if (!(stMI.pktLossD))
            {
                outTrace << "na" << '\t';
            }

            for (int i = 0; i <= stMI.pktlossPriority.cols - 1; i++)
            {
                if (stMI.pktlossPriority.at<double>(0, i))
                {
                    rtMI.pktlossPriority.at<double>(0, i) = ((stMI.pktlossPriority.at<double>(0, i) - rtMI.pktlossPriority.at<double>(0, i)) / stMI.pktlossPriority.at<double>(0, i)) * 100;
                    outTrace << rtMI.pktlossPriority.at<double>(0, i) << '\t';
                }
                else if (!(stMI.pktlossPriority.at<double>(0, i)))
                {
                    outTrace << "na" << '\t';
                }
            }

            for (int i = 0; i <= stMI.pktlossPriority.cols - 1; i++)
            {
                if (stMI.pktlossPriority.at<double>(1, i))
                {
                    rtMI.pktlossPriority.at<double>(1, i) = ((stMI.pktlossPriority.at<double>(1, i) - rtMI.pktlossPriority.at<double>(1, i)) / stMI.pktlossPriority.at<double>(1, i)) * 100;
                    outTrace << rtMI.pktlossPriority.at<double>(1, i) << '\t';
                }
                else if (!(stMI.pktlossPriority.at<double>(1, i)))
                {
                    outTrace << "na" << '\t';
                }
            }
        }

        outTrace << '\n';

        outTrace.close();
    }

    return EXIT_SUCCESS;
}

double psnrEvaluate(Mat_<double> capturedFrame, Mat_<double> decodedFrame)
{
    double mse, psnr;

    mse = mseEvaluate(capturedFrame, decodedFrame);

    if (mse != 0)
    {
        psnr = 10.0 * log10((255.0 * 255.0) / mse);
    }
    else if (mse == 0)
    {
        psnr = 60;
    }

    return psnr;
}

double ssimEvaluate(Mat_<double> capturedFrame, Mat_<double> decodedFrame)
{
    Mat I1, I2, I1_2, I2_2, I1_I2, mu1, mu2, mu1_2, mu2_2, mu1_mu2, sigma1_2, sigma2_2, sigma12, t1, t2, t3, ssim_map;
    Scalar mssim;
    double ssim, C1 = 6.5025, C2 = 58.5225;

    capturedFrame.convertTo(I1, CV_64F);
    decodedFrame.convertTo(I2, CV_64F);

    I1_2 = I1.mul(I1);
    I2_2 = I2.mul(I2);
    I1_I2 = I1.mul(I2);

    GaussianBlur(I1, mu1, Size(11, 11), 1.5);
    GaussianBlur(I2, mu2, Size(11, 11), 1.5);

    mu1_2 = mu1.mul(mu1);
    mu2_2 = mu2.mul(mu2);
    mu1_mu2 = mu1.mul(mu2);

    GaussianBlur(I1_2, sigma1_2, Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    GaussianBlur(I2_2, sigma2_2, Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    GaussianBlur(I1_I2, sigma12, Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    t1 = 2 * mu1_mu2 + C1;
    t2 = 2 * sigma12 + C2;
    t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);

    divide(t3, t1, ssim_map);

    mssim = mean(ssim_map);

    ssim = mssim(0);

    return ssim;
}

double ratioEvaluate(int capturedFrameSize, int decodedFrameSize)
{
    double ratio;

    ratio = (capturedFrameSize * 100.0) / decodedFrameSize;

    return ratio;
}

double mseEvaluate(Mat_<double> currentImage, Mat_<double> previousImage)
{
    double mse, error = 0;

    for (int i = 0; i <= currentImage.rows - 1; i++)
    {
        for (int j = 0; j <= currentImage.cols - 1; j++)
        {
            error = error + pow(currentImage.at<double>(i, j) - previousImage.at<double>(i, j), 2);
        }
    }

    mse = error / (currentImage.rows * currentImage.cols);

    return mse;
}

int frameZerosEvaluate(Mat_<double> frame)
{
    int frameZeros = 0;

    for (int i = 0; i <= frame.rows - 1; i++)
    {
        for (int j = 0; j <= frame.cols - 1; j++)
        {
            if (frame.at<double>(i, j) == 0)
            {
                frameZeros++;
            }
        }
    }

    return frameZeros;
}

int levelZerosEvaluate(Mat_<double> level)
{
    int levelZeros = 0;

    for (int i = 0; i <= level.cols - 1; i++)
    {
        if (level.at<double>(0, i) == 0)
        {
            levelZeros++;
        }
    }

    return levelZeros;
}
