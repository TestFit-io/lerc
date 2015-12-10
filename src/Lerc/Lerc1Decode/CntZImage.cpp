/*
Copyright 2015 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

A local copy of the license and additional notices are located with the
source distribution at:

  http://www.github.com/esri/lerc/LICENSE
  http://www.github.com/esri/lerc/NOTICE

Contributors:  Thomas Maurer
*/

#include "CntZImage.h"
#include "../Common/BitMask.h"
#include "../Common/RLE.h"
#include "../Common/Defines.h"
#include <math.h>
#include <float.h>

using namespace std;
using namespace LercNS;

// -------------------------------------------------------------------------- ;

CntZImage::CntZImage()
{
  type_                         = CNT_Z;
  m_currentVersion              = 11;
  InfoFromComputeNumBytes info0 = {0};
  m_infoFromComputeNumBytes     = info0;
  m_bDecoderCanIgnoreMask       = false;
};

CntZImage::~CntZImage()
{
};

// -------------------------------------------------------------------------- ;

bool CntZImage::resizeFill0(int width, int height)
{
  if (!resize(width, height))
    return false;

  memset(getData(), 0, width * height * sizeof(CntZ));
  return true;
}

// -------------------------------------------------------------------------- ;

unsigned int CntZImage::computeNumBytesNeededToReadHeader()
{
  unsigned int cnt = 0;

  CntZImage zImg;
  cnt += (unsigned int)zImg.getTypeString().length();    // "CntZImage ", 10 bytes
  cnt += 2 * sizeof(int);       // version, type
  cnt += 2 * sizeof(int);       // with, height
  cnt += 1 * sizeof(double);    // maxZError

  // cnt part
  cnt += 3 * sizeof(int);
  cnt += sizeof(float);

  // z part
  cnt += 3 * sizeof(int);
  cnt += sizeof(float);
  cnt += 1;

  return cnt;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::read(Byte** ppByte,
                     double maxZError,
                     bool onlyHeader,
                     bool onlyZPart)
{
  if (!ppByte)
    return false;

  size_t len = getTypeString().length();
  string typeStr(len, '0');

  if (ppByte)
  {
    memcpy(&typeStr[0], *ppByte, len);
    *ppByte += len;
  }

  if (typeStr != getTypeString())
  {
    return false;
  }

  int version = 0, type = 0;
  int width = 0, height = 0;
  double maxZErrorInFile = 0;

  if (ppByte)
  {
    Byte* ptr = *ppByte;

    version = *((const int*)ptr);  ptr += sizeof(int);
    type    = *((const int*)ptr);  ptr += sizeof(int);
    height  = *((const int*)ptr);  ptr += sizeof(int);
    width   = *((const int*)ptr);  ptr += sizeof(int);
    maxZErrorInFile = *((const double*)ptr);  ptr += sizeof(double);

    *ppByte = ptr;
  }

  SWAP_4(version);
  SWAP_4(type);
  SWAP_4(height);
  SWAP_4(width);
  SWAP_8(maxZErrorInFile);

  if (version < 11)
    return false;

  if (version > m_currentVersion)
    return false;

  if (type != type_)
    return false;

  if (width > 20000 || height > 20000)
    return false;

  if (maxZErrorInFile > maxZError)
    return false;

  if (onlyHeader)
    return true;

  if (!onlyZPart && !resizeFill0(width, height))    // init with (0,0), used below
    return false;

  m_bDecoderCanIgnoreMask = false;

  for (int iPart = 0; iPart < 2; iPart++)
  {
    bool zPart = iPart ? true : false;    // first cnt part, then z part

    if (!zPart && onlyZPart)
      continue;

    int numTilesVert = 0, numTilesHori = 0, numBytes = 0;
    float maxValInImg = 0;
    Byte* bArr = 0;

    if (ppByte)
    {
      Byte* ptr = *ppByte;
      numTilesVert = *((const int*)ptr);  ptr += sizeof(int);
      numTilesHori = *((const int*)ptr);  ptr += sizeof(int);
      numBytes     = *((const int*)ptr);  ptr += sizeof(int);
      maxValInImg  = *((const float*)ptr); ptr += sizeof(float);

      *ppByte = ptr;
      bArr = ptr;
    }

    SWAP_4(numTilesVert);
    SWAP_4(numTilesHori);
    SWAP_4(numBytes);
    SWAP_4(maxValInImg);

    if (!zPart && numTilesVert == 0 && numTilesHori == 0)    // no tiling for this cnt part
    {
      if (numBytes == 0)    // cnt part is const
      {
        CntZ* dstPtr = getData();
        for (int i = 0; i < height_; i++)
          for (int j = 0; j < width_; j++)
          {
            dstPtr->cnt = maxValInImg;
            dstPtr++;
          }

        if (maxValInImg > 0)
          m_bDecoderCanIgnoreMask = true;
      }

      if (numBytes > 0)    // cnt part is binary mask, use fast RLE class
      {
        // decompress to bit mask
        BitMask bitMask(width_, height_);
        RLE rle;
        if (!rle.decompress(bArr, (Byte*)bitMask.Bits()))
        {
          free(bArr);
          return false;
        }

        CntZ* dstPtr = getData();
        int k = 0;
        for (int i = 0; i < height_; i++)
          for (int j = 0; j < width_; j++, k++)
          {
            if (bitMask.IsValid(k))
              dstPtr->cnt = 1;
            else
              dstPtr->cnt = 0;

            dstPtr++;
          }
      }
    }
    else if (!readTiles(zPart, maxZErrorInFile, numTilesVert, numTilesHori, maxValInImg, bArr))
      return false;

    if (ppByte)
      *ppByte += numBytes;
    else
      free(bArr);
  }

  m_tmpDataVec.clear();
  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readTiles(bool zPart, double maxZErrorInFile,
                          int numTilesVert, int numTilesHori, float maxValInImg,
                          Byte* bArr)
{
  Byte* ptr = bArr;

  for (int iTile = 0; iTile <= numTilesVert; iTile++)
  {
    int tileH = height_ / numTilesVert;
    int i0 = iTile * tileH;
    if (iTile == numTilesVert)
      tileH = height_ % numTilesVert;

    if (tileH == 0)
      continue;

    for (int jTile = 0; jTile <= numTilesHori; jTile++)
    {
      int tileW = width_ / numTilesHori;
      int j0 = jTile * tileW;
      if (jTile == numTilesHori)
        tileW = width_ % numTilesHori;

      if (tileW == 0)
        continue;

      bool rv = zPart ? readZTile(  &ptr, i0, i0 + tileH, j0, j0 + tileW, maxZErrorInFile, maxValInImg) :
                        readCntTile(&ptr, i0, i0 + tileH, j0, j0 + tileW);

      if (!rv)
        return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readCntTile(Byte** ppByte, int i0, int i1, int j0, int j1)
{
  Byte* ptr = *ppByte;
  int numPixel = (i1 - i0) * (j1 - j0);

  Byte comprFlag = *ptr++;

  if (comprFlag == 2)    // entire tile is constant 0 (invalid)
  {                      // here we depend on resizeFill0()
    *ppByte = ptr;
    return true;
  }

  if (comprFlag == 3 || comprFlag == 4)    // entire tile is constant -1 (invalid) or 1 (valid)
  {
    CntZ cz1m = {-1, 0};
    CntZ cz1p = { 1, 0};
    CntZ cz1 = (comprFlag == 3) ? cz1m : cz1p;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
        *dstPtr++ = cz1;
    }

    *ppByte = ptr;
    return true;
  }

  if ((comprFlag & 63) > 4)
    return false;

  if (comprFlag == 0)
  {
    // read cnt's as flt arr uncompressed
    const float* srcPtr = (const float*)ptr;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        dstPtr->cnt = *srcPtr++;
        SWAP_4(dstPtr->cnt);
        dstPtr++;
      }
    }

    ptr += numPixel * sizeof(float);
  }
  else
  {
    // read cnt's as int arr bit stuffed
    int bits67 = comprFlag >> 6;
    int n = (bits67 == 0) ? 4 : 3 - bits67;

    float offset = 0;
    if (!readFlt(&ptr, offset, n))
      return false;

    //vector<unsigned int> dataVec;
    vector<unsigned int>& dataVec = m_tmpDataVec;
    BitStuffer bitStuffer;
    if (!bitStuffer.read(&ptr, dataVec))
      return false;

    unsigned int* srcPtr = &dataVec[0];

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        dstPtr->cnt = offset + (float)(*srcPtr++);
        dstPtr++;
      }
    }
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readZTile(Byte** ppByte, int i0, int i1, int j0, int j1,
                          double maxZErrorInFile, float maxZInImg)
{
  Byte* ptr = *ppByte;
  int numPixel = 0;

  Byte comprFlag = *ptr++;
  int bits67 = comprFlag >> 6;
  comprFlag &= 63;

  if (comprFlag == 2)    // entire zTile is constant 0 (if valid or invalid doesn't matter)
  {
    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if (dstPtr->cnt > 0)
          dstPtr->z = 0;
        dstPtr++;
      }
    }

    *ppByte = ptr;
    return true;
  }

  if (comprFlag > 3)
    return false;

  if (comprFlag == 0)
  {
    // read z's as flt arr uncompressed
    const float* srcPtr = (const float*)ptr;

    for (int i = i0; i < i1; i++)
    {
      CntZ* dstPtr = getData() + i * width_ + j0;
      for (int j = j0; j < j1; j++)
      {
        if (dstPtr->cnt > 0)
        {
          dstPtr->z = *srcPtr++;
          SWAP_4(dstPtr->z);
          numPixel++;
        }
        dstPtr++;
      }
    }

    ptr += numPixel * sizeof(float);
  }
  else
  {
    // read z's as int arr bit stuffed
    int n = (bits67 == 0) ? 4 : 3 - bits67;
    float offset = 0;
    if (!readFlt(&ptr, offset, n))
      return false;

    if (comprFlag == 3)
    {
      for (int i = i0; i < i1; i++)
      {
        CntZ* dstPtr = getData() + i * width_ + j0;
        for (int j = j0; j < j1; j++)
        {
          if (dstPtr->cnt > 0)
            dstPtr->z = offset;
          dstPtr++;
        }
      }
    }
    else
    {
      vector<unsigned int>& dataVec = m_tmpDataVec;
      BitStuffer bitStuffer;
      if (!bitStuffer.read(&ptr, dataVec))
        return false;

      double invScale = 2 * maxZErrorInFile;
      unsigned int* srcPtr = &dataVec[0];

      if (m_bDecoderCanIgnoreMask)
      {
        for (int i = i0; i < i1; i++)
        {
          CntZ* dstPtr = getData() + i * width_ + j0;
          for (int j = j0; j < j1; j++)
          {
            float z = (float)(offset + *srcPtr++ * invScale);
            dstPtr->z = min(z, maxZInImg);    // make sure we stay in the orig range
            dstPtr++;
          }
        }
      }
      else
      {
        for (int i = i0; i < i1; i++)
        {
          CntZ* dstPtr = getData() + i * width_ + j0;
          for (int j = j0; j < j1; j++)
          {
            if (dstPtr->cnt > 0)
            {
              float z = (float)(offset + *srcPtr++ * invScale);
              dstPtr->z = min(z, maxZInImg);    // make sure we stay in the orig range
            }
            dstPtr++;
          }
        }
      }
    }
  }

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

int CntZImage::numBytesFlt(float z) const
{
  short s = (short)z;
  char c = (char)s;
  return ((float)c == z) ? 1 : ((float)s == z) ? 2 : 4;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readFlt(Byte** ppByte, float& z, int numBytes) const
{
  Byte* ptr = *ppByte;

  if (numBytes == 1)
  {
    char c = *((char*)ptr);
    z = c;
  }
  else if (numBytes == 2)
  {
    short s = *((short*)ptr);
    SWAP_2(s);
    z = s;
  }
  else if (numBytes == 4)
  {
    z = *((float*)ptr);
    SWAP_4(z);
  }
  else
    return false;

  *ppByte = ptr + numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

