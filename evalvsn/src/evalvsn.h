#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <fstream>
#include <sys/stat.h>
#include <set>
#include <regex>
#include <filesystem>

using namespace std;
using namespace cv;

struct inputParameters
{
    string simId;
    string encoder;
    string imageResolution;
    int imagesNb;
    int videoDuration;
    double FPS;
    int threshold;
    double gopCoef;
    double qualityCoef;
    int priorityNb;
    string DCT;
    int zoneSize;
    string payloadFormat;
    string rleLevel;
    string binLevel;
    string rcvFile;
    // Encoder=JPEG, the Pham witness (tranche 3b-2): the quality factor of
    // the ucam-II encoder and the packet size it packetises to.
    int qualityFactor = 0;
    int mss = 0;
    // The packet payload in bytes, header included (-mpl, 74 by default):
    // the encoder cuts each priority level into packets of this size.
    int mpl = 74;
};

struct prioritiesInfo
{
    double sendTime;
    int frameNb;
    string frameType;
    int dataPriority;
    int dataSize;
    string data;
    int blockNb;
    Mat_<double> level;
    vector<int> levelSize;
    vector<int> levelOffset;
};

struct framesInfo
{
    int frameNb;
    string frameType;
    int frameSize;
    double refPSNR = 0;
    double refSSIM = 0;
    double compRatio = 0;
};

struct packetsInfo
{
    double sTime;
    int seqNb;
    int pSize;
    int frameNb;
    string frameType;
    int prioNb;
    int offset;
};

struct metricsInfo
{
    int frameNb;
    double psnr = 0;
    double ssim = 0;
    double pktLoss = 0;
    double pktLossM = 0;
    double pktLossD = 0;
    Mat_<double> pktlossPriority = Mat::zeros(2, 13, CV_64F);
};

bool isValidSimId(const string &simId);
bool isValidInt(const string &s);
bool isValidDouble(const string &s);
bool isValidDCT(const string &dct);
bool isValidZoneSize(const string &zs);
bool isValidRleLevel(const string &rle);
bool isValidBinLevel(const string &bl);
bool isValidPriorityNb(const string &pn);
bool getIP(inputParameters &IP, const string &line, string &errParam);

void mkDir(string pathName);

void avi2png(inputParameters IP, string videoFile, string outputDirectory);

int videoEncoder(inputParameters IP, string videoFile, string outputDirectory);
vector<prioritiesInfo> frameEncoder(inputParameters IP, Mat_<double> encodedFrame, prioritiesInfo PI);
Mat_<double> dctEncoder(inputParameters IP, Mat_<double> block);
Mat_<double> claEncoder(Mat_<double> block);
Mat_<double> llmEncoder(inputParameters IP, Mat_<double> block);
Mat_<double> binEncoder(inputParameters IP, Mat_<double> block);
Mat_<double> quantificationEncoder(inputParameters IP, Mat_<double> blockDCT);
Mat_<double> zigzagEncoder(Mat_<double> blockQTF);
vector<prioritiesInfo> priorityEncoder(inputParameters IP, Mat_<double> blockZigZag, vector<prioritiesInfo> vecPI, prioritiesInfo PI);
prioritiesInfo rleEncoder(prioritiesInfo PI);
prioritiesInfo binaryEncoder(inputParameters IP, prioritiesInfo PI);
prioritiesInfo staticEncoder(inputParameters IP, prioritiesInfo PI);
prioritiesInfo expGolombEncoder(prioritiesInfo PI);
double negativeEncoder(double dec);
string unaryEncoder(double ne);
double getGroupID(double ne);
string dec2bin(double dec, int bits);
prioritiesInfo hexEncoder(prioritiesInfo PI);
void traceEncoder(inputParameters IP, vector<prioritiesInfo> vecPI, vector<framesInfo> vecFI, string priorityName, string frameName);

vector<packetsInfo> lostPackets(inputParameters IP, string stPacketName, string rtPacketName);
vector<prioritiesInfo> traceDecoder(inputParameters IP, string unitsName, bool received);
vector<string> packetEncoder(inputParameters IP, prioritiesInfo PI, Mat_<double> elems, bool rle);
Mat_<double> packetDecoder(inputParameters IP, int dataPriority, const vector<string> &units);
prioritiesInfo hexDecoder(prioritiesInfo PI);
prioritiesInfo binaryDecoder(inputParameters IP, prioritiesInfo PI);
prioritiesInfo staticDecoder(inputParameters IP, prioritiesInfo PI);
prioritiesInfo expGolombDecoder(prioritiesInfo PI);
double unaryDecoder(string ue);
double negativeDecoder(double ne);
double bin2dec(string bin, int bits);
prioritiesInfo rleDecoder(prioritiesInfo PI);
vector<prioritiesInfo> priorityDecoder(inputParameters IP, vector<prioritiesInfo> &levelsInfo, prioritiesInfo PI);  // by reference: the vector grows to 60 000 entries, copying it per level was quadratic
Mat_<double> zigzagDecoder(Mat_<double> blockZigZag, int vmax, int hmax);
Mat_<double> quantificationDecoder(inputParameters IP, Mat_<double> blockQTF);
Mat_<double> dctDecoder(inputParameters IP, Mat_<double> blockDCT);
Mat_<double> claDecoder(Mat_<double> blockDCT);
Mat_<double> llmDecoder(Mat_<double> blockDCT);
Mat_<double> binDecoder(Mat_<double> blockDCT);
Mat_<double> frameDecoder(inputParameters IP, Mat_<double> decodedFrame);
int videoDecoder(inputParameters IP, string videoFile, string outputDirectory);

int traceEvaluate(inputParameters IP, string videoFile, string outputDirectory);
double psnrEvaluate(Mat_<double> capturedFrame, Mat_<double> decodedFrame);
double ssimEvaluate(Mat_<double> capturedFrame, Mat_<double> decodedFrame);
double ratioEvaluate(int capturedFrameSize, int decodedFrameSize);
double mseEvaluate(Mat_<double> currentImage, Mat_<double> previousImage);
int frameZerosEvaluate(Mat_<double> frame);
int levelZerosEvaluate(Mat_<double> level);

void png2avi(inputParameters IP, string videoFile, string outputDirectory);

// The Pham witness, pham.cc: the ported ucam-II JPEG encoder and the
// untouched reference decoder, driven as external programs.
string phamDir();
void writeBmp8(const string &path, const Mat &gray);
vector<string> phamEncodeFrame(inputParameters IP, const Mat &gray, const string &workDir, int frameNb, int &frameBytes);
int phamTraceEncoder(inputParameters IP, const vector<string> &packets, double sendTime, int frameNb, const string &frameType, int frameBytes, const string &priorityName, const string &frameName);
int phamVideoDecoder(inputParameters IP, const string &stPath, const string &rtPath, const string &priorityName, const string &stPacketName, const vector<packetsInfo> &vecPK, int frames, int height, int width);
