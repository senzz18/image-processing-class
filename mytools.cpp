#include "mytools.hpp"

#include <x86intrin.h>

#include <opencv2/quality.hpp>

#include "bitstream.hpp"
#include "huffman_tables.hpp"
#include "zigzag_order.hpp"
#include "ycctype.hpp"

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

void blk::quantize(cv::Mat &in, int *qtable)
{
  in.forEach<float>([&](float &v, const int *pos) -> void
                    {
    v /= qtable[pos[0] * in.cols + pos[1]];
    v = roundf(v); });
}

void blk::dequantize(cv::Mat &in, int *qtable)
{
  in.forEach<float>([&](float &v, const int *pos) -> void
                    { v *= qtable[pos[0] * in.cols + pos[1]]; });
}

void blk::dct2(cv::Mat &in, int *p) { cv::dct(in, in); }

void blk::idct2(cv::Mat &in, int *p) { cv::idct(in, in); }

void blkproc(cv::Mat &in, std::function<void(cv::Mat &, int *)> func, int *p)
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

void myPSNR(cv::Mat &ref, cv::Mat &img)
{
  auto psnr = cv::quality::QualityPSNR::create(ref);
  cv::Scalar a = psnr->compute(img);
  double tmp = 0.0;
  for (int c = 0; c < ref.channels(); ++c)
  {
    tmp += a[c];
  }
  tmp /= ref.channels();
  printf("PSNR = %f [dB]\n", tmp);
}

void create_qtable(int c, float scale, int *qtable)
{
  for (int i = 0; i < 64; ++i)
  {
    float stepsize = (blk::qmatrix[c][i] * scale + 50.0) / 100.0;
    stepsize = floor(stepsize);
    if (stepsize < 1.0)
    {
      stepsize = 1;
    }
    if (stepsize > 255)
    {
      stepsize = 255;
    }
    qtable[i] = stepsize;
  }
}

void EncodeDC(int diff, const uint32_t *Ctable, const uint32_t *Ltable,
              bitstream &enc)
{
  uint32_t uval = (diff < 0) ? -diff : diff;
  uint32_t s = 32 - _lzcnt_u32(uval);
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0)
  {
    if (diff < 0)
    {
      diff -= 1;
    }
    enc.put_bits(diff, s);
  }
}

void EncodeAC(int run, int val, const uint32_t *Ctable, const uint32_t *Ltable,
              bitstream &enc)
{
  uint32_t uval = (val < 0) ? -val : val;
  uint32_t s = 32 - _lzcnt_u32(uval);
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0)
  {
    if (val < 0)
    {
      val -= 1;
    }
    enc.put_bits(val, s);
  }
}

void EncodeBlock(cv::Mat &in, int c, int &prev_dc, bitstream &enc)
{
  float *p = (float *)in.data;
  // DC
  int diff = p[0] - prev_dc;
  prev_dc = p[0];
  // EncodeDC with Huffman
  EncodeDC(diff, DC_cwd[c], DC_len[c], enc);

  int run = 0;
  // AC
  for (int i = 1; i < 64; ++i)
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
        EncodeAC(0xF, 0x0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      // Encode non-zero AC with Huffman
      EncodeAC(run, ac, AC_cwd[c], AC_len[c], enc);
      run = 0;
    }
  }
  if (run)
  {
    // Encode EOB with Huffman
    EncodeAC(0x0, 0x0, AC_cwd[c], AC_len[c], enc);
  }
}

void Encode_MCUs(std::vector<cv::Mat> &buf, bitstream &enc, int YCCtype)
{
  const int nc = buf.size();
  const int height = buf[0].rows;
  const int width = buf[0].cols;
  int H, V;
  switch (YCCtype)
  {
  case YCC::YUV420:
    H = V = 2;
    break;
  case YCC::GRAY:
    H = V = 1;
    break;
  default:
    H = V = 0;
    break;
  }

  cv::Mat blk;
  int prev_dc[3] = {0};
  for (int Ly = 0, Cy = 0; Ly < height; Ly += BSIZE * V, Cy += BSIZE)
  {
    for (int Lx = 0, Cx = 0; Lx < width; Lx += BSIZE * H, Cx += BSIZE)
    {
      // Y
      for (int y = 0; y < V; ++y)
      {
        for (int x = 0; x < H; ++x)
        {
          blk = buf[0](cv::Rect(Lx + BSIZE * x, Ly + BSIZE * y, BSIZE, BSIZE))
                    .clone();
          EncodeBlock(blk, 0, prev_dc[0], enc);
        }
      }
      if (nc == 3)
      {
        // Cb
        blk = buf[2](cv::Rect(Cx, Cy, BSIZE, BSIZE)).clone();
        EncodeBlock(blk, 1, prev_dc[1], enc);
        // Cr
        blk = buf[1](cv::Rect(Cx, Cy, BSIZE, BSIZE)).clone();
        EncodeBlock(blk, 1, prev_dc[2], enc);
      }
    }
  }
}