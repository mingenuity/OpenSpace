/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
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

// open space includes
#include <openspace/rendering/renderablevolumegl.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/util/powerscaledcoordinate.h>

#include <ghoul/opengl/texturereader.h>
#include <ghoul/opencl/clworksize.h>
#include <ghoul/filesystem/filesystem.h>

#include <algorithm>

#include <openspace/engine/openspaceengine.h>
#include <sgct.h>

namespace {
    std::string _loggerCat = "RenderableVolumeGL";
}

namespace openspace {

RenderableVolumeGL::RenderableVolumeGL(const ghoul::Dictionary& dictionary):
    RenderableVolume(dictionary),  _box(nullptr), _boxScaling(1.0, 1.0, 1.0),
    _updateTransferfunction(false), _id(-1) {
        
    
    _filename = "";
    if(dictionary.hasKey("Volume")) {
        if(dictionary.getValue("Volume", _filename)) {
            _filename = findPath(_filename);
        }
    }

    LDEBUG("filename: " << _filename);
    
    if(dictionary.hasKey("Hints"))
        dictionary.getValue("Hints", _hintsDictionary);

    _transferFunction = nullptr;
    _transferFunctionFile = nullptr;
    _transferFunctionPath = "";
    if (dictionary.hasKey("TransferFunction")) {
        std::string transferFunctionPath = "";
        if(dictionary.getValue("TransferFunction", transferFunctionPath)) {
            _transferFunctionPath = findPath(transferFunctionPath);
        }
    }
    _samplerFilename = "";
    if (dictionary.hasKey("Sampler")) {
        if(dictionary.getValue("Sampler", _samplerFilename)) {
            _samplerFilename = findPath(_samplerFilename);
        }
    }
    if( _transferFunctionPath == "") {
        LERROR("No transferFunction!");
    } else {
        _transferFunctionFile = new ghoul::filesystem::File(_transferFunctionPath, true);
    }
    if( _samplerFilename == "") {
        LERROR("No samplerfile!");
    } 


    double tempValue;
    if(dictionary.hasKey("BoxScaling.1") && dictionary.getValue("BoxScaling.1", tempValue)) {
    	if(tempValue > 0.0)
    		_boxScaling[0] = tempValue;
    }
    if(dictionary.hasKey("BoxScaling.2") && dictionary.getValue("BoxScaling.2", tempValue)) {
    	if(tempValue > 0.0)
    		_boxScaling[1] = tempValue;
    }
    if(dictionary.hasKey("BoxScaling.3") && dictionary.getValue("BoxScaling.3", tempValue)) {
    	if(tempValue > 0.0)
    		_boxScaling[2] = tempValue;
    }

    _volumeName = "";
    if (dictionary.hasKey("VolumeName"))
        dictionary.getValue("VolumeName", _volumeName);
    _transferFunctionName = "";
    if (dictionary.hasKey("TransferFunctionName"))
        dictionary.getValue("TransferFunctionName", _transferFunctionName);

    setBoundingSphere(PowerScaledScalar::CreatePSS(glm::length(_boxScaling)));
}

RenderableVolumeGL::~RenderableVolumeGL() {
    deinitialize();
    if(_volume)
        delete _volume;
    if(_transferFunctionFile)
        delete _transferFunctionFile;
    if(_transferFunction)
        delete _transferFunction;
    if(_box)
        delete _box;
}

bool RenderableVolumeGL::initialize() {
    // assert(_filename != "");
    //	------ VOLUME READING ----------------
    // TODO: fix volume an transferfunction names
    if(_filename != "") {
        _volume = loadVolume(_filename, _hintsDictionary);
        _volume->uploadTexture();
        OsEng.renderEngine().abuffer()->addVolume(_volumeName, _volume);
    }

    if(_transferFunctionPath != "") {
        _transferFunction = loadTransferFunction(_transferFunctionPath);
        _transferFunction->uploadTexture();
        OsEng.renderEngine().abuffer()->addTransferFunction(_transferFunctionName, _transferFunction);

        auto textureCallback = [this](const ghoul::filesystem::File& file) {
            _updateTransferfunction = true;
        };
        _transferFunctionFile->setCallback(textureCallback);
    }

    // add the sampler and get the ID
    _id = OsEng.renderEngine().abuffer()->addSamplerfile(_samplerFilename);

    _box = new sgct_utils::SGCTBox(1.0f, sgct_utils::SGCTBox::Regular);
    OsEng.configurationManager().getValue("RaycastProgram", _boxProgram);
    _MVPLocation = _boxProgram->uniformLocation("modelViewProjection");
    _modelTransformLocation = _boxProgram->uniformLocation("modelTransform");
    _typeLocation = _boxProgram->uniformLocation("volumeType");

    // ============================
    //      GEOMETRY (quad)
    // ============================
    const GLfloat size = 0.5f;
    const GLfloat vertex_data[] = { // square of two triangles (sigh)
        //  x,     y,     z,     s,
        -size, -size,  size,  0.0f,
         size,  size,  size,  0.0f,
        -size,  size,  size,  0.0f,
        -size, -size,  size,  0.0f,
         size, -size,  size,  0.0f,
         size,  size,  size,  0.0f,

        -size, -size, -size,  0.0f,
         size,  size, -size,  0.0f,
        -size,  size, -size,  0.0f,
        -size, -size, -size,  0.0f,
         size, -size, -size,  0.0f,
         size,  size, -size,  0.0f,

         size, -size, -size,  0.0f,
         size,  size,  size,  0.0f,
         size, -size,  size,  0.0f,
         size, -size, -size,  0.0f,
         size,  size, -size,  0.0f,
         size,  size,  size,  0.0f,

        -size, -size, -size,  0.0f,
        -size,  size,  size,  0.0f,
        -size, -size,  size,  0.0f,
        -size, -size, -size,  0.0f,
        -size,  size, -size,  0.0f,
        -size,  size,  size,  0.0f,

        -size,  size, -size,  0.0f,
         size,  size,  size,  0.0f,
        -size,  size,  size,  0.0f,
        -size,  size, -size,  0.0f,
         size,  size, -size,  0.0f,
         size,  size,  size,  0.0f,

        -size, -size, -size,  0.0f,
         size, -size,  size,  0.0f,
        -size, -size,  size,  0.0f,
        -size, -size, -size,  0.0f,
         size, -size, -size,  0.0f,
         size, -size,  size,  0.0f,
    };
    GLuint vertexPositionBuffer;
    glGenVertexArrays(1, &_boxArray); // generate array
    glBindVertexArray(_boxArray); // bind array
    glGenBuffers(1, &vertexPositionBuffer); // generate buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, reinterpret_cast<void*>(0));
    //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*7, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);
    
    
    return true;
}

bool RenderableVolumeGL::deinitialize() {
    return true;
}

void RenderableVolumeGL::render(const Camera *camera, const psc &thisPosition) {
    if(_updateTransferfunction) {
        _updateTransferfunction = false;
        ghoul::opengl::Texture* transferFunction = loadTransferFunction(_transferFunctionPath);
        if(transferFunction) {
            const void* data = transferFunction->pixelData();
            glBindBuffer(GL_COPY_READ_BUFFER, *transferFunction);
            _transferFunction->bind();
            glTexImage1D(   GL_TEXTURE_1D, 0, _transferFunction->internalFormat(), 
                            _transferFunction->width(),0, _transferFunction->format(), 
                            _transferFunction->dataType(), data);
            delete transferFunction;
            LDEBUG("Updated transferfunction!");

        }
    }

    //psc relative = thisPosition-camera->position();

    // glm::mat4 transform = camera->viewRotationMatrix();
    // transform = glm::translate(transform, relative.vec3());
    // transform = glm::translate(transform, glm::vec3(-1.1,0.0,0.0));
    // transform = glm::scale(transform, _boxScaling);
    glm::mat4 transform = glm::mat4(1.0);
    transform = glm::scale(transform, _boxScaling);

    // fetch data
    psc currentPosition         = thisPosition;
    psc campos                  = camera->position();
    glm::mat4 camrot            = camera->viewRotationMatrix();
    PowerScaledScalar scaling   = camera->scaling();

    // psc addon(-1.1,0.0,0.0,0.0);
    // currentPosition += addon;

    // TODO: Use _id to identify this volume
    _boxProgram->activate();
    // _boxProgram->setUniform(_MVPLocation, camera->viewProjectionMatrix());
    // _boxProgram->setUniform(_modelTransformLocation, transform);
    _boxProgram->setUniform(_typeLocation, _id);

    _boxProgram->setUniform("modelViewProjection", camera->viewProjectionMatrix());
    _boxProgram->setUniform("modelTransform", transform);
    _boxProgram->setUniform("campos", campos.vec4());
    _boxProgram->setUniform("objpos", currentPosition.vec4());
    _boxProgram->setUniform("camrot", camrot);
    _boxProgram->setUniform("scaling", scaling.vec2());

    // make sure GL_CULL_FACE is enabled (it should be)
    glEnable(GL_CULL_FACE);

    //  Draw backface
    glCullFace(GL_FRONT);
    glBindVertexArray(_boxArray);
    glDrawArrays(GL_TRIANGLES, 0, 6*6);
    // _box->draw();

    //  Draw frontface (now the normal cull face is is set)
    glCullFace(GL_BACK);
    glDrawArrays(GL_TRIANGLES, 0, 6*6);
    // _box->draw();

    _boxProgram->deactivate();

}

void RenderableVolumeGL::update() {
    
}


} // namespace openspace
