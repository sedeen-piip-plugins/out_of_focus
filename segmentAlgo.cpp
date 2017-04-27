#include "stdafx.h"
#include "segmentAlgo.h"

	using namespace cv;
	using namespace std;
segmentAlgo::segmentAlgo()
{
}


segmentAlgo::~segmentAlgo()
{
}


double segmentAlgo::GetMedian(double daArray[], int iSize) {
	// Allocate an array of the same size and sort it.
	double* dpSorted = new double[iSize];
	for (int i = 0; i < iSize; ++i) {
		dpSorted[i] = daArray[i];
	}
	for (int i = iSize - 1; i > 0; --i) {
		for (int j = 0; j < i; ++j) {
			if (dpSorted[j] > dpSorted[j+1]) {
				double dTemp = dpSorted[j];
				dpSorted[j] = dpSorted[j+1];
				dpSorted[j+1] = dTemp;
			}
		}
	}

	// Middle or average of middle values in the sorted array.
	double dMedian = 0.0;
	if ((iSize % 2) == 0) {
		dMedian = (dpSorted[iSize/2] + dpSorted[(iSize/2) - 1])/2.0;
	} else {
		dMedian = dpSorted[iSize/2];
	}
	delete [] dpSorted;
	return dMedian;
}

void segmentAlgo::stdFilt(const Mat &image, cv::Mat &stdFiltMask){
	cv::Mat image32f;
	image.convertTo(image32f, CV_32F);

	Mat mu;
	blur(image32f, mu, Size(3, 3));

	Mat mu2;
	blur(image32f.mul(image32f), mu2, Size(3, 3));

	cv::sqrt(mu2 - mu.mul(mu), stdFiltMask);
}

void segmentAlgo::getRedBand(Mat &mat, cv::Mat &RedBand){
	Mat *channel;
	if (mat.channels()==3) { //check these cases in the future RGB
		channel= new Mat [3];
		cv::split(mat, channel);
		RedBand=channel[0].clone();
	}

	if (mat.channels()==4) { //check these cases in the future  RGBA?
		channel= new Mat [4];
		cv::split(mat, channel);
		RedBand=channel[0].clone();
		channel[3].release();
	}
	channel[0].release();
	channel[1].release();
	channel[2].release();
	delete []channel;

}