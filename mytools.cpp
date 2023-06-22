#include "mytools.hpp"
#include <opencv2/quality.hpp>
#include <zigzag_order.hpp>

void bgr2ycrcb(cv::Mat &image)
{
  const int WIDTH = image.cols;
  const int HEIGHT = image.rows;
  const int NC = image.channels();
  // const int STRIDE = WIDTH * NC;
  uchar *p0, *p1, *p2;
  p0 = image.data;     // B
  p1 = image.data + 1; // G
  p2 = image.data + 2; // R
  for (int y = 0; y < HEIGHT; ++y)
  {
    for (int x = 0; x < WIDTH; ++x, p0 += NC, p1 += NC, p2 += NC)
    {
      int B = *p0, G = *p1, R = *p2;

      double Y = 0.299 * R + 0.587 * G + 0.114 * B;
      double Cb = -0.1687 * R - 0.3313 * G + 0.5 * B + 128;
      double Cr = 0.5 * R - 0.4187 * G - 0.0813 * B + 128;

      *p0 = static_cast<uchar>(roundl(Y));
      *p1 = static_cast<uchar>(roundl(Cr));
      *p2 = static_cast<uchar>(roundl(Cb));
    }
  }
}

void blk::quantize(cv::Mat &in, int c, int *qtable)
{
  if (scale < 0.0)
  {
    return;
  }
  in.forEach<float>([&](float &v, const int *pos) -> void
                    {
    float stepsize = blk::qtable[pos[0] * in.cols + pos[1]];
    v /= qtable[pos[0] * in.cols + pos[1]];
    v = roundf(v); });
}

void blk::dequantize(cv::Mat &in, int c, int *qtable)
{
  in.forEach<float>([&](float &v, const int *pos) -> void
                    { v *= qtable[pos[0] * in.cols + pos[1]]; });
}

void blk::dct2(cv::Mat &in, int p0, int *p) { cv::dct(in, in); }

void blk::idct2(cv::Mat &in, int p0, int *p) { cv::idct(in, in); }

void blkproc(cv::Mat &in, std::function<void(cv::Mat &, int, float)> func,
             int *p)
{
  for (int y = 0; y < in.rows; y += BSIZE)
  {
    for (int x = 0; x < in.cols; x += BSIZE)
    {
      cv::Mat blk_in = in(cv::Rect(x, y, BSIZE, BSIZE)).clone();
      cv::Mat blk_out = in(cv::Rect(x, y, BSIZE, BSIZE));
      func(blk_in, p);
      blk_in.convertTo(blk_out, blk_out.type());
    }
  }
}

void create_qtable(int c, float scale, int *qtable)
{
  for (int i; i < 64; ++i)
  {
    float stepsize = (blk::qmatrix[c][i] * scale + 50.0) / 100.0;
    stepsize = fllor(stepsize);
    qtable[i] = stepsize;
    if (stepsize < 1.0)
    {
      stepsize = 1;
    }
    if (stepsize > 255)
    {
      stepsize = 255;
    }
  }
}

void EncodeBlock(cv::Mat &in, int c, int &prev_dc)
{
  float *p = (float *)in.data;
  // DC
  int diff = p[0] - prev_dc;
  prev_dc = p[0];
  // EncodeDC with Huffman

  int run = 0;
  // AC
  for (inti = 0; i < 64; ++i)
  {
    int ac = p[scan[i]];
    if (ac == 0)
    {
      run++;
    }
    else
    {
      while (run > 15)
      {
        // Encode ZRL with Huffman
        run -= 16;
      }
      // Encode non-zero AC with Huffman
      run = 0;
    }
  }
  if (run)
  {
    // Encode EOB with huffman
  }
}