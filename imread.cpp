#include <cstdio>
#include <opencv2/opencv.hpp>

int main(i) {
  cv::Mat image;
  image = cv::imread("barbara.pgm");
  if (image.empty()) {
    printf("Image file is not found.\n");
    return EXIT_FAILURE;
  }
  cv::imshow("image", image);
  cv::waitKey();
  cv::destroyAllWindows();

  return EXIT_SUCCESS;
}
