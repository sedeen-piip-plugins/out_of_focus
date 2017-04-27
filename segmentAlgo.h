#pragma once
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include <queue>




class segmentAlgo
{

public:
	segmentAlgo();
	~segmentAlgo();
	void start(cv::Mat&);
	void setImage(cv::Mat);



	void histEq3band(cv::Mat, cv::Mat&);
	void createOSUimage(cv::Mat, cv::Mat&);
	void getInitialCellNuclear(cv::Mat, cv::Mat&);
	void sizeAnalyze(cv::Mat&);
	double adaptiveThreshold(cv::Mat);
	void non_maxima_suppression(const cv::Mat & src, cv::Mat & mask, const bool remove_plateaus);
	void bwareaopen(cv::Mat&, double);
	void bwareaopenWithFillHoles(cv::Mat & im, double size);
	void imhmin(cv::Mat&, double);
	double GetMedian(double daArray[], int iSize) ;
	void stdFilt(const cv::Mat &image, cv::Mat &stdFiltMask);
	void getRedBand(cv::Mat &mat, cv::Mat &RedBand);
	cv::Mat imreconstruct(const cv::Mat&, const cv::Mat&, int);
	cv::Mat_<int> watershedV2(const cv::Mat & origImage, const cv::Mat_<float>& image, int connectivity);
	void localMaxima(cv::Mat src, cv::Mat & dst, int squareSize);
	void bwlabel(cv::Mat binaryImage, cv::Mat & labels);
	void bwlabel2(cv::Mat & binaryImage, cv::Mat & labels);
private:
	template <typename T>
	inline void propagate(const cv::Mat& image, cv::Mat& output, std::queue<int>& xQ, std::queue<int>& yQ,
		int x, int y, T* iPtr, T* oPtr, const T& pval) {

		T qval = oPtr[x];
		T ival = iPtr[x];
		if ((qval < pval) && (ival != qval)) {
			oPtr[x] = min(pval, ival);
			xQ.push(x);
			yQ.push(y);
		}
	}
	int cols;
	int rows;
	cv::Mat originalImage;
	cv::Mat outputImage;
};

