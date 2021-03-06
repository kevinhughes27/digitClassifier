// digitClassifier
//
// Kevin Hughes
//
// 2011
//
// classifies written digits (0-9)
// using a Neural Network

#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/ml/ml.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace cv;
using namespace std;

void loadData(const string fileList, Mat &X, Mat &y);
vector<Rect> segmentDigits(Mat &img);

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        cout << "Usage: " << argv[0] << "<training file> <testing file>" << endl;
        return -1;
    }

    // collect arguements
    const string train_path = argv[1];
    const string test_path = argv[2];

    // Train
    Mat X;
    Mat y;
    loadData(train_path,X,y);

    CvANN_MLP_TrainParams NNparams = CvANN_MLP_TrainParams();
        NNparams.term_crit = cvTermCriteria( CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 1000, 0.1 );
        NNparams.train_method = CvANN_MLP_TrainParams::RPROP;
        NNparams.bp_dw_scale = 0.1;
        NNparams.bp_moment_scale = 0.1;
        NNparams.rp_dw0 = 0.01;
        NNparams.rp_dw_plus = 1.2;
        NNparams.rp_dw_minus = 0.5;
        NNparams.rp_dw_min = FLT_EPSILON;
        NNparams.rp_dw_max = 50.;

    CvANN_MLP *NeuralNet = new CvANN_MLP();

    Mat layerSizes( 1, 4, CV_32SC1 );
    layerSizes.at<int>(0,0) = (int)X.cols;
    layerSizes.at<int>(0,1) = (int)(X.cols * .75);
    layerSizes.at<int>(0,2) = (int)(X.cols * .25);
    layerSizes.at<int>(0,3) = (int)y.cols;


    NeuralNet->create( layerSizes, CvANN_MLP::SIGMOID_SYM, 0, 0);

    cv::Mat sampleWeights;

    NeuralNet->train( X, y, sampleWeights, Mat(), NNparams );

    // Test on training data
    int misses = 0;
    for(int i = 0; i < (int)X.rows; i++)
    {
        Mat output;
        NeuralNet->predict( X.row(i), output );

        //cout << format(output,"numpy") << endl;

        Point t; minMaxLoc( y.row(i), NULL, NULL, NULL, &t ); int truth = t.x;
        Point p; minMaxLoc( output, NULL, NULL, NULL, &p ); int prediction = p.x;

        //cout << " Truth = " << truth << " Prediction = " << prediction << endl;

        if( truth != prediction) misses++;
    }

    float percent_correct = (float)(X.rows-misses) / X.rows * 100;
    cout << " Train Set Performance = " << percent_correct << "% correct" << endl;

    // Test
    Mat Xtest;
    Mat ytest;
    loadData(test_path,Xtest,ytest);

    // Test on new data
    misses = 0;
    for(int i = 0; i < (int)Xtest.rows; i++)
    {
        Mat output;
        NeuralNet->predict( Xtest.row(i), output );

        //cout << format(output,"numpy") << endl;

        Point t; minMaxLoc( ytest.row(i), NULL, NULL, NULL, &t ); int truth = t.x;
        Point p; minMaxLoc( output, NULL, NULL, NULL, &p ); int prediction = p.x;

        //cout << " Truth = " << truth << " Prediction = " << prediction << endl;

        if( truth != prediction) misses++;
    }

    percent_correct = (float)(ytest.rows-misses) / ytest.rows * 100;
    cout << " Test Set Performance = " << percent_correct << "% correct" << endl;

    return 0;
}


void loadData(const string filename, Mat &X, Mat &y)
{
    // load image paths from file
    ifstream file;
    vector<string> image_paths;
    vector<int> digit_truth;

    file.open(filename.c_str(), ifstream::in);
    if(file.is_open())
    {
        string line;
        while(getline(file, line))
        {
            string id(line.begin(), line.begin() + 1);
            string imagePath(line.begin() + 2,line.end());

            digit_truth.push_back(atoi(id.c_str()));
            image_paths.push_back(imagePath);
        }
    }
    else
    {
        cout << "unable to open: " << filename << endl;
    }
    file.close();

    // Clear return Mats
    X = Mat();
    y = Mat();

    // Loop through all Images
    for(int i = 0; i < (int)image_paths.size(); i++)
    {
        Mat image = imread(image_paths[i].c_str(),0);
        //imshow("image",image);
        //waitKey();

        vector<Rect> boundingBoxes = segmentDigits(image);

        // Loop through each digit
        for(int j = 0; j < (int)boundingBoxes.size(); j++)
        {
            Mat digit = image(boundingBoxes[j]).clone();
            threshold(digit,digit,180,255, THRESH_BINARY);
            resize(digit, digit, Size(15,15), 0, 0, INTER_LINEAR);
            digit.convertTo(digit, CV_32FC1);

            //imshow("digit", digit);
            //waitKey();

            //cout << digit.reshape(0,1).rows << "  " << digit.reshape(0,1).cols << endl;
            X.push_back(digit.reshape(0,1));

            Mat label = Mat::zeros(1,10, CV_32FC1);
            label.at<float>(0,digit_truth[i]) = 1;

            y.push_back(label);
        }
    }
}


vector<Rect> segmentDigits(Mat &img)
{
    // Threshold
    Mat bw;
    threshold(img,bw,180,255, THRESH_BINARY);
    //imshow( "Thresholded Image", bw);
    //waitKey();

    // logic might seem flipped here but its because it thinks white is "on" instead of black
    erode(bw,bw, Mat());
    erode(bw,bw, Mat());
    erode(bw,bw, Mat());
    erode(bw,bw, Mat());
    erode(bw,bw, Mat());
    dilate(bw,bw, Mat());
    dilate(bw,bw, Mat());
    dilate(bw,bw, Mat());
    dilate(bw,bw, Mat());
    dilate(bw,bw, Mat());

    // Find Contours
    vector<vector<Point> > contours;
    findContours(bw, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

    //print number of contours found
    //cout << (int)contours.size() << " contours found." << endl;

    // Draw contours
    Mat dst = Mat::zeros(img.size(), CV_8UC3);
    for(int idx = 0; idx < (int)contours.size()-1; idx++)
    {
        Scalar color((rand()&255), (rand()&255), (rand()&255));
        drawContours(dst, contours, idx, color, CV_FILLED, 8);
    }
    //imshow( "Connected Components", dst );
    //waitKey();

    // Get Bounding Boxes
    vector<Rect> boundingBoxes;
    for(int i = 0; i < (int)contours.size()-1; i++)
    {
        Rect r = boundingRect(contours[i]);
        r.x = r.x - 1;
        r.y = r.y - 1;
        r.width = r.width + 2;
        r.height = r.height + 2;
        boundingBoxes.push_back(r);
    }

    // Draw the Bounding Boxes
    Mat dst2 = Mat::zeros(img.size(), CV_8UC3);
    cvtColor(img, dst2, CV_GRAY2BGR);
    for(int i = 0; i < (int)boundingBoxes.size(); i++)
    {
        rectangle(dst2, boundingBoxes[i], Scalar(255,0,0));
    }

    //imshow( "Bounding Boxes", dst2 );
    //waitKey();

    return boundingBoxes;
}



