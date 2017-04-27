// Primary header
#include "windows.h"
#include <tchar.h>
#include <stdio.h>
#include "segmentation.h"

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
#include "segmentAlgo.h"
//#include "opencv2/photo.hpp"
//#include "OpenCVAdaptor.h"
// Poco header needed for the macros below 
#include <Poco/ClassLibrary.h>
#include <fstream>
#include <iostream>
#include <fstream>
using namespace std;

/*********************************************************************************
**********************************************************************************
	Out of focus detection is a plugin for PathCore Sedeen Viewer.
	This plugin has been developed by Dr. Caglar Senaras
	in Dr. Metin Gurcan's lab at the Ohio State University.
	
	This is a part of pathology image informatics platform (PIIP)
	funded by NIH.
	
	Contact info: caglarsenaras@gmail.com
	Date of the release of the code: April 27, 2017
	
	TODO:
	-update comments
	-create a better log mechnanism.
	-create visualization function.

**********************************************************************************
**********************************************************************************/


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
			// filename: filesize + filename + ".png"
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
			   std::string finalFileName= tempPath + sizeString+ m_path_to_root+ ".png"; // save as png.

			   return finalFileName;
			}

			void outOfFocus::run() {
				int Debug=1;
				ofstream myfile;
				std:String outputfile=getUniqueFilename();
				if (Debug) {
					myfile.open (outputfile + "_log.txt");
				}


				if (Debug) {
					myfile << "Get factory.\n";
				}
				auto source_factory = image()->getFactory();
				auto source_color = source_factory->getColor();
				auto compositor = image::tile::Compositor(source_factory) ;
				double maxRes= getMaximumMagnification(image());
				auto szInitial = compositor.getDimensions(0);
				auto sz=szInitial;
				if (Debug) {
					myfile << "Get maxlevelID.";
				}
				int maxLevelID=getNumResolutionLevels	(image());
				double * resolutions;
				if (Debug) {
					myfile << maxLevelID << "\n";
				}
				resolutions=new double [maxLevelID] ;
				resolutions[0]=maxRes;
				int optimiumScale=-1;
				double optimumScaleRatio=1.0/64.0;
				double downscaleRatio=1.0/4.0;

				//Extract The scale information in other layers. Detect most suitable layer for tissue detection.
				for (int i=1;i<maxLevelID;i++){
					sz = compositor.getDimensions(i);
					resolutions[i]=maxRes * double(sz.height()) / double(szInitial.height()) ;

					if (sz.height()<4000 && sz.height()>500){  // resolutions[i]>0.5
						optimiumScale=i;
						optimumScaleRatio= double(sz.height()) / double(szInitial.height()) ;
					}
				}

				if (Debug) {
					myfile <<"Optimal Scale and Rat:" << optimiumScale<< "  " <<optimumScaleRatio << "\n";
				}
				sz = compositor.getDimensions(optimiumScale);
				Mat heatMap2;	
				
				//std:String outputfile=getUniqueFilename();
	
				if (Debug) {
					myfile <<"Checking Cache:\n";
				}
				heatMap2 = imread(outputfile, 0);   // Read the intermediate result file
				
				if((!heatMap2.data) ||  1==retainment_   )  // Check for invalid input
				{
					if (Debug) {
					myfile <<"No Cache --- retainment\n";
					}
					cv::namedWindow("Scanning",cv::WINDOW_AUTOSIZE);
					int shiftSize=512*window_size_;
					int sampleSize=512*2;
					int bgTreshold=700;
					segmentAlgo myFocusDetector;

					cv::Mat imageOpenCV;

								Mat summedMat;
					// 			
					//Tissue detection part
					//
					if (optimiumScale>-1){  //We have a layer to anaylze the tissue!

						if (Debug) {
						myfile <<"Get image and read  as opencv object\n";
						}
						auto pixelRect=	Rect (Point(0,0),Size(sz.width(),sz.height()));
						auto SmallImg = compositor.getImage(optimiumScale, pixelRect);
						//auto lowResMat = image::toOpenCVMat(SmallImg,true);
						auto lowResMat = image::toOpenCV(SmallImg, false);
						if (Debug) {
						myfile <<"convert to CV_32F\n";
						}
						lowResMat.convertTo(lowResMat, CV_32F);
						//extract tissue map
						Mat *channel;
						channel= new Mat [lowResMat.channels()];
						cv::split(lowResMat, channel);
						//calculare r+g+b for each pixel
						summedMat=channel[0]+channel[1]+channel[2];

						double min,max;
						cv::minMaxLoc(summedMat, &min, &max);//can be deleted

						Mat otsuR;
						Mat summedMat8=summedMat/3;
						summedMat8.convertTo(summedMat8,CV_8U);
						Mat summedMat8S;
						//apply a bluring function before OTSU 
						GaussianBlur (summedMat8,summedMat8S,cv::Size(9,9),0,0);
						cv::threshold(summedMat8S, otsuR, 50,  255, THRESH_BINARY | CV_THRESH_OTSU  );
						//our tissue mask is summedMat
						if (Debug) {
							myfile <<"Tissue detected.\n";
						}
						summedMat=otsuR;

					}else { 
						// We need to give an error!!! We couldnt find any suitable resized image
						if (Debug) {
							myfile <<"optimiumScale is lower than -1\n";
						}
						summedMat=cv::Scalar(255); 

					}

					Mat stdFltResult;
					int subsample=3;// StdMaxVals= subsample*subsample
					double StdMaxVals[9];//StdMaxVals= subsample*subsample
					int subRectCounter;
					double min, max;

					Mat resultMap= Mat (sz.height()*downscaleRatio,sz.width()*downscaleRatio,CV_8UC1);
					Mat dataMap= Mat (sz.height()*downscaleRatio,sz.width()*downscaleRatio,CV_8UC1);

					resultMap=cv::Scalar(255);
					dataMap=cv::Scalar(255);

					Mat smallMap;
					double tileVal;
					// Aim of this loop: we get samples from the tissue and calculate the stdfilt for each sample. 
					//not: resultMap is a resized result map. we enlarge it after the loop.
					if (Debug) {
							myfile <<"reading Small tiles and identify out of focus regions:... ";
						}
					for (int i=0;i< compositor.getDimensions(0).height()-shiftSize-1;i=i+shiftSize)
						for (int j=0;j< compositor.getDimensions(0).width()-shiftSize-1;j=j+shiftSize){

							int si=i*optimumScaleRatio;
							int sj=j*optimumScaleRatio;
							int intensity = summedMat.at<uchar>(si,sj);

							if (intensity<255){
								auto pixelRect=	Rect (Point(j,i),Size(sampleSize,sampleSize));
								auto PixVal = compositor.getImage(0, pixelRect);
								auto mat = image::toOpenCV(PixVal,true);

								Mat RedBand;
								myFocusDetector.getRedBand(mat, RedBand);
								myFocusDetector.stdFilt(RedBand, stdFltResult);

								subRectCounter=0;
								//devide the  sample into n sample, calculate the median of max values of each small tiles.
							
								for (int si=0;si< (RedBand.rows-1);si=si+RedBand.rows/subsample){
									for (int sj=0;sj< (RedBand.cols-1);sj=sj+RedBand.cols/subsample){
										cv::minMaxLoc(stdFltResult(cv::Rect(sj,si,floor(RedBand.cols/subsample), floor(RedBand.rows/subsample))) , &min, &max);
										StdMaxVals[subRectCounter]=max;
										subRectCounter++;
									}
								}
								tileVal= ceil (myFocusDetector.GetMedian(StdMaxVals,subRectCounter));
								//todo: skyler will put tileVal values into a vector
								//put these informations to resultmap
								Mat roi(resultMap, cv::Rect(j*optimumScaleRatio*downscaleRatio,i*optimumScaleRatio*downscaleRatio,sampleSize*optimumScaleRatio*downscaleRatio,sampleSize*optimumScaleRatio*downscaleRatio));
								roi=cv::Scalar(tileVal);
								Mat roi2(dataMap, cv::Rect(j*optimumScaleRatio*downscaleRatio,i*optimumScaleRatio*downscaleRatio,sampleSize*optimumScaleRatio*downscaleRatio,sampleSize*optimumScaleRatio*downscaleRatio));
								roi2=cv::Scalar(0);
								RedBand.release();
								mat.release();
	
							}else{
								Mat roi2(dataMap, cv::Rect(j* optimumScaleRatio*downscaleRatio,i *optimumScaleRatio*downscaleRatio,1,1));
								roi2=cv::Scalar(0);

							}

								cv::imshow("Scanning",resultMap);
								cvWaitKey(1);

						}
					
					if (Debug) {
						myfile <<"OK\n";
					}
					cv::Mat heatMap;
					resize(resultMap,heatMap,summedMat.size(),0,0,CV_INTER_NN);
					heatMap2=cv::Scalar(255);
					heatMap.copyTo(heatMap2,summedMat==0);
					heatMap2=heatMap2+summedMat;
					if (Debug) {
						myfile <<" saving to  cache:";
					}
					imwrite(outputfile,heatMap2); //write intermediate result for further usage.
					if (Debug) {
						myfile <<"OK\n";
					}
						
				}
				else
				{
					if (Debug) {
					myfile <<" cache used\n";
					}

				}


					auto largest_region = source_factory->getLevelRegion(0);
					auto largest_size = size(largest_region);


					image::RawImage mask(sz, Color(ColorSpace::RGBA_8));
					mask.fill(0);

					int thresholdVal1=score_threshold_; //default 40
					int thresholdVal2=score_threshold_*4/5;
					int thresholdVal3=score_threshold_*3/5;
					int thresholdVal4=score_threshold_*2/5;
					int thresholdVal5=score_threshold_*1/5;

					if (Debug) {
					myfile <<"Creating final map for visualization.\n";
					}
					//Creating final map for visualization.
					for (int i=0;i<sz.width();i++){ //not best way
						for (int j=0;j<sz.height();j++){
							unsigned char &intensity = heatMap2.at<unsigned char>(j, i);
							unsigned char intensityR=0;
							unsigned char intensityG=0;
							unsigned char intensityA=0;
							if (intensity<=thresholdVal1 && intensity>0){ //TODO: update that part, use otsu 
								intensityA=255;
								intensityG=0;
								if (intensity>=thresholdVal2) {intensityR=80;}  //TODO: It is UGLY!. Do it nice!
								else if(intensity>=thresholdVal3){intensityR=130;}
								else if(intensity>=thresholdVal4){intensityR=180;}	
								else if(intensity>=thresholdVal5){intensityR=220;}	
								else {intensityR=255;}
							}
							if (intensity>thresholdVal1 && intensity<255){
								intensityA=255;
								intensityR=0;
								if (intensity<thresholdVal1+(80-thresholdVal1)*1/4 ) {intensityG=80;} 
								else if(intensity<=thresholdVal1+(80-thresholdVal1)*2/4 ){intensityG=130;}
								else if(intensity<=thresholdVal1+(80-thresholdVal1)*3/4 ){intensityG=180;}	
								else if(intensity<=thresholdVal1+(80-thresholdVal1)*4/4 ){intensityG=220;}	
								else {intensityG=255;}
							}
							mask.setValue(i,j,0,intensityR);
							mask.setValue(i,j,1,intensityG); 
							mask.setValue(i,j,3,intensityA);

						}
					}

					image_result_.update(mask, largest_region);
					cv::destroyAllWindows();
					if (Debug) {
					myfile <<"Done.\n";
					myfile.close();
					}

			}



			bool outOfFocus::buildPipeline(int threshold) {
				// For Cache, FilterFactory, ChannelSelect, Threshold, etc...
				using namespace image::tile;
				bool pipeline_changed = false;



				return pipeline_changed;
			}


		} // namespace algorithm
} // namespace sedeen

