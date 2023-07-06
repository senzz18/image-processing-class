#pragma once

#include <cassert>
#include <cstdint>
#include <vector>
#include "jpgmarkers.hpp"

class bitstream
{
private:
  int32_t bits;
  uint8_t tmp;
  std::vector<uint8_t> stream;

public:
  bitstream() : bits(0), tmp(0) { put_word(SOI); }
  // 1バイトを書き込む
  void put_byte(uint8_t data) { stream.push_back(data); }
  // 2バイトを書き込む
  void put_word(uint16_t data)
  {
    put_byte(data >> 8);
    put_byte(data & 0xFF);
  }
  // nビットを書き込む
  void put_bits(uint32_t cwd, uint32_t n)
  {
    assert(n > 0);
    while (n)
    {
      tmp <<= 1;
      tmp |= (cwd >> (n - 1)) & 1;
      n--;
      bits++;
      if (bits == 8)
      {
        put_byte(tmp);
        // byte stuff
        if (tmp == 0xFF)
        {
          put_byte(0x00);
        }
        tmp = 0;
        bits = 0;
      }
    }
  }
  // ビットストリームの終端処理
  void flush()
  {
    if (bits)
    {
      uint8_t stuff = 0xFFU >> bits;
      tmp <<= (8 - bits);
      tmp |= stuff;
      put_byte(tmp);
      // byte stuff
      if (tmp == 0xFF)
      {
        put_byte(0x00);
      }
    }
    tmp = 0;
    bits = 0;
  }

  auto finalize()
  {
    flush();
    put_word(EOI);
    return std::move(stream);
  }
};