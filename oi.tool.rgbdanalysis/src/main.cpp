#include <iostream>
#include <fstream>
#include <string>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include "OICore.hpp"

using namespace std;
using namespace cv;

using namespace oi::core;
using namespace oi::core::recording;

char window_name1[] = "Unprocessed Image";

std::string fileName = "R20171123155139.oi.bidx";
std::string dir = "/Users/jan/Desktop/ImmObsData/";

std::vector<Mat> readCustomMjpeg(std::string path, int flags) {
    std::vector<Mat> res;
    std::ifstream _reader;
    _reader.open(path, std::ios::binary | std::ios::in);
    unsigned int jpeg_size;
    while (!_reader.eof()) {
        _reader.read(reinterpret_cast<char *>(&jpeg_size), sizeof(jpeg_size));
        if (_reader.eof()) break;
        std::vector<double> jpgbytes(jpeg_size);
        _reader.read(reinterpret_cast<char*>(jpgbytes.data()), jpeg_size);
        Mat frame = imdecode(jpgbytes, flags);
        res.push_back(frame);
    }
    _reader.clear();
    _reader.close();
    return res;
}

int main( int argc, char** argv ) {
    std::string path = dir+fileName;
    std::cout << "File: " << path << std::endl;
    vector<Mat> frames = readCustomMjpeg(path, CV_LOAD_IMAGE_GRAYSCALE); // CV_LOAD_IMAGE_COLOR
    int current = 0;
    
    namedWindow( window_name1, WINDOW_AUTOSIZE );
    while (current < frames.size()) {
        imshow(window_name1, frames[current]);
        if (waitKey(1) == (char)27) break;
        current++;
    }
    waitKey(0);
    destroyAllWindows();
    return 0;
}
