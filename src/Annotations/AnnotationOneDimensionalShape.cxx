
/*LICENSE_START*/
/*
 *  Copyright (C) 2015 Washington University School of Medicine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*LICENSE_END*/

#define __ANNOTATION_ONE_DIMENSIONAL_SHAPE_DECLARE__
#include "AnnotationOneDimensionalShape.h"
#undef __ANNOTATION_ONE_DIMENSIONAL_SHAPE_DECLARE__

#include "AnnotationCoordinate.h"
#include "AnnotationSpatialModification.h"
#include "CaretAssert.h"
#include "SceneClass.h"
#include "SceneClassAssistant.h"

using namespace caret;


    
/**
 * \class caret::AnnotationOneDimensionalShape 
 * \brief Class for annotations that are one-dimensional (lines)
 * \ingroup Annotations
 */

/**
 * Constructor.
 * @param type
 *    Type of annotation.
 * @param attributeDefaultType
 *    Type for attribute defaults
 */
AnnotationOneDimensionalShape::AnnotationOneDimensionalShape(const AnnotationTypeEnum::Enum type,
                                                             const AnnotationAttributesDefaultTypeEnum::Enum attributeDefaultType)
: Annotation(type,
             attributeDefaultType)
{
    initializeMembersAnnotationOneDimensionalShape();
}

/**
 * Destructor.
 */
AnnotationOneDimensionalShape::~AnnotationOneDimensionalShape()
{
}

/**
 * Copy constructor.
 * @param obj
 *    Object that is copied.
 */
AnnotationOneDimensionalShape::AnnotationOneDimensionalShape(const AnnotationOneDimensionalShape& obj)
: Annotation(obj)
{
    initializeMembersAnnotationOneDimensionalShape();
    
    this->copyHelperAnnotationOneDimensionalShape(obj);
}

/**
 * Assignment operator.
 * @param obj
 *    Data copied from obj to this.
 * @return 
 *    Reference to this object.
 */
AnnotationOneDimensionalShape&
AnnotationOneDimensionalShape::operator=(const AnnotationOneDimensionalShape& obj)
{
    if (this != &obj) {
        Annotation::operator=(obj);
        this->copyHelperAnnotationOneDimensionalShape(obj);
    }
    return *this;    
}

/**
 * Helps with copying an object of this type.
 * @param obj
 *    Object that is copied.
 */
void 
AnnotationOneDimensionalShape::copyHelperAnnotationOneDimensionalShape(const AnnotationOneDimensionalShape& obj)
{
    *m_startCoordinate = *obj.m_startCoordinate;
    *m_endCoordinate   = *obj.m_endCoordinate;
}

/**
 * Initialize members of this class.
 */
void
AnnotationOneDimensionalShape::initializeMembersAnnotationOneDimensionalShape()
{
    m_startCoordinate.grabNew(new AnnotationCoordinate());
    m_endCoordinate.grabNew(new AnnotationCoordinate());
    
    m_sceneAssistant.grabNew(new SceneClassAssistant());
    m_sceneAssistant->add("m_startCoordinate",
                          "AnnotationCoordinate",
                          m_startCoordinate);
    m_sceneAssistant->add("m_endCoordinate",
                          "AnnotationCoordinate",
                          m_endCoordinate);
}

/**
 * @return The start coordinate for the one dimensional shape.
 */
AnnotationCoordinate*
AnnotationOneDimensionalShape::getStartCoordinate()
{
    return m_startCoordinate;
}

/**
 * @return The start coordinate for the one dimensional shape.
 */
const AnnotationCoordinate*
AnnotationOneDimensionalShape::getStartCoordinate() const
{
    return m_startCoordinate;
}

/**
 * @return The end coordinate for the one dimensional shape.
 */
AnnotationCoordinate*
AnnotationOneDimensionalShape::getEndCoordinate()
{
    return m_endCoordinate;
}

/**
 * @return The end coordinate for the one dimensional shape.
 */
const AnnotationCoordinate*
AnnotationOneDimensionalShape::getEndCoordinate() const
{
    return m_endCoordinate;
}

/**
 * Is the object modified?
 * @return true if modified, else false.
 */
bool
AnnotationOneDimensionalShape::isModified() const
{
    if (Annotation::isModified()) {
        return true;
    }
    
    if (m_startCoordinate->isModified()) {
        return true;
    }
    
    if (m_endCoordinate->isModified()) {
        return true;
    }
    
    return false;
}

/**
 * Set the status to unmodified.
 */
void
AnnotationOneDimensionalShape::clearModified()
{
    Annotation::clearModified();
    
    m_startCoordinate->clearModified();
    m_endCoordinate->clearModified();
}

/**
 * Apply the coordinates, size, and rotation from the given annotation
 * to this annotation.
 * 
 * @param otherAnnotation
 *     The other annotation from which attributes are obtained.
 */
void
AnnotationOneDimensionalShape::applyCoordinatesSizeAndRotationFromOther(const Annotation* otherAnnotation)
{
    CaretAssert(otherAnnotation);
    const AnnotationOneDimensionalShape* otherOneDim = dynamic_cast<const AnnotationOneDimensionalShape*>(otherAnnotation);
    CaretAssert(otherOneDim);
    
    AnnotationCoordinate* startCoord = getStartCoordinate();
    const AnnotationCoordinate* otherStartCoord = otherOneDim->getStartCoordinate();
    *startCoord = *otherStartCoord;
    
    AnnotationCoordinate* endCoord = getEndCoordinate();
    const AnnotationCoordinate* otherEndCoord = otherOneDim->getEndCoordinate();
    *endCoord = *otherEndCoord;
    
    setCoordinateSpace(otherAnnotation->getCoordinateSpace());
    setTabIndex(otherAnnotation->getTabIndex());
    setWindowIndex(otherAnnotation->getWindowIndex());
}

/**
 * Is the given sizing handle valid for this annotation?
 *
 * @sizingHandle
 *    The sizing handle.
 * @return
 *    True if sizing handle valid, else false.
 */
bool
AnnotationOneDimensionalShape::isSizeHandleValid(const AnnotationSizingHandleTypeEnum::Enum sizingHandle) const
{
    bool pixelsFlag      = false;
    bool tabWindowFlag   = false;
    bool stereotaxicFlag = false;
    bool surfaceFlag     = false;
    
    switch (getCoordinateSpace()) {
        case AnnotationCoordinateSpaceEnum::PIXELS:
            pixelsFlag = true;
            break;
        case AnnotationCoordinateSpaceEnum::STEREOTAXIC:
            stereotaxicFlag = true;
            break;
        case AnnotationCoordinateSpaceEnum::SURFACE:
            surfaceFlag = true;
            break;
        case AnnotationCoordinateSpaceEnum::TAB:
            tabWindowFlag = true;
            break;
        case AnnotationCoordinateSpaceEnum::WINDOW:
            tabWindowFlag = true;
            break;
    }
    
    bool validFlag = false;
    
    if ( ! pixelsFlag) {
        switch (sizingHandle) {
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM_LEFT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM_RIGHT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_LEFT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_RIGHT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP_LEFT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP_RIGHT:
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_LINE_END:
                validFlag = true;
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_LINE_START:
                validFlag = true;
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_NONE:
                if (tabWindowFlag) {
                    validFlag = true;
                }
                break;
            case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_ROTATION:
                break;
        }
    }
    
    return validFlag;
}

/**
 * Apply a spatial modification to an annotation.
 *
 * @param spatialModification
 *     Contains information about the spatial modification.
 * @return
 *     True if the annotation was modified, else false.
 */
bool
AnnotationOneDimensionalShape::applySpatialModification(const AnnotationSpatialModification& spatialModification)
{
    if ( ! isSizeHandleValid(spatialModification.m_sizingHandleType)) {
        return false;
    }
    
    bool tabWindowFlag   = false;
    bool stereotaxicFlag = false;
    bool surfaceFlag     = false;
    
    /*
     * Verify that the coordinate space of this annotation matches
     * the coordinate space in the spatial modification
     */
    switch (getCoordinateSpace()) {
        case AnnotationCoordinateSpaceEnum::PIXELS:
            break;
        case AnnotationCoordinateSpaceEnum::STEREOTAXIC:
            if (spatialModification.m_stereotaxicCoordinateAtMouseXY.m_stereotaxicValid) {
                stereotaxicFlag = true;
            }
            break;
        case AnnotationCoordinateSpaceEnum::SURFACE:
        {
            StructureEnum::Enum structure = StructureEnum::INVALID;
            int32_t surfaceNumberOfNodes  = -1;
            int32_t surfaceNodeIndex      = -1;
            float surfaceOffset           = -1.0;

            m_startCoordinate->getSurfaceSpace(structure,
                                          surfaceNumberOfNodes,
                                          surfaceNodeIndex,
                                          surfaceOffset);
            if (spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNodeValid) {
                if ((spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceStructure == structure)
                    && (spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNumberOfNodes == surfaceNumberOfNodes)) {
                    surfaceFlag = true;
                }
            }
        }
            break;
        case AnnotationCoordinateSpaceEnum::TAB:
            tabWindowFlag = true;
            break;
        case AnnotationCoordinateSpaceEnum::WINDOW:
            tabWindowFlag = true;
            break;
    }
    
    float xyz1[3];
    float xyz2[3];
    m_startCoordinate->getXYZ(xyz1);
    m_endCoordinate->getXYZ(xyz2);
    
    float newX1 = xyz1[0];
    float newY1 = xyz1[1];
    float newX2 = xyz2[0];
    float newY2 = xyz2[1];
    
    const float spaceDX = 100.0 * ((spatialModification.m_viewportWidth != 0.0)
                                   ? (spatialModification.m_mouseDX / spatialModification.m_viewportWidth)
                                   : 0.0);
    const float spaceDY = 100.0 * ((spatialModification.m_viewportHeight != 0.0)
                                   ? (spatialModification.m_mouseDY / spatialModification.m_viewportHeight)
                                   : 0.0);
    bool validFlag = false;
    switch (spatialModification.m_sizingHandleType) {
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM_LEFT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_BOTTOM_RIGHT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_LEFT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_RIGHT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP_LEFT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_BOX_TOP_RIGHT:
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_LINE_END:
            if (stereotaxicFlag) {
                m_endCoordinate->setXYZ(spatialModification.m_stereotaxicCoordinateAtMouseXY.m_stereotaxicXYZ);
                validFlag = true;
            }
            else if (surfaceFlag) {
                m_endCoordinate->setSurfaceSpace(spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceStructure,
                                                 spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNumberOfNodes,
                                                 spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNodeIndex,
                                                 m_endCoordinate->getDefaultSurfaceOffsetLength());
                validFlag = true;
            }
            else if (tabWindowFlag) {
                newX2 += spaceDX;
                newY2 += spaceDY;
                validFlag = true;
            }
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_LINE_START:
            if (stereotaxicFlag) {
                m_startCoordinate->setXYZ(spatialModification.m_stereotaxicCoordinateAtMouseXY.m_stereotaxicXYZ);
                validFlag = true;
            }
            else if (surfaceFlag) {
                m_startCoordinate->setSurfaceSpace(spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceStructure,
                                                 spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNumberOfNodes,
                                                 spatialModification.m_surfaceCoordinateAtMouseXY.m_surfaceNodeIndex,
                                                 m_startCoordinate->getDefaultSurfaceOffsetLength());
                validFlag = true;
            }
            else if (tabWindowFlag) {
                newX1 += spaceDX;
                newY1 += spaceDY;
                validFlag = true;
            }
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_NONE:
            if (tabWindowFlag) {
                newX1 += spaceDX;
                newY1 += spaceDY;
                newX2 += spaceDX;
                newY2 += spaceDY;
                validFlag = true;
            }
            break;
        case AnnotationSizingHandleTypeEnum::ANNOTATION_SIZING_HANDLE_ROTATION:
            break;
    }
    
    if (validFlag) {
        if (tabWindowFlag) {
            if ((newX1 >= 0.0)
                && (newX1 <= 100.0)
                && (newY1 >= 0.0)
                && (newY1 <= 100.0)
                && (newX2 >= 0.0)
                && (newX2 <= 100.0)
                && (newY2 >= 0.0)
                && (newY2 <= 100.0)) {
                xyz1[0] = newX1;
                xyz1[1] = newY1;
                m_startCoordinate->setXYZ(xyz1);
                xyz2[0] = newX2;
                xyz2[1] = newY2;
                m_endCoordinate->setXYZ(xyz2);
            }
            else {
                validFlag = false;
            }
        }
    }
    
    return validFlag;
}

/**
 * Save subclass data to the scene.
 *
 * @param sceneAttributes
 *    Attributes for the scene.  Scenes may be of different types
 *    (full, generic, etc) and the attributes should be checked when
 *    restoring the scene.
 *
 * @param sceneClass
 *     sceneClass to which data members should be added.  Will always
 *     be valid (non-NULL).
 */
void
AnnotationOneDimensionalShape::saveSubClassDataToScene(const SceneAttributes* sceneAttributes,
                                            SceneClass* sceneClass)
{
    m_sceneAssistant->saveMembers(sceneAttributes,
                                  sceneClass);
}

/**
 * Restore file data from the scene.
 *
 * @param sceneAttributes
 *    Attributes for the scene.  Scenes may be of different types
 *    (full, generic, etc) and the attributes should be checked when
 *    restoring the scene.
 *
 * @param sceneClass
 *     sceneClass for the instance of a class that implements
 *     this interface.  Will NEVER be NULL.
 */
void
AnnotationOneDimensionalShape::restoreSubClassDataFromScene(const SceneAttributes* sceneAttributes,
                                                 const SceneClass* sceneClass)
{
    m_sceneAssistant->restoreMembers(sceneAttributes,
                                     sceneClass);
}

