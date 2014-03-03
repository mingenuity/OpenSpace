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

#include "properties/intproperty.h"

namespace openspace {
namespace properties {

template <>
std::string PropertyDelegate<IntProperty>::className() {
    return "IntProperty";
}

template <>
std::string PropertyDelegate<TemplateProperty<int>>::className() {
    return "IntProperty";
}

template <>
template <>
int PropertyDelegate<IntProperty>::defaultValue<int>() {
    return 0;
}

//template <>
//std::string classNameDelegate<IntProperty>() { return "IntProperty"; }

} // namespace properties
} // namespace openspace

//IntProperty::IntProperty(const std::string& identifier, const std::string& guiName,
//    int value, int minimumValue, int maximumValue, int stepping)
//    : NumericalProperty<int>(identifier, guiName, value, minimumValue, maximumValue, stepping)
//{
//
//}

//std::string IntProperty::className() const {
//    return "IntProperty";
//}
