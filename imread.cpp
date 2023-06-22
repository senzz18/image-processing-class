#include <math.h>

#include <cstdio>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/quality.hpp>
#include <string>
#include <vector>

#include "mytools.hpp"

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("An input image file is missing.\n");
    return EXIT_FAILURE;
  }
  cv::Mat image;
  image = cv::imread(argv[1], cv::ImreadModes::IMREAD_COLOR);
  if (image.empty())
  {
    printf("Image file is not found.\n");
    return EXIT_FAILURE;
  }
  cv::Mat original = image.clone();
  if (argc < 3)
  {
    printf("Qfactor is missing.\n");
    return EXIT_FAILURE;
  }
  int QF = strtol(argv[2], nullptr, 10);
  if (QF < 0 || QF > 100)
  {
    printf("Valid range for Qfactor is from 0 to 100.\n");
    return EXIT_FAILURE;
  }
  QF = (QF == 0) ? 1 : QF;
  float scale;
  if (QF < 50)
  {
    scale = floorf(5000.0 / QF);
  }
  else
  {
    scale = 200 - QF * 2;
  }

  int qtable_L[64], qtable_C[64];
  create_qtable(0, scale, qtable_L);
  create_qtable(1, scale, qtable_C);

  bgr2ycrcb(image);
  std::vector<cv::Mat> ycrcb;
  cv::split(image, ycrcb);
  // encoder
  for (int c = 0; c < image.channels(); ++c)
  {
    int *qtable = qtable_L;
    if (c > 0)
    {
      qtable = qtable_L;
      cv::resize(ycrcb[c], ycrcb[c], cv::Size(1 * 0.5, 1 * 0.5), cv::INTER_LINEAR_EXACT);
    }

    cv::Mat image_resize;
    image_resize += ycrcb[c];
    cv::Mat buf;
    // 行列を他の型に変換(32bit浮動小数点型)
    ycrcb[c].convertTo(buf[c], CV_32F);
    // DCレベルシフト
    buf[c] -= 128.0;

    blkproc(buf, blk::dct2);
    blkproc(buf, blk::quantize, qtable);
  }
  // decoder
  for (int c = 0; c < image.channels(); ++c)
  {
    int *qtable = qtable_L;
    if (c > 0)
    {
      qtable = qtable_L;
    }
    blkproc(buf, blk::dequantize, qtable);
    blkproc(buf, blk::idct2);
    // 逆DCレベルシフト
    buf[c] += 128.0;

    buf[c].convertTo(ycrcb[c], ycrcb[c].type());

    if (c > 0)
    {

      cv::resize(ycrcb[c], ycrcb[c], cv::Size(1.0 / 0.5, 1.0 / 0.5),
                 cv::INTER_LINEAR_EXACT);
    }
    cv::Mat image_original;
    image_original += ycrcb[c];

    if (c = 2)
    {
      double psnr = PSNR(image_original, image_resize);
      std::cout << "psnr: " << psnr << std::endl;
    }
  }
  // // qualityを使う場合
  // myPSNR(img, ref)
  // {
  //   auto psnr = cv::qulaity::QualityPSNR;
  //   ;
  //   create(ref);
  //   cv::Scalar a = psnr->compute(img);
  //   double tmp = 0.0;
  //   for (c = 0; c < image.channels; ++c)
  //   {
  //     tmp += a[c];
  //   }
  //   tmp /= ref.channels();
  //   pintf("")
  // }
  cv::merge(ycrcb, image);
  cv::cvtColor(image, image, cv::COLOR_YCrCb2BGR);

  // PSNRの計算
  // double PSNR(cv::Mat& image_original, cv::Mat& image_resize) {
  //   PSNR =
  //       cv::psnr(cv.quality.QualityPSNR_compute(image_original,
  //       image_resize));
  //   print("PSNRの値は ", PSNR);
  // }

  cv::imshow("image", image);
  cv::waitKey();
  cv::destroyAllWindows();

  return EXIT_SUCCESS;
}

double PSNR(const cv::Mat &image_original, const cv::Mat &image_resize)
{
  cv::Mat image_error;
  // 2つの配列の差の絶対値を求める
  cv::absdiff(image_original, image_resize, image_error);
  // 浮動小数点型に変更
  image_error.convertTo(image_error, CV_32F);

  cv::Mat error_squared;
  // 2つの配列の各要素の積を求める
  cv::multiply(image_error, image_error, error_squared);

  // cv::meanは配列要素の平均値を求める
  double mse = cv::mean(error_squared).val[0];
  double MAX_VALUE = 255.0;
  double psnr = 20.0 * log10(MAX_VALUE / sqrt(mse));

  return psnr;
}