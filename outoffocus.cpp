// Primary header
#include "windows.h"
#include <tchar.h>
#include <stdio.h>
#include "outoffocus.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include "BindingsOpenCV.h"
// DPTK headers
#include "Algorithm.h"
#include "Geometry.h"
#include "Global.h"
#include "Image.h"
#include "basedfunctions.h"
//#include "opencv2/photo.hpp"
//#include "OpenCVAdaptor.h"
// Poco header needed for the macros below 
#include <Poco/ClassLibrary.h>
#include <fstream>
#include <iostream>
#include <fstream>

using namespace std;
// This plugin is  developed by Caglar Senaras
// if you have any comments or feedbacks please contact: caglarsenaras@gmail.com
POCO_BEGIN_MANIFEST(sedeen::algorithm::AlgorithmBase)
	POCO_EXPORT_CLASS(sedeen::algorithm::outOfFocus)
	POCO_END_MANIFEST

	namespace sedeen {
		namespace algorithm {

			outOfFocus::outOfFocus()
				: 
				behavior_(),
				window_size_(),
				score_threshold_(),
				foreground_results_() {
			}

			outOfFocus::~outOfFocus() {
			}

			////////////////////////////////////
			// PLUGIN USERINTERFACE SETTINGS
			////////////////////////////////////
			void outOfFocus::init(const image::ImageHandle& image) {
				if (isNull(image)) return;

				window_size_ = createIntegerParameter(*this,
					"Sampling Distance",
					"Larger value means less sample but works faster",
					5,   // initial value
					2,   // minimum value
					15,
					false); 
				score_threshold_ = createIntegerParameter(*this,
					"out of focus threshold",
					"out of focus threshold",
					40,   // initial value
					10,   // minimum value
					70,
					false); 
				// Init compare options
				std::vector<std::string> compare_options;
				compare_options.push_back("Used cached information");
				compare_options.push_back("Ignore cache");
				retainment_ = 
					createOptionParameter(*this,            // Algorithm - to be bound to UI
					"Cache", // Widget label
					"If you prefer to use cache, the algorithm will not read the whole image again. Useful for changing threshold value",  // Widget tooltip
					0,                // Initial selection
					compare_options,
					false); // List of options


				image_result_ = createImageResult(*this, "Image result");
			}



			//In order to save intermediate results we use that function
			// filename: window size+ filesize+ plugin version + filename + ".png"
			String outOfFocus::getUniqueFilename(){
				TCHAR buf [MAX_PATH];
				GetTempPathA (MAX_PATH, buf);
				std::string tempPath(buf); //get windows temp path.

				std::string path_to_image = image()->getMetaData()->get(image::StringTags::SOURCE_DESCRIPTION, 0);
				auto found1 = path_to_image.find_last_of("\\")+1;
				if (found1==0) {
					found1 = path_to_image.find_last_of("/")+1;
				}

				auto found2 = path_to_image.find_last_of(".");
				std::string  m_path_to_root = path_to_image.substr(found1);
				std::ifstream in(path_to_image, std::ifstream::ate | std::ifstream::binary);
				int sizeOfFile= in.tellg();  //will get filesize 
				std::string sizeString = std::to_string(sizeOfFile);
				std::string finalFileName= tempPath +  std::to_string(window_size_)+ sizeString+ "v2" + m_path_to_root+ ".png"; // save as png.

				return finalFileName;
			}

			//Since we need to detect tissue, we resize the image 1/64, 
			//convert it to grayscale and apply a  thresholding. 
			cv::Mat outOfFocus::calcTissueMask(const sedeen::image::ImageHandle& image, logger &mylogger, int sampleSize){

				auto source_factory = image->getFactory();
				auto compositor = image::tile::Compositor(source_factory) ;
				auto szInitial = compositor.getDimensions(0);
				auto reducedSize= szInitial*(1.0/sampleSize); //requested new dimension
				auto pixelRect=	Rect (Point(0,0),Size(szInitial.width(),szInitial.height()));
				auto SmallImg = compositor.getImage( pixelRect, reducedSize) ;
				auto lowResMat = image::toOpenCV(SmallImg, false);

				//lowResMat.convertTo(lowResMat, CV_32F);
				//extract tissue map
				mylogger.print("f1- convert to gray: band"+ std::to_string(lowResMat.channels()) + "\n",1);
				mylogger.print("type"+ std::to_string(lowResMat.type()) + "\n",1);
				Mat grayImg;
				cvtColor(lowResMat, grayImg, cv::COLOR_RGB2GRAY);	
				grayImg.convertTo(grayImg,CV_8U);
				Mat otsuR;
				//apply a bluring function before OTSU 
				GaussianBlur (grayImg,grayImg,cv::Size(9,9),0,0);
				auto selectedThreshold=cv::threshold(grayImg, otsuR, 220,  255, THRESH_BINARY | CV_THRESH_OTSU  );
				mylogger.print("f1- Otsu threshold Val: " + std::to_string( selectedThreshold)+"\n");
				if (selectedThreshold<220)  cv::threshold(grayImg, otsuR, 220,  255, THRESH_BINARY  );
				//our tissue mask is otsuR
				mylogger.print("f1- Tissue detected.\n");

				return  otsuR;
			}

			cv::Mat outOfFocus::calculateFocusMap(const sedeen::image::ImageHandle& image, Mat &tissueMask,int sampleSize,int window_size_ ,logger &mylogger){
				auto source_factory = image->getFactory();
				auto compositor = image::tile::Compositor(source_factory) ;
				auto width= tissueMask.size().width;	
				auto height= tissueMask.size().height;	
				segmentAlgo myFocusDetector;
				Mat stdFltResult;
				int subsample=3;// StdMaxVals= subsample*subsample
				double StdMaxVals[9];//StdMaxVals= subsample*subsample
				int subRectCounter;
				double min, max;
				double tileVal;
				Mat resultMap= Mat::zeros (height,width,CV_8UC1);
				cv::namedWindow("Analyzing",cv::WINDOW_NORMAL);
				cv::resizeWindow("Analyzing", (200*width/height),200);
				for (int i=0;i<width;i=i+window_size_){ //window_size_ is our step size
					for (int j=0;j<height;j=j+window_size_){
						int intensity = tissueMask.at<uchar>(j, i); // intensity will be 0 for tissue
						if (intensity==0){
							auto pixelRect=	Rect (Point(i*sampleSize,j*sampleSize),Size(sampleSize,sampleSize));
									auto PixVal = compositor.getImage(0, pixelRect);
									mylogger.print("b" ,1);
									auto mat = image::toOpenCV(PixVal,true);
									Scalar meanVal=cv::mean(mat);

									if (meanVal.val[0]<225){ //  tissue control for Block
										Mat RedBand;
										myFocusDetector.getRedBand(mat, RedBand);
										myFocusDetector.stdFilt(RedBand, stdFltResult);

										subRectCounter=0;
										int subSize=floor(RedBand.cols/subsample);
										//devide the  sample into n sample, calculate the median of max values of each small tiles.
										mylogger.print("c" ,1);
										for (int si=0;si<subsample;si++){
											for (int sj=0;sj<subsample;sj++){
												cv::minMaxLoc(stdFltResult(cv::Rect(sj*subSize,si*subSize,subSize, subSize)) , &min, &max);
												StdMaxVals[subRectCounter]=max;
												subRectCounter++;
											}
										}
										mylogger.print("d" ,1);
										tileVal= ceil (myFocusDetector.GetMedian(StdMaxVals,subRectCounter));
										cout<<tileVal<<endl;
										resultMap.at<uchar>(j, i)=tileVal;
									}else{
										tissueMask.at<uchar>(j, i)=255;
									}
						}

						cv::imshow("Analyzing",resultMap>0);
						cvWaitKey(1);
					}
				}
				return resultMap;
			}

			// Aims to visualize
			image::RawImage outOfFocus::createMask(cv::Mat finalMap, sedeen::Size sz, logger &mylogger){
				mylogger.print("final visual mask generation:\n");
				image::RawImage mask(sz, Color(ColorSpace::RGBA_8));
				mask.fill(0);
				unsigned char intensityA=255;

				int thresholdVal=score_threshold_; 
				auto width= finalMap.size().width;	
				auto height= finalMap.size().height;	
				unsigned char  RedVals [8]={255,180,120,80,0,0,0,0};
				unsigned char  GreenVals [8]={0,0,0,0,80,180,220,255};
				int newVal;
				for (int i=0;i<width;i=i+window_size_){ //window_size_ is our step size
					for (int j=0;j<height;j=j+window_size_){
						int intensity = finalMap.at<uchar>(j, i); // intensity will be 0 for tissue
						if (intensity<255){
							if (intensity<thresholdVal){
								newVal= 4-  4*double(thresholdVal-intensity)/(thresholdVal-9);
							}else{
								newVal=4+ 3*double(intensity-thresholdVal) /(80-thresholdVal+0.1);
								if (newVal>7) newVal=7;
							}
							mask.setValue(i,j,0,RedVals[newVal]);
							mask.setValue(i,j,1,GreenVals[newVal]); 
							mask.setValue(i,j,3,intensityA);
						
						}
					}
				}
				mylogger.print("Done\n");
				return mask;
			}


			void outOfFocus::run() {
				int debugMODE=2;  //debugMODE= 0 >no log file  //debugMODE= 1 >brief log //debugMODE= 2 >detailed log

				int sampleSize=512*2;
				std:String outputfile=getUniqueFilename();
				logger mylogger;
				mylogger.setDebugMode(debugMODE);
				mylogger.setFilename(outputfile);
				mylogger.print("Get factory.\n");

				auto source_factory = image()->getFactory();
				auto source_color = source_factory->getColor();
				auto compositor = image::tile::Compositor(source_factory) ;
				auto szInitial = compositor.getDimensions(0);
				auto reducedSize= szInitial*(1.0/sampleSize); //requested new dimension
				double maxRes= getMaximumMagnification(image());


				Mat finalHeatMap = imread(outputfile, 0);   // Read the intermediate result file

				// Check for invalid input or retainment_ (request for using existing cache file)
				if ((!finalHeatMap.data) ||  1==retainment_   )  
				{
					mylogger.print("f0 - running without cache..\n");
					Mat tissueMask=calcTissueMask(image(), mylogger, sampleSize);
					auto szInitial = compositor.getDimensions(0);
					auto sz=szInitial;
					mylogger.print("f0 - Get maxlevelID.\n");
					Mat heatmap= calculateFocusMap(image(),tissueMask,sampleSize,window_size_, mylogger);
					heatmap.copyTo(finalHeatMap,tissueMask==0);
					finalHeatMap=finalHeatMap+tissueMask;
					mylogger.print("f0 - saving to  cache:");
					imwrite(outputfile,finalHeatMap); //write intermediate result for further usage.
					mylogger.print("f0 - OK\n");
				}
				else
				{
					mylogger.print(" cache used\n");

				}

				auto largest_region = source_factory->getLevelRegion(0);
				auto largest_size = size(largest_region);
				auto mask=createMask(finalHeatMap,reducedSize,mylogger);


				image_result_.update(mask, largest_region);

				cv::destroyAllWindows();
				mylogger.print("Done.\n");
				mylogger.close();

			}





		} // namespace algorithm
} // namespace sedeen

