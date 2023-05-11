#include <cstdio>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

int main(int arg, char *argv[])
{
  cv::Mat image;
  image = cv::imread(argv[1], cv::ImreadModes::IMREAD_GRAYSCALE);
  if (image.empty())
  {
    printf("Image file is not found.\n");
    return EXIT_FAILURE;
  }

  printf("width = %d, height = %d", image.cols, image.rows);
  for (int y = 0; y < image.rows; ++y)
  {
    for (int x = 0; x < image.cols; ++x)
    {
      int val = image.data[y * image.cols + x];
      val /= 2;
      image.data[y * image.cols + x] = val;
    }
  }

  cv::imshow("image", image);
  cv::waitKey();
  cv::destroyAllWindows();

  return EXIT_SUCCESS;
}
