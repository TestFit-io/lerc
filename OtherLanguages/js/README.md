[![npm version][npm-img]][npm-url]

[npm-img]: https://img.shields.io/npm/v/lerc.svg?style=flat-square
[npm-url]: https://www.npmjs.com/package/lerc

# Lerc JS

> Rapid decoding of Lerc compressed raster data for any standard pixel type, not just rgb or byte

## Usage

```js
npm install 'lerc'
```
```js
var Lerc = require('lerc');

Lerc.decode(xhrResponse, {
  pixelType: "U8", // leave pixelType out in favor of F32 for lerc1
  inputOffset: 10 // start from the 10th byte
});
```

## API Reference

<a name="module_Lerc"></a>

## Lerc
a module for decoding LERC blobs

<a name="exp_module_Lerc--decode"></a>

### decode(input, [options]) ⇒ <code>Object</code> ⏏
A wrapper for decoding both LERC1 and LERC2 byte streams capable of handling multiband pixel blocks for various pixel types.

**Kind**: Exported function

| Param | Type | Description |
| --- | --- | --- |
| input | <code>ArrayBuffer</code> | The LERC input byte stream |
| [options] | <code>object</code> | The decoding options below are optional. |
| [options.inputOffset] | <code>number</code> | The number of bytes to skip in the input byte stream. A valid Lerc file is expected at that position. |
| [options.pixelType] | <code>string</code> | (LERC1 only) Default value is F32. Valid pixel types for input are U8/S8/S16/U16/S32/U32/F32. |
| [options.noDataValue] | <code>number</code> | (LERC1 only). It is recommended to use the returned mask instead of setting this value. |

**Result Object**

| Name | Type | Description |
| --- | --- | --- |
| width | <code>number</code> | Width of decoded image. |
| height | <code>number</code> | Height of decoded image. |
| pixels | <code>array</code> | [band1, band2, …] Each band is a typed array of width*height. |
| pixelType | <code>string</code> | The type of pixels represented in the output. |
| mask | <code>mask</code> | Typed array with a size of width*height, or null if all pixels are valid. |
| statistics | <code>array</code> | [statistics_band1, statistics_band2, …] Each element is a statistics object representing min and max values |

* * *

## Licensing

Copyright 2017 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and limitations under the License.

A local copy of the license and additional notices are located with the source distribution at:

http://github.com/Esri/lerc/

[](Esri Tags: raster, image, encoding, encoded, decoding, decoded, compression, codec, lerc)
[](Esri Language: JS, JavaScript, Python)