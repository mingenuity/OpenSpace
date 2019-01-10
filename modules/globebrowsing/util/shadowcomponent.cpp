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

#include <modules/globebrowsing/util/shadowcomponent.h>
#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/src/renderableglobe.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>

#include <openspace/engine/globals.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>

#include <openspace/interaction/navigationhandler.h>
#include <openspace/interaction/orbitalnavigator.h>
#include <openspace/interaction/keyframenavigator.h>

#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>

#include <openspace/util/camera.h>

#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>

#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <cstdlib>
#include <locale>

namespace {
    constexpr const char* _loggerCat = "ShadowComponent";
    
    constexpr openspace::properties::Property::PropertyInfo SaveDepthTextureInfo = {
        "SaveDepthTextureInfo",
        "Save Depth Texture",
        "Debug"
    };

    constexpr openspace::properties::Property::PropertyInfo DistanceFractionInfo = {
        "DistanceFraction",
        "Distance Fraction",
        "Distance fraction of original distance from light source to the globe to be "
        "considered as the new light source distance."
    };

    constexpr openspace::properties::Property::PropertyInfo PolygonOffsetFactorInfo = {
        "PolygonOffsetFactor",
        "Polygon Offset Factor",
        "Polygon Offset Factor"
    };

    constexpr openspace::properties::Property::PropertyInfo PolygonOffsetUnitsInfo = {
        "PolygonOffsetUnits",
        "Polygon Offset Units",
        "Polygon Offset Units"
    };

    void checkFrameBufferState(const std::string& codePosition)
    {
        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LERROR("Framework not built. " + codePosition);
            GLenum fbErr = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            switch (fbErr) {
            case GL_FRAMEBUFFER_UNDEFINED:
                LERROR("Indefined framebuffer.");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                LERROR("Incomplete, missing attachement.");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                LERROR("Framebuffer doesn't have at least one image attached to it.");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                LERROR(
                    "Returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is "
                    "GL_NONE for any color attachment point(s) named by GL_DRAW_BUFFERi."
                );
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                LERROR(
                    "Returned if GL_READ_BUFFER is not GL_NONE and the value of "
                    "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color "
                    "attachment point named by GL_READ_BUFFER.");
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                LERROR(
                    "Returned if the combination of internal formats of the attached "
                    "images violates an implementation - dependent set of restrictions."
                );
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                LERROR(
                    "Returned if the value of GL_RENDERBUFFE_r_samples is not the same "
                    "for all attached renderbuffers; if the value of GL_TEXTURE_SAMPLES "
                    "is the not same for all attached textures; or , if the attached "
                    "images are a mix of renderbuffers and textures, the value of "
                    "GL_RENDERBUFFE_r_samples does not match the value of "
                    "GL_TEXTURE_SAMPLES."
                );
                LERROR(
                    "Returned if the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not "
                    "the same for all attached textures; or , if the attached images are "
                    "a mix of renderbuffers and textures, the value of "
                    "GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not GL_TRUE for all attached "
                    "textures."
                );
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                LERROR(
                    "Returned if any framebuffer attachment is layered, and any "
                    "populated attachment is not layered, or if all populated color "
                    "attachments are not from textures of the same target."
                );
                break;
            default:
                LDEBUG("No error found checking framebuffer: " + codePosition);
                break;
            }
        }
    }
} // namespace

namespace openspace {

    documentation::Documentation ShadowComponent::Documentation() {
        using namespace documentation;
        return {
            "ShadowsRing Component",
            "globebrowsing_shadows_component",
            {
                {
                    DistanceFractionInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    DistanceFractionInfo.description
                },
                {
                    PolygonOffsetFactorInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    PolygonOffsetFactorInfo.description
                },
                {
                    PolygonOffsetUnitsInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    PolygonOffsetUnitsInfo.description
                }
            }
        };
    }

    ShadowComponent::ShadowComponent(const ghoul::Dictionary& dictionary)
        : properties::PropertyOwner({ "Shadows" })		
        , _saveDepthTexture(SaveDepthTextureInfo)
        , _distanceFraction(DistanceFractionInfo, 30, 1, 100000)
        , _enabled({ "Enabled", "Enabled", "Enable/Disable Shadows" }, true)
        , _enablePolygonOffset({ "Polygon Offset", "Polygon Offset", "Enable/Disable Polygon Offset" }, true)
        , _enableFaceCulling({ "Face Culling", "Face Culling", "Enable/Disable Face Culling" }, false)
        , _enableFrontFaceCull({ "Front Face Culling", "Front Face Culling", "Enable/Disable Front Face Culling" }, false)
        , _enableBackFaceCull({ "Back Face Culling", "Back Face Culling", "Enable/Disable Back Face Culling" }, true)
        , _polyOffFactor(PolygonOffsetFactorInfo, 2.5f, 0.f, 100000000000.f)
        , _polyOffUnits(PolygonOffsetUnitsInfo, 10.f, 0.f, 100000000000.f)
        , _shadowAcneEpsilon({ "Acne", "Acne", "Acne" }, glm::vec3(10), glm::vec3(-1E10), glm::vec3(1E10))
        , _shadowMapDictionary(dictionary)
        , _shadowDepthTextureHeight(1024)
        , _shadowDepthTextureWidth(1024)
        , _shadowDepthTexture(-1)
        , _positionInLightSpaceTexture(-1)
        , _shadowFBO(-1)
        , _firstPassSubroutine(-1)
        , _secondPassSubroutine(1)
        , _defaultFBO(-1)
        , _sunPosition(0.0)
        , _shadowMatrix(1.0)
        , _executeDepthTextureSave(false)
    {
        using ghoul::filesystem::File;

        if (dictionary.hasKey("Rings")) {
            ghoul::Dictionary ringsDic;
            dictionary.getValue("Rings", ringsDic);
            if (ringsDic.hasKey("Shadows")) {
                ringsDic.getValue("Shadows", _shadowMapDictionary);
            }
        }

        documentation::testSpecificationAndThrow(
            Documentation(),
            _shadowMapDictionary,
            "ShadowComponent"
        );

        if (_shadowMapDictionary.hasKey(DistanceFractionInfo.identifier)) {
            _distanceFraction = static_cast<int>(
                _shadowMapDictionary.value<float>(DistanceFractionInfo.identifier)
                );
        }

        if (_shadowMapDictionary.hasKey(PolygonOffsetFactorInfo.identifier)) {
            _polyOffFactor =
                _shadowMapDictionary.value<float>(PolygonOffsetFactorInfo.identifier);
        }

        if (_shadowMapDictionary.hasKey(PolygonOffsetUnitsInfo.identifier)) {
            _polyOffUnits =
                _shadowMapDictionary.value<float>(PolygonOffsetUnitsInfo.identifier);
        }

        _saveDepthTexture.onChange([&]() {
            _executeDepthTextureSave = true;
        });

        _enableFrontFaceCull.onChange([&]() {
            if (_enableFrontFaceCull) {
                _enableBackFaceCull = false;
            }
            else {
                _enableBackFaceCull = true;
            }
        });

        _enableBackFaceCull.onChange([&]() {
            if (_enableBackFaceCull) {
                _enableFrontFaceCull = false;
            }
            else {
                _enableFrontFaceCull = true;
            }
        });

        addProperty(_enabled);
        addProperty(_saveDepthTexture);
        addProperty(_distanceFraction);

        addProperty(_enablePolygonOffset);
        addProperty(_polyOffFactor);
        addProperty(_polyOffUnits);

        addProperty(_enableFaceCulling);
        addProperty(_enableFrontFaceCull);
        addProperty(_enableBackFaceCull);

        addProperty(_shadowAcneEpsilon);
    }

    void ShadowComponent::initialize()
    {
        using ghoul::filesystem::File;
    }

    bool ShadowComponent::isReady() const {
        return true;
    }

    void ShadowComponent::initializeGL() {
        createDepthTexture();
        createShadowFBO();
    }

    void ShadowComponent::deinitializeGL() {
        glDeleteTextures(1, &_shadowDepthTexture);
        glDeleteTextures(1, &_positionInLightSpaceTexture);
        glDeleteFramebuffers(1, &_shadowFBO);
        checkGLError("ShadowComponent::deinitializeGL() -- Deleted Textures and Framebuffer");
    }

    void ShadowComponent::begin(const RenderData& data) {
        // ===========================================
        // Builds light's ModelViewProjectionMatrix:
        // ===========================================
        
        glm::dvec3 diffVector = glm::dvec3(_sunPosition) - data.modelTransform.translation;
        double originalLightDistance = glm::length(diffVector);
        glm::dvec3 lightDirection = glm::normalize(diffVector);
        
        // Percentage of the original light source distance (to avoid artifacts)
        double multiplier = originalLightDistance * (static_cast<double>(_distanceFraction)/1.0E5);
        
        // New light source position
        glm::dvec3 lightPosition = data.modelTransform.translation + (lightDirection * multiplier);
       
        // Saving current Camera parameters
        Camera *camera = global::renderEngine.camera();
        _cameraPos = camera->positionVec3();
        _cameraFocus = camera->focusPositionVec3();
        _cameraRotation = camera->rotationQuaternion();

        //=============== Automatically Created Camera Matrix ===================
        //=======================================================================
        //glm::dmat4 lightViewMatrix = glm::lookAt(
        //    //lightPosition,
        //    glm::dvec3(0.0),
        //    //glm::dvec3(_sunPosition), // position
        //    glm::dvec3(data.modelTransform.translation), // focus 
        //    data.camera.lookUpVectorWorldSpace()  // up
        //    //glm::dvec3(0.0, 1.0, 0.0)
        //);

        //camera->setPositionVec3(lightPosition);
        //camera->setFocusPositionVec3(data.modelTransform.translation);
        //camera->setRotation(glm::dquat(glm::inverse(lightViewMatrix)));
        
        //=======================================================================
        //=======================================================================


        //=============== Manually Created Camera Matrix ===================
        //==================================================================
        // camera Z
        glm::dvec3 cameraZ = lightDirection;
        
        // camera X
        glm::dvec3 upVector = glm::dvec3(0.0, -1.0, 0.0);
        glm::dvec3 cameraX = glm::normalize(glm::cross(upVector, cameraZ)); 
        
        // camera Y
        glm::dvec3 cameraY = glm::cross(cameraZ, cameraX);

        // init 4x4 matrix
        glm::dmat4 cameraRotationMatrix(1.0);
        
        double* matrix = glm::value_ptr(cameraRotationMatrix);
        matrix[0] = cameraX.x;
        matrix[4] = cameraX.y;
        matrix[8] = cameraX.z;
        matrix[1] = cameraY.x;
        matrix[5] = cameraY.y;
        matrix[9] = cameraY.z;
        matrix[2] = cameraZ.x;
        matrix[6] = cameraZ.y;
        matrix[10] = cameraZ.z;

        // set translation part
        // We aren't setting the position here because it is set in the camera->setPosition()
        //matrix[12] = -glm::dot(cameraX, lightPosition);
        //matrix[13] = -glm::dot(cameraY, lightPosition);
        //matrix[14] = -glm::dot(cameraZ, lightPosition);

        /*Scene* scene = camera->parent()->scene();
        global::navigationHandler.setFocusNode(data.);
        */

        camera->setPositionVec3(lightPosition);
        camera->setFocusPositionVec3(data.modelTransform.translation);
        camera->setRotation(glm::dquat(glm::inverse(cameraRotationMatrix)));

        //=======================================================================
        //=======================================================================
        

        //============= Light Matrix by Camera Matrices Composition =============
        //=======================================================================
        glm::dmat4 lightProjectionMatrix = glm::dmat4(camera->projectionMatrix());
        //glm::dmat4 lightProjectionMatrix = glm::ortho(0.0f, static_cast<float>(_shadowDepthTextureWidth), 0.f,
        //    static_cast<float>(_shadowDepthTextureHeight), 0.1f, 100.0f);
        
        // The model transformation missing in the final shadow matrix is add when rendering each
        // object (using its transformations provided by the RenderData structure)
        _shadowData.shadowMatrix = 
            _toTextureCoordsMatrix * 
            lightProjectionMatrix * 
            camera->combinedViewMatrix();

        // temp
        _shadowData.worldToLightSpaceMatrix = camera->combinedViewMatrix();
        _shadowData.shadowAcneEpsilon = _shadowAcneEpsilon;

        checkGLError("begin() -- Saving Current GL State");
        // Saves current state
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);
        glGetIntegerv(GL_VIEWPORT, _mViewport);
        
        _faceCulling = glIsEnabled(GL_CULL_FACE);
        if (_faceCulling) {
            glGetIntegerv(GL_CULL_FACE_MODE, &_faceToCull);
        }

        _polygonOffSet = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        if (_polygonOffSet) {
            glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &_polygonOffSetFactor);
            glGetFloatv(GL_POLYGON_OFFSET_UNITS, &_polygonOffSetUnits);
        }
        
        glGetFloatv(GL_COLOR_CLEAR_VALUE, _colorClearValue);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &_depthClearValue);
        _depthIsEnabled = glIsEnabled(GL_DEPTH_TEST);

        checkGLError("begin() -- before binding FBO");
        glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
        checkGLError("begin() -- after binding FBO");
        glViewport(0, 0, _shadowDepthTextureWidth, _shadowDepthTextureHeight);
        checkGLError("begin() -- set new viewport");
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        checkGLError("begin() -- after cleanning Depth buffer");
        
        
        if (_enableFaceCulling)
        {
            glEnable(GL_CULL_FACE);
            checkGLError("begin() -- enabled cull face");

            if (_enableFrontFaceCull) {
                glCullFace(GL_FRONT);
            }
            else {
                glCullFace(GL_BACK);
            }
            
        }
        else {
            glDisable(GL_CULL_FACE);
        }
        
        if (_enablePolygonOffset) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            checkGLError("enableShadowOptions() -- enabled polygon offset fill");
            glPolygonOffset(_polyOffFactor, _polyOffUnits);
            checkGLError("enableShadowOptions() -- set values for polygon offset");
            std::cout << "--- PolyOff Factor: " << _polyOffFactor << ", PolyOff Units: " << _polyOffUnits << " ---" << std::endl;
        }

        checkGLError("begin() finished");
        
    }

    void ShadowComponent::end(const RenderData& /*data*/) {
        checkGLError("end() -- Flushing");
        //glFlush();
        if (_executeDepthTextureSave) {
            saveDepthBuffer();
            _executeDepthTextureSave = false;
        }

        // Restores Camera Parameters
        Camera *camera = global::renderEngine.camera();
        camera->setPositionVec3(_cameraPos);
        camera->setFocusPositionVec3(_cameraFocus);
        camera->setRotation(_cameraRotation);
        
        // Updating camera's matrices
        camera->combinedViewMatrix();

        // Restores system state
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
        checkGLError("end() -- Rebinding default FBO");
        glViewport(
            _mViewport[0],
            _mViewport[1],
            _mViewport[2],
            _mViewport[3]
        );
        
        if (_faceCulling) {
            glEnable(GL_CULL_FACE);
            glCullFace(_faceToCull);
        }
        else {
            glDisable(GL_CULL_FACE);
        }

        if (_depthIsEnabled) {
            glEnable(GL_DEPTH_TEST);
        }
        else {
            glDisable(GL_DEPTH_TEST);
        }

        if (_polygonOffSet) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(_polygonOffSetFactor, _polygonOffSetUnits);
        }
        else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        glClearColor(
            _colorClearValue[0], 
            _colorClearValue[1], 
            _colorClearValue[2], 
            _colorClearValue[3]
        );
        glClearDepth(_depthClearValue);

        checkGLError("end() finished");
    }

    void ShadowComponent::enableShadowOptions() {
        if (_enablePolygonOffset) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            checkGLError("enableShadowOptions() -- enabled polygon offset fill");
            glPolygonOffset(_polyOffFactor, _polyOffUnits);
            checkGLError("enableShadowOptions() -- set values for polygon offset");
            std::cout << "--- PolyOff Factor: " << _polyOffFactor << ", PolyOff Units: " << _polyOffUnits << " ---" << std::endl;
        }
    }

    void ShadowComponent::disableShadowOptions() {
        if (_polygonOffSet) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(_polygonOffSetFactor, _polygonOffSetUnits);
        }
        else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    void ShadowComponent::update(const UpdateData& /*data*/) {
        _sunPosition = global::renderEngine.scene()->sceneGraphNode("Sun")->worldPosition();
    }

    void ShadowComponent::createDepthTexture() {
        checkGLError("createDepthTexture() -- Starting configuration");
        glGenTextures(1, &_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, _shadowDepthTexture);
        /*glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            GL_DEPTH_COMPONENT32,
            _shadowDepthTextureWidth, 
            _shadowDepthTextureHeight
        );
*/
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT32F,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            0
        );
        checkGLError("createDepthTexture() -- Depth testure created");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, shadowBorder);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        checkGLError("createdDepthTexture");

        glGenTextures(1, &_positionInLightSpaceTexture);
        glBindTexture(GL_TEXTURE_2D, _positionInLightSpaceTexture);        
        //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(
            GL_TEXTURE_2D, 
            0, 
            GL_RGB32F, 
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight, 
            0, 
            GL_RGB, 
            GL_FLOAT, 
            nullptr
        );
        checkGLError("createDepthTexture() -- Position/Distance buffer created");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        checkGLError("createdPositionTexture");

        glBindTexture(GL_TEXTURE_2D, 0);

        _shadowData.shadowDepthTexture = _shadowDepthTexture;
        _shadowData.positionInLightSpaceTexture = _positionInLightSpaceTexture;
    }

    void ShadowComponent::createShadowFBO() {
        // Saves current FBO first
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);

        /*GLint _mViewport[4];
        glGetIntegerv(GL_VIEWPORT, _mViewport);*/

        glGenFramebuffers(1, &_shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, 
            GL_DEPTH_ATTACHMENT, 
            GL_TEXTURE_2D,
            _shadowDepthTexture, 
            0
        );

        glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT3,
            _positionInLightSpaceTexture,
            0
        );
        checkGLError("createShadowFBO() -- Created Shadow Framebuffer");
        //GLenum drawBuffers[] = { GL_NONE };
        GLenum drawBuffers[] = { GL_NONE, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3 };
        glDrawBuffers(4, drawBuffers);

        checkFrameBufferState("createShadowFBO()");

        // Restores system state
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
        /*glViewport(
            _mViewport[0],
            _mViewport[1],
            _mViewport[2],
            _mViewport[3]
        );*/
        checkGLError("createShadowFBO() -- createdShadowFBO");
    }

    void ShadowComponent::saveDepthBuffer() {
        int size = _shadowDepthTextureWidth * _shadowDepthTextureHeight;
        GLubyte * buffer = new GLubyte[size];

        glReadPixels(
            0,
            0,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_BYTE,
            buffer
        );

        checkGLError("readDepthBuffer To buffer");
        std::fstream ppmFile;

        ppmFile.open("depthBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight 
                    << std::endl;
            ppmFile << "255" << std::endl;

            std::cout << "\n\nSaving depth texture to file depthBufferShadowMapping.ppm\n\n";
            int k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++, k++) {
                    unsigned int val = static_cast<unsigned int>(buffer[k]);
                    ppmFile << val << " " << val << " " << val << " ";
                }
                ppmFile << std::endl;
            }

            ppmFile.close();

            std::cout << "Texture saved to file depthBufferShadowMapping.ppm\n\n";
        }

        delete[] buffer;

        GLfloat * bBuffer = new GLfloat[size * 4];

        glReadBuffer(GL_COLOR_ATTACHMENT3);
        glReadPixels(
            0,
            0,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            GL_RGBA,
            GL_FLOAT,
            bBuffer
        );

        checkGLError("readPositionBuffer To buffer");
        ppmFile.clear();

        ppmFile.open("positionBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight
                << std::endl;
            ppmFile << "255" << std::endl;

            std::cout << "\n\nSaving texture position to positionBufferShadowMapping.ppm\n\n";

            float biggestValue = 0.f;

            int k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++) {
                    biggestValue = bBuffer[k] > biggestValue ?
                        bBuffer[k] : biggestValue;
                    k += 4;
                }
            }

            biggestValue /= 255.f;

            k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++) {
                    ppmFile << static_cast<unsigned int>(bBuffer[k] / biggestValue) << " "
                        << static_cast<unsigned int>(bBuffer[k + 1] / biggestValue) << " "
                        << static_cast<unsigned int>(bBuffer[k + 2] / biggestValue) << " ";
                    k += 4;
                }
                ppmFile << std::endl;
            }

            ppmFile.close();

            std::cout << "Texture saved to file positionBufferShadowMapping.ppm\n\n";
        }

        delete[] bBuffer;
    }

    void ShadowComponent::checkGLError(const std::string & where) const {
        const GLenum error = glGetError();
        switch (error) {
        case GL_NO_ERROR:
            break;
        case GL_INVALID_ENUM:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_INVALID_ENUM", where)
            );
            break;
        case GL_INVALID_VALUE:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_INVALID_VALUE", where)
            );
            break;
        case GL_INVALID_OPERATION:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format(
                    "Function {}: GL_INVALID_OPERATION", where
                ));
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format(
                    "Function {}: GL_INVALID_FRAMEBUFFER_OPERATION",
                    where
                )
            );
            break;
        case GL_OUT_OF_MEMORY:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_OUT_OF_MEMORY", where)
            );
            break;
        default:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Unknown error code: {0:x}", static_cast<int>(error))
            );
        }
    }

    bool ShadowComponent::isEnabled() const {
        return _enabled;
    }

    ShadowComponent::ShadowMapData ShadowComponent::shadowMapData() const {
        return _shadowData;
    }
} // namespace openspace