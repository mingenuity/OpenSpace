/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2017                                                               *
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

#include <modules/volume/transferfunctionhandler.h>
#include <ghoul/misc/dictionary.h>
#include <openspace/properties/scalarproperty.h>

#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/filesystem/filesystem.h>

static const openspace::properties::Property::PropertyInfo TransferFunctionInfo = {
    "TransferFunction",
    "TransferFunction",
    "All the envelopes"
};

static const openspace::properties::Property::PropertyInfo HistogramInfo = {
    "Histogram",
    "Histogram",
    "All the data"
};

namespace openspace {
    namespace volume {

        ghoul::opengl::Texture::FilterMode filtermode = ghoul::opengl::Texture::FilterMode::Linear;
        ghoul::opengl::Texture::WrappingMode wrappingmode = ghoul::opengl::Texture::WrappingMode::ClampToEdge;

        
        TransferFunctionHandler::TransferFunctionHandler(const properties::StringProperty& prop)
            : properties::PropertyOwner({ "TransferFunctionHandler" }),
            _transferFunctionPath(prop),
            _transferFunctionProperty(TransferFunctionInfo),
            _histogramProperty(HistogramInfo)
        {
            _transferFunction = std::make_shared<openspace::TransferFunction>(_transferFunctionPath);
        }

        void TransferFunctionHandler::initialize() {


            addProperty(_transferFunctionPath);
            addProperty(_transferFunctionProperty);
            addProperty(_histogramProperty);

            _texture = std::make_shared<ghoul::opengl::Texture>(
                glm::size3_t(1024, 1, 1), ghoul::opengl::Texture::Format::RGBA,
                GL_RGBA, GL_FLOAT, filtermode, wrappingmode);

            _transferFunction->setTextureFromTxt(_texture);
            uploadTexture();

            _transferFunctionProperty.onChange([this]() {
                setTexture();
            });
        }

        void TransferFunctionHandler::buildHistogram(float *data, int num) {
            _histogram = std::make_shared<Histogram>(0.0, 1.0, 100);
            for (int i = 0; i < num; ++i) {
                _histogram->add(data[i]);
            }
            _histogramProperty.setValue(openspace::properties::VectorProperty{ _histogram->getDataAsVector() });
        }

        void TransferFunctionHandler::setTexture() {
            if (_transferFunctionProperty.value().createTexture(_texture)) {
                uploadTexture();
                useTxtTexture = false;
            }
        }

        ghoul::opengl::Texture& TransferFunctionHandler::getTexture() {
            return *_texture.get();
        }

        void TransferFunctionHandler::uploadTexture() {
            _texture->uploadTexture();
        }

        bool TransferFunctionHandler::hasTexture() {
            if (_texture == nullptr)
                return false;
            return true;
        }

        std::shared_ptr<openspace::TransferFunction> TransferFunctionHandler::getTransferFunction() {
            return _transferFunction;
        }

    } //namespace volume
} // namespace openspace
