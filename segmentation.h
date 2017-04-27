#ifndef SEDEEN_SEGMENTATION_H
#define SEDEEN_SEGMENTATION_H

// System headers
#include <memory>

// DPTK headers - a minimal set 
#include "algorithm/AlgorithmBase.h"
#include "algorithm/Parameters.h"
#include "algorithm/Results.h"

#include "opencv2/imgproc/imgproc_c.h"
//#include "opencv2/imgcodecs.hpp"
//#include "opencv2/highgui/highgui.hpp"
namespace sedeen {

namespace image {

class RawImage;

namespace tile {

class ChannelSelect;
class Closing;
class FilterFactory;
class Opening;
class Threshold;

} // namespace tile

} // namespace image
using namespace cv;
namespace algorithm {

/// Algorithm for detecting foreground of images
//
/// This algorithm segments an image and draws bounding boxes around regions it
/// determines are in the foreground. 
///
/// Foreground detection is based on OTSU thresholding of the selected channel
/// followed by morphological closing and opening (which remove the spurious 
/// results). The minimum size of the foreground object can be set via the
/// window size of the morphological operators.
///
/// \todo
/// To better delineate foreground region, output should be polygons rather 
/// than rectangles
class outOfFocus : public AlgorithmBase {
 public:
  outOfFocus();

  virtual ~outOfFocus();

 private:
  virtual void run();

  virtual void init(const image::ImageHandle& image);


  bool buildPipeline(int threshold);
  void getMatObject(Mat&);
  void setOutputObject(cv::Mat imageOpenCV);
  String getUniqueFilename();
    /// Parameter for selecting threshold retainment 
  algorithm::OptionParameter behavior_;

    // The output color
 // algorithm::ColorParameter output_color_;
  algorithm::OptionParameter retainment_;
  algorithm::IntegerParameter window_size_;
  algorithm::IntegerParameter score_threshold_;
  /// Overlay reporter through which bounding boxes are displayed
  OverlayResult foreground_results_;

   /// The result image.
  algorithm::ImageResult image_result_;
  


};

} // namespace algorithm
} // namespace sedeen

#endif // ifndef SEDEEN_SRC_TISSUEFINDER_TISSUEFINDER_H