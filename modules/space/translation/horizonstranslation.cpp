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

#include <modules/space/translation/horizonstranslation.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>

#include <ghoul/fmt.h>
#include <ghoul/lua/ghoul_lua.h>
#include <ghoul/lua/lua_helper.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

#include <fstream>

namespace {
    const char* _loggerCat = "HorizonsTranslation";
}

namespace {
    static const openspace::properties::Property::PropertyInfo HorizonsTextFileInfo = {
        "HorizonsTextFile",
        "Horizons Text File",
        "This value is the path to the text file generated by Horizons with observer range "
        "and Galactiv longitude and latitude for different timestamps."
    };
} // namespace

namespace openspace {

documentation::Documentation HorizonsTranslation::Documentation() {
    using namespace documentation;
    return {
        "Horizons Translation",
        "base_transform_translation_horizons",
        {
            {
                "Type",
                new StringEqualVerifier("HorizonsTranslation"),
                Optional::No
            },
            {
                HorizonsTextFileInfo.identifier,
                new StringVerifier,
                Optional::No,
                HorizonsTextFileInfo.description
            }
        }
    };
}


HorizonsTranslation::HorizonsTranslation()
    : _horizonsTextFile(HorizonsTextFileInfo)
{
    _timeline = Timeline<glm::dvec3>();

    addProperty(_horizonsTextFile);

    _horizonsTextFile.onChange([&](){
        requireUpdate();
        _fileHandle = std::make_unique<ghoul::filesystem::File>(_horizonsTextFile);
        _fileHandle->setCallback([&](const ghoul::filesystem::File&) {
             requireUpdate();
             notifyObservers();
         });
        readHorizonsTextFile(_horizonsTextFile);
    });
}

HorizonsTranslation::HorizonsTranslation(const ghoul::Dictionary& dictionary)
    : HorizonsTranslation()
{
    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "HorizonsTranslation"
    );

    _horizonsTextFile = 
        absPath(dictionary.value<std::string>(HorizonsTextFileInfo.identifier));

    // Read specified file and store it in memory. 
    readHorizonsTextFile(_horizonsTextFile);
}

glm::dvec3 HorizonsTranslation::position(const UpdateData& data) const {
    glm::dvec3 interpolatedPos = glm::dvec3(0.0);

    auto lastBefore = _timeline.lastKeyframeBefore(data.time.j2000Seconds(), true);
    auto firstAfter = _timeline.firstKeyframeAfter(data.time.j2000Seconds(), false);
    if (lastBefore && firstAfter) {
        // We're inbetween first and last value.
        double timelineDiff = firstAfter->timestamp - lastBefore->timestamp;
        double timeDiff = data.time.j2000Seconds() - lastBefore->timestamp;
        double diff = (timelineDiff > DBL_EPSILON) ? timeDiff / timelineDiff : 0.0;

        glm::dvec3 dir = firstAfter->data - lastBefore->data;
        interpolatedPos = lastBefore->data + dir * diff;
    }
    else if (lastBefore) {
        // Requesting a time after last value. Return last known position.
        interpolatedPos = lastBefore->data;
    }
    else if (firstAfter) {
        // Requesting a time before first value. Return last known position.
        interpolatedPos = firstAfter->data;
    }

    return interpolatedPos;
}

void HorizonsTranslation::readHorizonsTextFile(const std::string& _horizonsTextFilePath) {
    
    std::ifstream fileStream(_horizonsTextFilePath);

    if (!fileStream.good()) {
        LERROR(fmt::format("Failed to open Horizons text file '{}'", 
            _horizonsTextFilePath));
        return;
    }

    // The beginning of a Horizons file has a header with a lot of information about the
    // query that we do not care about. Ignore everything until data starts, including 
    // the row marked by $$SOE (i.e. Start Of Ephemerides).
    std::string line = "";
    while (line[0] != '$') {
        std::getline(fileStream, line);
    }

    // Read data line by line until $$EOE (i.e. End Of Ephemerides).
    // Skip the rest of the file. 
    std::getline(fileStream, line);
    while (line[0] != '$') {
        std::stringstream str(line);
        std::string date;
        std::string time;
        float range = 0;
        float gLon = 0;
        float gLat = 0;
        
        // File is structured by: 
        // YYYY-MM-DD 
        // HH:MM:SS 
        // Range-to-observer (km)
        // Range-delta (km/s) -- suppressed!
        // Galactic Longitude (degrees)
        // Galactic Latitude (degrees)
        str >> date >> time >> range >> gLon >> gLat;

        // Convert date and time to seconds after 2000 
        // and pos to Galactic positions in meter from Observer.
        std::string timeString = date + " " + time;
        double timeInJ2000 = Time::convertTime(timeString);
        glm::dvec3 gPos = glm::dvec3(
            1000 * range * cos(glm::radians(gLat)) * cos(glm::radians(gLon)),
            1000 * range * cos(glm::radians(gLat)) * sin(glm::radians(gLon)),
            1000 * range * sin(glm::radians(gLat))
        );

        // Add position to stored timeline. 
        _timeline.addKeyframe(timeInJ2000, gPos);

        std::getline(fileStream, line);
    }
    fileStream.close();
}

} // namespace openspace
