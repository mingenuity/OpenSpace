/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_MODULE_GAIAMISSION___GAIAOPTIONS___H__
#define __OPENSPACE_MODULE_GAIAMISSION___GAIAOPTIONS___H__


namespace openspace::gaiamission {

    enum RenderOption {
        Static = 0,
        Color = 1,
        Motion = 2
    };

    enum FileReaderOption {
        Fits = 0,
        Speck = 1,
        BinaryRaw = 2,
        BinaryOctree = 3,
        StreamOctree = 4
    };

    enum ShaderOption {
        Point_SSBO = 0,
        Point_VBO = 1,
        Billboard_SSBO = 2,
        Billboard_VBO = 3,
        Billboard_SSBO_noFBO = 4
    };

} // namespace openspace::gaiamission

#endif // __OPENSPACE_MODULE_GAIAMISSION___GAIAOPTIONS___H__