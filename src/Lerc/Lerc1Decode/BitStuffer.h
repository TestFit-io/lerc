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

#pragma once

#include "Image.h"
#include <vector>

/** Bit stuffer, for writing unsigned int arrays compressed lossless
*
*/

namespace LercNS
{
  class BitStuffer
  {
  public:
    BitStuffer()           {}
    virtual ~BitStuffer()  {}

    // these 2 do not allocate memory. Byte ptr is moved like a file pointer.
    bool write(Byte** ppByte, const std::vector<unsigned int>& dataVec) const;
    bool read(Byte** ppByte, std::vector<unsigned int>& dataVec) const;

    static unsigned int computeNumBytesNeeded(unsigned int numElem, unsigned int maxElem);
    static unsigned int numExtraBytesToAllocate()  { return 3; }

  protected:
    unsigned int findMax(const std::vector<unsigned int>& dataVec) const;

    // numBytes = 1, 2, or 4
    bool writeUInt(Byte** ppByte, unsigned int k, int numBytes) const;
    bool readUInt(Byte** ppByte, unsigned int& k, int numBytes) const;

    static int numBytesUInt(unsigned int k)  { return (k < 256) ? 1 : (k < (1 << 16)) ? 2 : 4; }
    static unsigned int numTailBytesNotNeeded(unsigned int numElem, int numBits);
  };
}

