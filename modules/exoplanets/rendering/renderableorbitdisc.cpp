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

#include <modules/exoplanets/rendering/renderableorbitdisc.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/scene/scene.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

namespace {
    static const openspace::properties::Property::PropertyInfo TextureInfo = {
        "Texture",
        "Texture",
        "This value is the path to a texture on disk that contains a one-dimensional "
        "texture which is used for these rings."
    };

    static const openspace::properties::Property::PropertyInfo SizeInfo = {
        "Size",
        "Size",
        "This value specifies the semi-major axis of the orbit in meter."
    };
    
    static const openspace::properties::Property::PropertyInfo EccentricityInfo = {
        "Eccentricity",
        "Eccentricity",
        "This value determines the eccentricity, that is the deviation from a perfect sphere, for this orbit."
    };

    static const openspace::properties::Property::PropertyInfo OffsetInfo = {
        "Offset",
        "Offset",
        "This value is used to limit the width of the rings. Each of the two values is "
        "the lower and the upper uncertainties of the semi-major axis. "
    };

    static const openspace::properties::Property::PropertyInfo TransparencyInfo = {
        "Transparency",
        "Transparency",
        "This value determines the transparency of part of the rings depending on the "
        "color values. For this value v, the transparency is equal to length(color) / v."
    };
} // namespace

namespace openspace {

documentation::Documentation RenderableOrbitdisc::Documentation() {
    using namespace documentation;
    return {
        "Renderable Orbitdisc",
        "exoplanets_renderable_orbitdisc",
        {
            {
                "Type",
                new StringEqualVerifier("RenderableOrbitdisc"),
                Optional::No
            },
            {
                TextureInfo.identifier,
                new StringVerifier,
                Optional::No,
                TextureInfo.description
            },
            {
                SizeInfo.identifier,
                new DoubleVerifier,
                Optional::No,
                SizeInfo.description
            },
            {
                EccentricityInfo.identifier,
                new DoubleVerifier,
                Optional::No,
                EccentricityInfo.description
            },
            {
                OffsetInfo.identifier,
                new DoubleVector2Verifier,
                Optional::Yes,
                OffsetInfo.description
            },
            {
                TransparencyInfo.identifier,
                new DoubleVerifier,
                Optional::Yes,
                TransparencyInfo.description
            }
        }
    };
}

RenderableOrbitdisc::RenderableOrbitdisc(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _texturePath(TextureInfo)
    , _size(SizeInfo, 1.f, 0.f, 3.0e12f)
    , _eccentricity(EccentricityInfo, 0.f, 0.f, 1.f)
    , _offset(OffsetInfo, glm::vec2(0.f, 1.f), glm::vec2(0.f), glm::vec2(1.f))
    , _transparency(TransparencyInfo, 0.15f, 0.f, 1.f)
    , _shader(nullptr)
    , _texture(nullptr)
    , _textureFile(nullptr)
    , _textureIsDirty(false)
    , _quad(0)
    , _vertexPositionBuffer(0)
    , _planeIsDirty(false)
{
    using ghoul::filesystem::File;

    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "RenderableOrbitdisc"
    );

    if (dictionary.hasKey(OffsetInfo.identifier)) {
        _offset = dictionary.value<glm::vec2>(OffsetInfo.identifier);
    }
    addProperty(_offset);

    _size = static_cast<float>(dictionary.value<double>(SizeInfo.identifier));
    _size = _size + (_offset.value().y * 149597870700);
    setBoundingSphere(_size);
    _size.onChange([&]() { _planeIsDirty = true; });
    addProperty(_size);

    _texturePath = absPath(dictionary.value<std::string>(TextureInfo.identifier));
    _textureFile = std::make_unique<File>(_texturePath);

    _texturePath.onChange([&]() { loadTexture(); });
    addProperty(_texturePath);

    _textureFile->setCallback([&](const File&) { _textureIsDirty = true; });

    if (dictionary.hasKey(TransparencyInfo.identifier)) {
        _transparency = static_cast<float>(
            dictionary.value<double>(TransparencyInfo.identifier)
        );
    }
    addProperty(_transparency);

    _eccentricity = static_cast<float>(dictionary.value<double>(EccentricityInfo.identifier));
    _eccentricity.onChange([&]() { _planeIsDirty = true; });
    addProperty(_eccentricity);
}

bool RenderableOrbitdisc::isReady() const {
    return _shader && _texture;
}

void RenderableOrbitdisc::initializeGL() {
    _shader = OsEng.renderEngine().buildRenderProgram(
        "OrbitdiscProgram",
        absPath("${BASE}/modules/exoplanets/shaders/orbitdisc_vs.glsl"),
        absPath("${BASE}/modules/exoplanets/shaders/orbitdisc_fs.glsl")
    );

    _uniformCache.modelViewProjection = _shader->uniformLocation(
        "modelViewProjectionTransform"
    );
    _uniformCache.textureOffset = _shader->uniformLocation("textureOffset");
    _uniformCache.transparency = _shader->uniformLocation("transparency");
    //_uniformCache.sunPosition = _shader->uniformLocation("sunPosition");
    _uniformCache.texture = _shader->uniformLocation("texture1");
    _uniformCache.eccentricity = _shader->uniformLocation("eccentricity");
    _uniformCache.semiMajorAxis = _shader->uniformLocation("semiMajorAxis");

    glGenVertexArrays(1, &_quad);
    glGenBuffers(1, &_vertexPositionBuffer);

    createPlane();
    loadTexture();
}

void RenderableOrbitdisc::deinitializeGL() {
    glDeleteVertexArrays(1, &_quad);
    _quad = 0;

    glDeleteBuffers(1, &_vertexPositionBuffer);
    _vertexPositionBuffer = 0;

    _textureFile = nullptr;
    _texture = nullptr;

    OsEng.renderEngine().removeRenderProgram(_shader.get());
    _shader = nullptr;
}

void RenderableOrbitdisc::render(const RenderData& data, RendererTasks&) {
    _shader->activate();

	glm::dmat4 modelTransform =
		glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
		glm::dmat4(data.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

    glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() * modelTransform;

    _shader->setUniform(
        _uniformCache.modelViewProjection,
        data.camera.projectionMatrix() * glm::mat4(modelViewTransform)
    );
    _shader->setUniform(_uniformCache.textureOffset, _offset);
    _shader->setUniform(_uniformCache.transparency, _transparency);
    _shader->setUniform(_uniformCache.eccentricity, _eccentricity);
    _shader->setUniform(_uniformCache.semiMajorAxis, _size);
    //_shader->setUniform(_uniformCache.sunPosition, _sunPosition);

    //setPscUniforms(*_shader, data.camera, data.position);

    ghoul::opengl::TextureUnit unit;
    unit.activate();
    _texture->bind();
    _shader->setUniform(_uniformCache.texture, unit);

    glDisable(GL_CULL_FACE);

    glBindVertexArray(_quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_CULL_FACE);
    _shader->deactivate();
}

void RenderableOrbitdisc::update(const UpdateData& data) {
    if (_shader->isDirty()) {
        _shader->rebuildFromFile();

        _uniformCache.modelViewProjection = _shader->uniformLocation(
            "modelViewProjectionTransform"
        );
        _uniformCache.textureOffset = _shader->uniformLocation("textureOffset");
        _uniformCache.transparency = _shader->uniformLocation("transparency");
        //_uniformCache.sunPosition = _shader->uniformLocation("sunPosition");
        _uniformCache.texture = _shader->uniformLocation("texture1");
        _uniformCache.eccentricity = _shader->uniformLocation("eccentricity");
        _uniformCache.semiMajorAxis = _shader->uniformLocation("semiMajorAxis");
    }

    if (_planeIsDirty) {
        createPlane();
        _planeIsDirty = false;
    }

    if (_textureIsDirty) {
        loadTexture();
        _textureIsDirty = false;
    }

    //_sunPosition = OsEng.renderEngine().scene()->sceneGraphNode("Sun")->worldPosition() -
        //data.modelTransform.translation;
}

void RenderableOrbitdisc::loadTexture() {
    if (_texturePath.value() != "") {
        std::unique_ptr<ghoul::opengl::Texture> texture =
            ghoul::io::TextureReader::ref().loadTexture(absPath(_texturePath));

        if (texture) {
            LDEBUGC(
                "RenderableOrbitdisc",
                fmt::format("Loaded texture from '{}'", absPath(_texturePath))
            );
            _texture = std::move(texture);

            _texture->uploadTexture();
            _texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);

            _textureFile = std::make_unique<ghoul::filesystem::File>(_texturePath);
            _textureFile->setCallback(
                [&](const ghoul::filesystem::File&) { _textureIsDirty = true; }
            );
        }
    }
}

void RenderableOrbitdisc::createPlane() {
    const GLfloat size = _size.value() * (1.0 + _eccentricity.value());

    struct VertexData {
        GLfloat x;
        GLfloat y;
        GLfloat s;
        GLfloat t;
    };

    VertexData data[] = {
        { -size, -size, 0.f, 0.f },
        {  size,  size, 1.f, 1.f },
        { -size,  size, 0.f, 1.f },
        { -size, -size, 0.f, 0.f },
        {  size, -size, 1.f, 0.f },
        {  size,  size, 1.f, 1.f },
    };

    glBindVertexArray(_quad);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexData),
        nullptr
    );
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexData),
        reinterpret_cast<void*>(offsetof(VertexData, s))
    );
}

} // namespace openspace