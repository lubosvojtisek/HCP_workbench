

/*
 *  Copyright 1995-2002 Washington University School of Medicine
 *
 *  http://brainmap.wustl.edu
 *
 *  This file is part of CARET.
 *
 *  CARET is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  CARET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CARET; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*LICENSE_END*/

#include <cstdlib>

#ifdef CARET_OS_WINDOWS
#include <Windows.h>
#endif
#ifdef CARET_OS_MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <QFont>

#define __BRAIN_OPENGL_FIXED_PIPELINE_DEFINE_H
#include "BrainOpenGLFixedPipeline.h"
#undef __BRAIN_OPENGL_FIXED_PIPELINE_DEFINE_H

#include "Brain.h"
#include "BrainOpenGLViewportContent.h"
#include "BrainStructure.h"
#include "BrainStructureNodeAttributes.h"
#include "BrowserTabContent.h"
#include "BrowserTabYoking.h"
#include "BoundingBox.h"
#include "CaretAssert.h"
#include "CaretLogger.h"
#include "CaretMappableDataFile.h"
#include "CaretPreferences.h"
#include "ConnectivityLoaderFile.h"
#include "DescriptiveStatistics.h"
#include "DisplayPropertiesVolume.h"
#include "ElapsedTimer.h"
#include "IdentificationItemSurfaceNode.h"
#include "IdentificationItemSurfaceTriangle.h"
#include "IdentificationItemVoxel.h"
#include "IdentificationWithColor.h"
#include "IdentificationManager.h"
#include "MathFunctions.h"
#include "NodeAndVoxelColoring.h"
#include "Overlay.h"
#include "OverlaySet.h"
#include "Palette.h"
#include "PaletteColorMapping.h"
#include "PaletteFile.h"
#include "PaletteScalarAndColor.h"
#include "Plane.h"
#include "SessionManager.h"
#include "SphereOpenGL.h"
#include "Surface.h"
#include "ModelDisplayControllerSurface.h"
#include "ModelDisplayControllerVolume.h"
#include "ModelDisplayControllerWholeBrain.h"
#include "VolumeFile.h"
#include "VolumeSurfaceOutlineSelection.h"

using namespace caret;

/**
 * Constructor.
 *
 * @param parentGLWidget
 *   The optional text renderer is used for text rendering.
 *   This parameter may be NULL in which case no text
 *   rendering is performed.
 */
BrainOpenGLFixedPipeline::BrainOpenGLFixedPipeline(BrainOpenGLTextRenderInterface* textRenderer)
: BrainOpenGL(textRenderer)
{
    this->initializeMembersBrainOpenGL();
    this->colorIdentification   = new IdentificationWithColor();
    this->sphereDisplayList = 0;
    this->sphereOpenGL = NULL;
}

/**
 * Destructor.
 */
BrainOpenGLFixedPipeline::~BrainOpenGLFixedPipeline()
{
    if (this->sphereDisplayList > 0) {
        glDeleteLists(this->sphereDisplayList, 1);
    }
    if (this->sphereOpenGL != NULL) {
        delete this->sphereOpenGL;
        this->sphereOpenGL = NULL;
    }
    delete this->colorIdentification;
    this->colorIdentification = NULL;
}

/**
 * Selection on a model.
 *
 * @param viewportConent
 *    Viewport content in which mouse was clicked
 * @param mouseX
 *    X position of mouse click
 * @param mouseY
 *    Y position of mouse click
 */
void 
BrainOpenGLFixedPipeline::selectModel(BrainOpenGLViewportContent* viewportContent,
                         const int32_t mouseX,
                         const int32_t mouseY)
{
    /*
     * For identification, set the background
     * to white.
     */
    glClearColor(1.0, 1.0, 1.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);    
    
    this->mouseX = mouseX;
    this->mouseY = mouseY;
    
    this->colorIdentification->reset();

    this->drawModelInternal(MODE_IDENTIFICATION,
                            viewportContent);

    this->getIdentificationManager()->filterSelections();
}

/**
 * Draw models in their respective viewports.
 *
 * @param viewportContents
 *    Viewport info for drawing.
 */
void 
BrainOpenGLFixedPipeline::drawModels(std::vector<BrainOpenGLViewportContent*>& viewportContents)
{
    float backgroundColor[3];
    CaretPreferences* prefs = SessionManager::get()->getCaretPreferences();
    prefs->getColorBackground(backgroundColor);
    glClearColor(backgroundColor[0],
                 backgroundColor[1],
                 backgroundColor[2],
                 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);    
    
    for (int32_t i = 0; i < static_cast<int32_t>(viewportContents.size()); i++) {
        this->drawModelInternal(MODE_DRAWING,
                                viewportContents[i]);
    }
}
/**
 * Draw a model.
 *
 * @param mode
 *    The mode of operations (draw, select, etc).
 * @param viewportContent
 *    Viewport contents for drawing.
 */
void 
BrainOpenGLFixedPipeline::drawModelInternal(Mode mode,
                               BrainOpenGLViewportContent* viewportContent)
{
    this->browserTabContent= viewportContent->getBrowserTabContent();
    ModelDisplayController* modelDisplayController = this->browserTabContent->getModelControllerForDisplay();
    this->windowTabIndex = this->browserTabContent->getTabNumber();
    int viewport[4];
    viewportContent->getViewport(viewport);
    
    ElapsedTimer et;
    et.start();
    
    this->mode = mode;
    
    /*
     * Update transformations with those from the yoked 
     * group.  Does nothing if not yoked.
     */
    browserTabContent->updateTransformationsForYoking();
    
    if(modelDisplayController != NULL) {
        CaretAssert((this->windowTabIndex >= 0) && (this->windowTabIndex < BrainConstants::MAXIMUM_NUMBER_OF_BROWSER_TABS));
                
        ModelDisplayControllerSurface* surfaceController = dynamic_cast<ModelDisplayControllerSurface*>(modelDisplayController);
        ModelDisplayControllerVolume* volumeController = dynamic_cast<ModelDisplayControllerVolume*>(modelDisplayController);
        ModelDisplayControllerWholeBrain* wholeBrainController = dynamic_cast<ModelDisplayControllerWholeBrain*>(modelDisplayController);
        if (surfaceController != NULL) {
            this->drawSurfaceController(surfaceController, viewport);
        }
        else if (volumeController != NULL) {
            this->drawVolumeController(browserTabContent,
                                       volumeController, viewport);
        }
        else if (wholeBrainController != NULL) {
            this->drawWholeBrainController(browserTabContent,
                                           wholeBrainController, viewport);
        }
        else {
            CaretAssertMessage(0, "Unknown type of model display controller for drawing");
        }
        
        int viewport[4];
        viewportContent->getViewport(viewport);
        this->drawAllPalettes(modelDisplayController->getBrain());
    }
    
    glFlush();
    
    this->checkForOpenGLError(modelDisplayController, "At end of drawModel()");
    
    if (modelDisplayController != NULL) {
        CaretLogFine("Time to draw " 
                    + modelDisplayController->getNameForGUI(false)
                    + " was "
                    + AString::number(et.getElapsedTimeSeconds())
                    + " seconds");
    }
}

/**
 * Set the viewport.
 * @param viewport
 *   Values for viewport (x, y, x-size, y-size)
 * @param isRightSurfaceLateralMedialYoked
 *    True if the displayed model is a right surface that is 
 *    lateral/medial yoked.
 */
void 
BrainOpenGLFixedPipeline::setViewportAndOrthographicProjection(const int32_t viewport[4],
                                                  const bool isRightSurfaceLateralMedialYoked)
{
    glViewport(viewport[0], 
               viewport[1], 
               viewport[2], 
               viewport[3]); 
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    this->setOrthographicProjection(viewport,
                                    isRightSurfaceLateralMedialYoked);
    glMatrixMode(GL_MODELVIEW);
}

/**
 * Apply the viewing transformations for the model controller 
 * in the given tab.
 *
 * @param modelDisplayController
 *    Model controller being viewed.
 * @param tabIndex
 *    Index of tab containing the controller.
 * @param objectCenterXYZ
 *    If not NULL, contains center of object about
 *    which rotation should take place.
 * @param isRightSurfaceLateralMedialYoked
 *    True if the displayed model is a right surface that is 
 *    lateral/medial yoked.
 */
void 
BrainOpenGLFixedPipeline::applyViewingTransformations(const ModelDisplayController* modelDisplayController,
                                         const int32_t tabIndex,
                                         const float objectCenterXYZ[3],
                                         const bool isRightSurfaceLateralMedialYoked)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    const float* translation = modelDisplayController->getTranslation(tabIndex);
    glTranslatef(translation[0], 
                 translation[1], 
                 translation[2]);
    
    const int32_t matrixIndex = (isRightSurfaceLateralMedialYoked ? 1 : 0);
    const Matrix4x4* rotationMatrix = modelDisplayController->getViewingRotationMatrix(tabIndex,
                                                                                       matrixIndex);
    double rotationMatrixElements[16];
    rotationMatrix->getMatrixForOpenGL(rotationMatrixElements);
    glMultMatrixd(rotationMatrixElements);
    
    const float scale = modelDisplayController->getScaling(tabIndex);
    glScalef(scale, 
             scale, 
             scale);    

    if (objectCenterXYZ != NULL) {
        /*
         * Place center of surface at origin.
         */
        glTranslatef(-objectCenterXYZ[0], -objectCenterXYZ[1], -objectCenterXYZ[2]); 
    }
}

/**
 * Apply the viewing transformations for the model controller 
 * in the given tab for viewing a volume slice in a plane.
 *
 * @param modelDisplayController
 *    Model controller being viewed.
 * @param tabIndex
 *    Index of tab containing the controller.
 * @param viewPlane
 *    View plane of slice.
 */
void 
BrainOpenGLFixedPipeline::applyViewingTransformationsVolumeSlice(const ModelDisplayController* modelDisplayController,
                                            const int32_t tabIndex,
                                            const VolumeSliceViewPlaneEnum::Enum viewPlane)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    const float* translation = modelDisplayController->getTranslation(tabIndex);
    glTranslatef(translation[0], 
                 translation[1], 
                 translation[2]);
    
    Matrix4x4 rotationMatrix;
    switch (viewPlane) {
        case VolumeSliceViewPlaneEnum::ALL:
            break;
        case VolumeSliceViewPlaneEnum::AXIAL:
            break;
        case VolumeSliceViewPlaneEnum::CORONAL:
            rotationMatrix.rotateX(-90.0);
            //rotationMatrix.rotateY(180.0);
            break;
        case VolumeSliceViewPlaneEnum::PARASAGITTAL:
            rotationMatrix.rotateY(90.0);
            rotationMatrix.rotateZ(90.0);
            break;
    }
    
    double rotationMatrixElements[16];
    rotationMatrix.getMatrixForOpenGL(rotationMatrixElements);
    glMultMatrixd(rotationMatrixElements);
    
    const float scale = modelDisplayController->getScaling(tabIndex);
    glScalef(scale, 
             scale, 
             scale);        
}

void 
BrainOpenGLFixedPipeline::initializeMembersBrainOpenGL()
{
    for (int i = 0; i < BrainConstants::MAXIMUM_NUMBER_OF_BROWSER_TABS; i++) {
        orthographicLeft[i] = -1.0f;
        orthographicRight[i] = -1.0f;
        orthographicBottom[i] = -1.0f;
        orthographicTop[i] = -1.0f;
        orthographicFar[i] = -1.0f;
        orthographicNear[i] = -1.0f;
    }
    
    this->initializedOpenGLFlag = false;
}
/**
 * Initialize OpenGL.
 */
void 
BrainOpenGLFixedPipeline::initializeOpenGL()
{
    if (BrainOpenGLFixedPipeline::versionOfOpenGL == 0.0) {
        //
        // Note: The version string might be something like 1.2.4.  std::atof()
        // will get just the 1.2 which is okay.
        //
        const char* versionStr = (char*)(glGetString(GL_VERSION));
        BrainOpenGLFixedPipeline::versionOfOpenGL = std::atof(versionStr);
        const char* vendorStr = (char*)(glGetString(GL_VENDOR));
        const char* renderStr = (char*)(glGetString(GL_RENDERER));
        CaretLogConfig("OpenGL version: " + AString(versionStr)
                       + "\nOpenGL vendor: " + AString(vendorStr)
                       + "\nOpenGL renderer: " + AString(renderStr));
    }
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glFrontFace(GL_CCW);
#ifndef GL_VERSION_1_3 
    glEnable(GL_NORMALIZE);
#else
    if (BrainOpenGLFixedPipeline::versionOfOpenGL >= 1.3) {
        glEnable(GL_RESCALE_NORMAL);
    }
    else {
        glEnable(GL_NORMALIZE);
    }
#endif
    
    //
    // Avoid drawing backfacing polygons
    //
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    
    glShadeModel(GL_SMOOTH);
    
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
    float lightColor[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightColor);
    glEnable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    
    float materialColor[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, materialColor);
    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    
    float ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient); 
    
    this->sphereDisplayList = 0;
    //this->sphereDisplayList = glGenLists(1);
    if (this->sphereDisplayList > 0) {
        glNewList(this->sphereDisplayList, GL_COMPILE);
        GLUquadric* sphereQuadric = gluNewQuadric();
        gluQuadricDrawStyle(sphereQuadric, (GLenum)GLU_FILL);
        gluQuadricOrientation(sphereQuadric, (GLenum)GLU_OUTSIDE);
        gluQuadricNormals(sphereQuadric, (GLenum)GLU_SMOOTH);
        gluSphere(sphereQuadric, 1.0, 10, 10);
        gluDeleteQuadric(sphereQuadric);
        glEndList();
    }
    
    
    if (this->sphereOpenGL == NULL) {
        this->sphereOpenGL = new SphereOpenGL(1.0);
    }
    
    if (this->initializedOpenGLFlag) {
        return;
    }
    this->initializedOpenGLFlag = true;
    
    /*
     * Remaining items need to executed only once.
     */
    float sizes[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, sizes);
    BrainOpenGL::minPointSize = sizes[0];
    BrainOpenGL::maxPointSize = sizes[1];
    glGetFloatv(GL_LINE_WIDTH_RANGE, sizes);
    BrainOpenGL::minLineWidth = sizes[0];
    BrainOpenGL::maxLineWidth = sizes[1];
}

/**
 *
 */
void 
BrainOpenGLFixedPipeline::updateOrthoSize(const int32_t windowIndex, 
                             const int32_t width,
                             const int32_t height)
{
    const double aspectRatio = (static_cast<double>(width)) /
                               (static_cast<double>(height));
    orthographicRight[windowIndex]  =    getModelViewingHalfWindowHeight() * aspectRatio;
    orthographicLeft[windowIndex]   =   -getModelViewingHalfWindowHeight() * aspectRatio;
    orthographicTop[windowIndex]    =    getModelViewingHalfWindowHeight();
    orthographicBottom[windowIndex] =   -getModelViewingHalfWindowHeight();
    orthographicNear[windowIndex]   = -5000.0; //-500.0; //-10000.0;
    orthographicFar[windowIndex]    =  5000.0; //500.0; // 10000.0;
}

/**
 * Enable lighting based upon the current mode.
 */
void 
BrainOpenGLFixedPipeline::enableLighting()
{
    float lightPosition[] = { 0.0f, 0.0f, 1000.0f, 0.0f };
    switch (this->mode) {
        case MODE_DRAWING:
            glPushMatrix();
            glLoadIdentity();
            glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
            glEnable(GL_LIGHT0);
            
            //
            // Light 1 position is opposite of light 0
            //
            lightPosition[0] = -lightPosition[0];
            lightPosition[1] = -lightPosition[1];
            lightPosition[2] = -lightPosition[2];
            glLightfv(GL_LIGHT1, GL_POSITION, lightPosition);      
            glEnable(GL_LIGHT1);
            glPopMatrix();
            
            glEnable(GL_LIGHTING);
            glEnable(GL_COLOR_MATERIAL);
            break;
        case MODE_IDENTIFICATION:
            this->disableLighting();
            break;
    }
}

/**
 * Disable lighting.
 */
void 
BrainOpenGLFixedPipeline::disableLighting()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
}

/**
 * Draw contents of a surface controller.
 * @param surfaceController
 *    Controller that is drawn.
 * @param viewport
 *    Viewport for drawing region.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceController(ModelDisplayControllerSurface* surfaceController,
                                   const int32_t viewport[4])
{
    Surface* surface = surfaceController->getSurface();
    float center[3];
    surface->getBoundingBox()->getCenter(center);

    const bool isRightSurfaceLateralMedialYoked = 
        this->browserTabContent->isDisplayedModelSurfaceRightLateralMedialYoked();
    this->setViewportAndOrthographicProjection(viewport,
                                               isRightSurfaceLateralMedialYoked);
    
    this->applyViewingTransformations(surfaceController, 
                                      this->windowTabIndex,
                                      center,
                                      isRightSurfaceLateralMedialYoked);
    
    this->drawSurface(surface);
}

/**
 * Draw the surface axes.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceAxes()
{
    const float bigNumber = 1000000.0;
    glPushMatrix();
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
    glVertex3f(-bigNumber, 0.0, 0.0);
    glVertex3f( bigNumber, 0.0, 0.0);
    glVertex3f(0.0, -bigNumber, 0.0);
    glVertex3f(0.0,  bigNumber, 0.0);
    glVertex3f(0.0, 0.0, -bigNumber);
    glVertex3f(0.0, 0.0, bigNumber);
    glEnd();
    glPopMatrix();
    
}

/**
 * Draw a surface.
 * @param surface
 *    Surface that is drawn.
 */
void 
BrainOpenGLFixedPipeline::drawSurface(Surface* surface)
{
    glMatrixMode(GL_MODELVIEW);
    
    glEnable(GL_DEPTH_TEST);
    
    this->enableLighting();
    
    switch (this->mode) {
        case MODE_DRAWING:
            this->drawSurfaceTrianglesWithVertexArrays(surface);
            this->drawSurfaceNodeAttributes(surface);
            break;
        case MODE_IDENTIFICATION:
            glShadeModel(GL_FLAT);
            this->drawSurfaceNodes(surface);
            this->drawSurfaceTriangles(surface);
            glShadeModel(GL_SMOOTH);
            break;
    }
    
    this->disableLighting();
}

/**
 * Draw a surface as individual triangles.
 * @param surface
 *    Surface that is drawn.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceTriangles(Surface* surface)
{
    const int numTriangles = surface->getNumberOfTriangles();
    
    const int32_t* triangles = surface->getTriangle(0);
    const float* coordinates = surface->getCoordinate(0);
    const float* normals     = surface->getNormalVector(0);
    const float* rgbaColoring = this->browserTabContent->getSurfaceColoring(surface);

    IdentificationItemSurfaceTriangle* triangleID = 
        this->getIdentificationManager()->getSurfaceTriangleIdentification();
    bool isSelect = false;
    if (this->isIdentifyMode()) {
        if (triangleID->isEnabledForSelection()) {
            isSelect = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        else {
            return;
        }
    }
    
    uint8_t rgb[3];
    
    glBegin(GL_TRIANGLES);
    for (int32_t i = 0; i < numTriangles; i++) {
        const int32_t i3 = i * 3;
        const int32_t n1 = triangles[i3];
        const int32_t n2 = triangles[i3+1];
        const int32_t n3 = triangles[i3+2];
        
        if (isSelect) {
            this->colorIdentification->addItem(rgb, IdentificationItemDataTypeEnum::SURFACE_TRIANGLE, i);
            glColor3ubv(rgb);
            glNormal3fv(&normals[n1*3]);
            glVertex3fv(&coordinates[n1*3]);
            glNormal3fv(&normals[n2*3]);
            glVertex3fv(&coordinates[n2*3]);
            glNormal3fv(&normals[n3*3]);
            glVertex3fv(&coordinates[n3*3]);
        }
        else {
            glColor4fv(&rgbaColoring[n1*4]);
            glNormal3fv(&normals[n1*3]);
            glVertex3fv(&coordinates[n1*3]);
            glColor4fv(&rgbaColoring[n2*4]);
            glNormal3fv(&normals[n2*3]);
            glVertex3fv(&coordinates[n2*3]);
            glColor4fv(&rgbaColoring[n3*4]);
            glNormal3fv(&normals[n3*3]);
            glVertex3fv(&coordinates[n3*3]);
        }
    }
    glEnd();
    
    if (isSelect) {
        int32_t triangleIndex = -1;
        float depth = -1.0;
        this->getIndexFromColorSelection(IdentificationItemDataTypeEnum::SURFACE_TRIANGLE, 
                                         this->mouseX, 
                                         this->mouseY,
                                         triangleIndex,
                                         depth);
        
        
        if (triangleIndex >= 0) {
            triangleID->setSurface(surface);
            triangleID->setTriangleNumber(triangleIndex);
            triangleID->setScreenDepth(depth);
            
            /*
             * Node indices
             */
            const int32_t n1 = triangles[triangleIndex*3];
            const int32_t n2 = triangles[triangleIndex*3 + 1];
            const int32_t n3 = triangles[triangleIndex*3 + 2];
            
            /*
             * Node coordinates
             */
            const float* c1 = &coordinates[n1*3];
            const float* c2 = &coordinates[n2*3];
            const float* c3 = &coordinates[n3*3];
            
            const float average[3] = {
                c1[0] + c2[0] + c3[0],
                c1[1] + c2[1] + c3[1],
                c1[2] + c2[2] + c3[2]
            };
            this->setIdentifiedItemScreenXYZ(triangleID, average);
                   
            GLdouble selectionModelviewMatrix[16];
            glGetDoublev(GL_MODELVIEW_MATRIX, selectionModelviewMatrix);
            
            GLdouble selectionProjectionMatrix[16];
            glGetDoublev(GL_PROJECTION_MATRIX, selectionProjectionMatrix);
            
            GLint selectionViewport[4];
            glGetIntegerv(GL_VIEWPORT, selectionViewport);
            
            /*
             * Window positions of each coordinate
             */
            double dc1[3] = { c1[0], c1[1], c1[2] };
            double dc2[3] = { c2[0], c2[1], c2[2] };
            double dc3[3] = { c3[0], c3[1], c3[2] };
            double wc1[3], wc2[3], wc3[3];
            if (gluProject(dc1[0], 
                           dc1[1], 
                           dc1[2],
                           selectionModelviewMatrix,
                           selectionProjectionMatrix,
                           selectionViewport,
                           &wc1[0],
                           &wc1[1],
                           &wc1[2])
                && gluProject(dc2[0], 
                              dc2[1], 
                              dc2[2],
                              selectionModelviewMatrix,
                              selectionProjectionMatrix,
                              selectionViewport,
                              &wc2[0],
                              &wc2[1],
                              &wc2[2])
                && gluProject(dc3[0], 
                              dc3[1], 
                              dc3[2],
                              selectionModelviewMatrix,
                              selectionProjectionMatrix,
                              selectionViewport,
                              &wc3[0],
                              &wc3[1],
                              &wc3[2])) {
                    const double d1 = MathFunctions::distanceSquared2D(wc1[0], 
                                                                       wc1[1], 
                                                                       this->mouseX, 
                                                                       this->mouseY);
                    const double d2 = MathFunctions::distanceSquared2D(wc2[0], 
                                                                       wc2[1], 
                                                                       this->mouseX, 
                                                                       this->mouseY);
                    const double d3 = MathFunctions::distanceSquared2D(wc3[0], 
                                                                       wc3[1], 
                                                                       this->mouseX, 
                                                                       this->mouseY);
                    triangleID->setNearestNode(n3);
                    triangleID->setNearestNodeScreenXYZ(wc3);
                    triangleID->setNearestNodeModelXYZ(dc3);
                    if ((d1 < d2) && (d1 < d3)) {
                        triangleID->setNearestNode(n1);
                        triangleID->setNearestNodeScreenXYZ(wc1);
                        triangleID->setNearestNodeModelXYZ(dc1);
                    }
                    else if ((d2 < d1) && (d2 < d3)) {
                        triangleID->setNearestNode(n2);
                        triangleID->setNearestNodeScreenXYZ(wc2);
                        triangleID->setNearestNodeModelXYZ(dc2);
                    }
            }
            CaretLogFine("Selected Triangle: " + QString::number(triangleIndex));
        }
        
    }
}

/**
 * Draw a surface as individual nodes.
 * @param surface
 *    Surface that is drawn.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceNodes(Surface* surface)
{
    const int numNodes = surface->getNumberOfNodes();
    
    const float* coordinates = surface->getCoordinate(0);
    const float* normals     = surface->getNormalVector(0);
    const float* rgbaColoring = this->browserTabContent->getSurfaceColoring(surface);
    
    IdentificationItemSurfaceNode* nodeID = 
    this->getIdentificationManager()->getSurfaceNodeIdentification();
    bool isSelect = false;
    if (this->isIdentifyMode()) {
        if (nodeID->isEnabledForSelection()) {
            isSelect = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);            
        }
        else {
            return;
        }
    }
    
    uint8_t rgb[3];
    
    glPointSize(2.0);
    glBegin(GL_POINTS);
    for (int32_t i = 0; i < numNodes; i++) {
        const int32_t i3 = i * 3;
        
        if (isSelect) {
            this->colorIdentification->addItem(rgb, IdentificationItemDataTypeEnum::SURFACE_NODE, i);
            glColor3ubv(rgb);
            glNormal3fv(&normals[i3]);
            glVertex3fv(&coordinates[i3]);
        }
        else {
            glColor4fv(&rgbaColoring[i*4]);
            glNormal3fv(&normals[i3]);
            glVertex3fv(&coordinates[i3]);
        }
    }
    glEnd();
    
    if (isSelect) {
        int nodeIndex = -1;
        float depth = -1.0;
        this->getIndexFromColorSelection(IdentificationItemDataTypeEnum::SURFACE_NODE, 
                                         this->mouseX, 
                                         this->mouseY,
                                         nodeIndex,
                                         depth);
        if (nodeIndex >= 0) {
            nodeID->setSurface(surface);
            nodeID->setNodeNumber(nodeIndex);
            nodeID->setScreenDepth(depth);
            this->setIdentifiedItemScreenXYZ(nodeID, &coordinates[nodeIndex * 3]);
            CaretLogFine("Selected Node: " + QString::number(nodeIndex));   
        }
    }
}


/**
 * Draw a surface triangles with vertex arrays.
 * @param surface
 *    Surface that is drawn.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceTrianglesWithVertexArrays(const Surface* surface)
{
    const float* rgbaColoring = this->browserTabContent->getSurfaceColoring(surface);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, 
                    GL_FLOAT, 
                    0, 
                    reinterpret_cast<const GLvoid*>(surface->getCoordinate(0)));
    glColorPointer(4, 
                   GL_FLOAT, 
                   0, 
                   reinterpret_cast<const GLvoid*>(rgbaColoring));
    glNormalPointer(GL_FLOAT, 
                    0, 
                    reinterpret_cast<const GLvoid*>(surface->getNormalVector(0)));
    
    const int numTriangles = surface->getNumberOfTriangles();
    glDrawElements(GL_TRIANGLES, 
                   (3 * numTriangles), 
                   GL_UNSIGNED_INT,
                   reinterpret_cast<const GLvoid*>(surface->getTriangle(0)));
    
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
}

/**
 * Draw attributes for the given surface.
 * @param surface
 *    Surface for which attributes are drawn.
 */
void 
BrainOpenGLFixedPipeline::drawSurfaceNodeAttributes(Surface* surface)
{
    BrainStructure* brainStructure = surface->getBrainStructure();
    CaretAssert(brainStructure);
    
    const int numNodes = surface->getNumberOfNodes();
    
    const float* coordinates = surface->getCoordinate(0);

    //glDisable(GL_COLOR_MATERIAL);
    
    glColor3f(0.0, 1.0, 0.0);
    //glPointSize(5.0);
    //glBegin(GL_POINTS);
    for (int32_t i = 0; i < numNodes; i++) {
        if (brainStructure->getNodeAttributes(i)->isIdentified()) {
            const int32_t i3 = i * 3;
            glPushMatrix();
            glTranslatef(coordinates[i3], coordinates[i3+1], coordinates[i3+2]);
            this->drawSphere(3.5);
            glPopMatrix();
        }
    }
    //glEnd();
}

/**
 * Setup volume drawing information for an overlay.
 *
 * @param browserTabContent
 *    Content in the browser tab.
 * @param paletteFile
 *    File from which palette is obtained.
 * @param volumeDrawInfoOut
 *    Output containing information for volume drawing.
 */
void 
BrainOpenGLFixedPipeline::setupVolumeDrawInfo(BrowserTabContent* browserTabContent,
                                 PaletteFile* paletteFile,
                                 std::vector<VolumeDrawInfo>& volumeDrawInfoOut)
{
    volumeDrawInfoOut.clear();
    
    OverlaySet* overlaySet = browserTabContent->getOverlaySet();
    const int32_t numberOfOverlays = overlaySet->getNumberOfDisplayedOverlays();
    for (int32_t iOver = (numberOfOverlays - 1); iOver >= 0; iOver--) {
        Overlay* overlay = overlaySet->getOverlay(iOver);
        if (overlay->isEnabled()) {
            CaretMappableDataFile* mapFile;
            int32_t mapIndex;
            overlay->getSelectionData(browserTabContent,
                                      mapFile,
                                      mapIndex);
            if (mapFile != NULL) {
                if (mapFile->isVolumeMappable()) {
                    VolumeFile* vf = NULL;
                    ConnectivityLoaderFile* connLoadFile = dynamic_cast<ConnectivityLoaderFile*>(mapFile);
                    if (connLoadFile != NULL) {
                        vf = connLoadFile->getConnectivityVolumeFile();  
                    }
                    else {
                        vf = dynamic_cast<VolumeFile*>(mapFile);
                    }
                    if (vf != NULL) {
                        if (vf->isMappedWithPalette()) {
                            PaletteColorMapping* paletteColorMapping = vf->getMapPaletteColorMapping(mapIndex);
                            if (connLoadFile != NULL) {
                                paletteColorMapping = connLoadFile->getPaletteColorMapping(mapIndex);
                            }
                            Palette* palette = paletteFile->getPaletteByName(paletteColorMapping->getSelectedPaletteName());
                            if (palette != NULL) {
                                bool useIt = true;
                                
                                const float opacity = overlay->getOpacity();
                                
                                if (volumeDrawInfoOut.empty() == false) {
                                    /*
                                     * If previous volume is the same as this
                                     * volume, there is no need to draw it twice.
                                     */
                                    const VolumeDrawInfo& vdi = volumeDrawInfoOut[volumeDrawInfoOut.size() - 1];
                                    if ((vdi.volumeFile == vf) 
                                        && (opacity >= 1.0)
                                        && (mapIndex == vdi.brickIndex)
                                        && (palette == vdi.palette)) {
                                        useIt = false;
                                    }
                                }
                                if (useIt) {
                                    const DescriptiveStatistics* statistics = 
                                        (connLoadFile != NULL) 
                                        ? connLoadFile->getMapStatistics(mapIndex)
                                        : vf->getMapStatistics(mapIndex);
                                    
                                        VolumeDrawInfo vdi(vf,
                                                           palette,
                                                           paletteColorMapping,
                                                           statistics,
                                                           mapIndex,
                                                           opacity);
                                        volumeDrawInfoOut.push_back(vdi);
                                }
                            }
                            else {
                                CaretLogWarning("No valid palette for drawing volume file: "
                                                + vf->getFileNameNoPath());
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Draw the volume slices.
 * @param browserTabContent
 *    Content of the window.
 * @param volumeController
 *    Controller for slices.
 * @param viewport
 *    Region of drawing.
 */
void 
BrainOpenGLFixedPipeline::drawVolumeController(BrowserTabContent* browserTabContent,
                                  ModelDisplayControllerVolume* volumeController,
                                  const int32_t viewport[4])
{
    glDisable(GL_DEPTH_TEST);
    
    const int32_t tabNumber = browserTabContent->getTabNumber();
    volumeController->updateController(tabNumber);
    Brain* brain = volumeController->getBrain();
    
    /*
     * Determine volumes that are to be drawn
     */
    std::vector<VolumeDrawInfo> volumeDrawInfo;
    this->setupVolumeDrawInfo(browserTabContent,
                              volumeController->getBrain()->getPaletteFile(),
                              volumeDrawInfo);
    
    if (volumeDrawInfo.empty() == false) {
        const VolumeSliceIndicesSelection* selectedSlices = volumeController->getSelectedVolumeSlices(tabNumber);
        
        VolumeFile* underlayVolumeFile = volumeDrawInfo[0].volumeFile;
        float selectedVoxelXYZ[3];
        underlayVolumeFile->indexToSpace(selectedSlices->getSliceIndexParasagittal(),
                                         selectedSlices->getSliceIndexCoronal(), 
                                         selectedSlices->getSliceIndexAxial(),
                                         selectedVoxelXYZ);

        switch (volumeController->getSliceViewMode(tabNumber)) {
            case VolumeSliceViewModeEnum::MONTAGE:
            {
                const int numRows = volumeController->getMontageNumberOfRows(tabNumber);
                CaretAssert(numRows > 0);
                const int numCols = volumeController->getMontageNumberOfColumns(tabNumber);
                CaretAssert(numCols > 0);
                const int vpSizeX = viewport[2] / numCols;
                const int vpSizeY = viewport[3] / numRows;
                
                int sliceIndex = -1;
                int maximumSliceIndex = -1;
                std::vector<int64_t> dimensions;
                volumeDrawInfo[0].volumeFile->getDimensions(dimensions);
                const int sliceStep = volumeController->getMontageSliceSpacing(tabNumber);
                const VolumeSliceViewPlaneEnum::Enum slicePlane = volumeController->getSliceViewPlane(this->windowTabIndex);
                switch (slicePlane) {
                    case VolumeSliceViewPlaneEnum::ALL:
                        sliceIndex = -1;
                        break;
                    case VolumeSliceViewPlaneEnum::AXIAL:
                        sliceIndex = selectedSlices->getSliceIndexAxial();
                        maximumSliceIndex = dimensions[2];
                        break;
                    case VolumeSliceViewPlaneEnum::CORONAL:
                        sliceIndex = selectedSlices->getSliceIndexCoronal();
                        maximumSliceIndex = dimensions[1];
                        break;
                    case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                        sliceIndex = selectedSlices->getSliceIndexParasagittal();
                        maximumSliceIndex = dimensions[0];
                        break;
                }

                if (sliceIndex >= 0) {
                    for (int i = 0; i < numRows; i++) {
                        for (int j = 0; j < numCols; j++) {
                            if (sliceIndex < maximumSliceIndex) {
                                const int vpX = j * vpSizeX;
                                const int vpY = i * vpSizeY;
                                const int vp[4] = { 
                                    viewport[0] + vpX, 
                                    viewport[1] + vpY, 
                                    vpSizeX, 
                                    vpSizeY };
                                
                                this->setViewportAndOrthographicProjection(vp);
                                this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                             this->windowTabIndex, 
                                                                             slicePlane);
                                this->drawVolumeOrthogonalSlice(slicePlane, 
                                                                sliceIndex, 
                                                                volumeDrawInfo);
                                this->drawVolumeSurfaceOutlines(brain, 
                                                                slicePlane, 
                                                                sliceIndex, 
                                                                underlayVolumeFile);
                                this->drawVolumeAxesCrosshairs(slicePlane, 
                                                               selectedVoxelXYZ);
                            }
                            sliceIndex += sliceStep;
                        }
                    }
                }
                
                glViewport(viewport[0],
                           viewport[1],
                           viewport[2],
                           viewport[3]);
                this->drawVolumeAxesLabels(slicePlane, 
                                           viewport);
            }
                break;
            case VolumeSliceViewModeEnum::OBLIQUE:
                break;
            case VolumeSliceViewModeEnum::ORTHOGONAL:
            {
                const VolumeSliceViewPlaneEnum::Enum slicePlane = volumeController->getSliceViewPlane(this->windowTabIndex);
                switch (slicePlane) {
                    case VolumeSliceViewPlaneEnum::ALL:
                    {
                        const int halfX = viewport[2] / 2;
                        const int halfY = viewport[3] / 2;
                        
                        const int axialVP[4] = { viewport[0], viewport[1] + halfY, halfX, halfY };
                        this->setViewportAndOrthographicProjection(axialVP);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::AXIAL);
                        this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::AXIAL, 
                                                        selectedSlices->getSliceIndexAxial(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        VolumeSliceViewPlaneEnum::AXIAL, 
                                                        selectedSlices->getSliceIndexAxial(), 
                                                        underlayVolumeFile);
                        this->drawVolumeAxesCrosshairs(VolumeSliceViewPlaneEnum::AXIAL, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(VolumeSliceViewPlaneEnum::AXIAL, 
                                                   axialVP);
                        
                        const int coronalVP[4] = { viewport[0] + halfX, viewport[1] + halfY, halfX, halfY };
                        this->setViewportAndOrthographicProjection(coronalVP);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::CORONAL);
                        this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::CORONAL, 
                                                        selectedSlices->getSliceIndexCoronal(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        VolumeSliceViewPlaneEnum::CORONAL, 
                                                        selectedSlices->getSliceIndexCoronal(), 
                                                        underlayVolumeFile);
                       this->drawVolumeAxesCrosshairs(VolumeSliceViewPlaneEnum::CORONAL, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(VolumeSliceViewPlaneEnum::CORONAL, 
                                                   coronalVP);
                        
                        const int parasagittalVP[4] = { viewport[0] + halfX, viewport[1], halfX, halfY };
                        this->setViewportAndOrthographicProjection(parasagittalVP);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::PARASAGITTAL);
                        this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                        selectedSlices->getSliceIndexParasagittal(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                        selectedSlices->getSliceIndexParasagittal(), 
                                                        underlayVolumeFile);
                        this->drawVolumeAxesCrosshairs(VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                   parasagittalVP);
                        
                    }
                        break;
                    case VolumeSliceViewPlaneEnum::AXIAL:
                        this->setViewportAndOrthographicProjection(viewport);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::AXIAL);
                        this->drawVolumeOrthogonalSlice(slicePlane, 
                                                        selectedSlices->getSliceIndexAxial(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        slicePlane, 
                                                        selectedSlices->getSliceIndexAxial(), 
                                                        underlayVolumeFile);
                        this->drawVolumeAxesCrosshairs(slicePlane, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(slicePlane, 
                                                   viewport);
                        break;
                    case VolumeSliceViewPlaneEnum::CORONAL:
                        this->setViewportAndOrthographicProjection(viewport);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::CORONAL);
                        this->drawVolumeOrthogonalSlice(slicePlane, 
                                                        selectedSlices->getSliceIndexCoronal(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        slicePlane, 
                                                        selectedSlices->getSliceIndexCoronal(), 
                                                        underlayVolumeFile);
                        this->drawVolumeAxesCrosshairs(slicePlane, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(slicePlane, 
                                                   viewport);
                        break;
                    case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                        this->setViewportAndOrthographicProjection(viewport);
                        this->applyViewingTransformationsVolumeSlice(volumeController, 
                                                                     this->windowTabIndex, 
                                                                     VolumeSliceViewPlaneEnum::PARASAGITTAL);
                        this->drawVolumeOrthogonalSlice(slicePlane, 
                                                        selectedSlices->getSliceIndexParasagittal(),
                                                        volumeDrawInfo);
                        this->drawVolumeSurfaceOutlines(brain, 
                                                        slicePlane, 
                                                        selectedSlices->getSliceIndexParasagittal(), 
                                                        underlayVolumeFile);
                        this->drawVolumeAxesCrosshairs(slicePlane, 
                                                       selectedVoxelXYZ);
                        this->drawVolumeAxesLabels(slicePlane, 
                                                   viewport);
                        break;
                }
            }
            break;
        }
        
    }
}

/**
 * Draw the volume axes crosshairs.
 * @param brain
 *   Brain that owns the volumes.
 * @param slicePlane
 *   Plane being viewed.
 * @param voxelXYZ
 *   Stereotaxic coordinate of current slice indices.
 */
void 
BrainOpenGLFixedPipeline::drawVolumeAxesCrosshairs(
                                               const VolumeSliceViewPlaneEnum::Enum slicePlane,
                                               const float voxelXYZ[3])
{
    if (SessionManager::get()->getCaretPreferences()->isVolumeAxesCrosshairsDisplayed()) {
        unsigned char red[3]   = { 255, 0, 0 };
        unsigned char green[3] = { 0, 255, 0 };
        unsigned char blue[3]  = { 0, 0, 255 };
        
        float crosshairX = 0;
        float crosshairY = 0;
        
        unsigned char* xColor = red;
        unsigned char* yColor = red;
        
        bool drawIt = true;
        switch(slicePlane) {
            case VolumeSliceViewPlaneEnum::AXIAL:
                crosshairX = voxelXYZ[0]; //slices[0] * spacing[0] + origin[0];
                xColor = red;
                crosshairY = voxelXYZ[1]; //slices[1] * spacing[1] + origin[1];
                yColor = green;
                break;
            case VolumeSliceViewPlaneEnum::CORONAL:
                crosshairX = voxelXYZ[0]; //slices[0] * spacing[0] + origin[0];
                xColor = red;
                crosshairY = voxelXYZ[2]; //slices[2] * spacing[2] + origin[2];
                yColor = blue;
                break;
            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                crosshairX = voxelXYZ[1]; //slices[1] * spacing[1] + origin[1];
                xColor = green;
                crosshairY = voxelXYZ[2]; //slices[2] * spacing[2] + origin[2];
                yColor = blue;
                break;
            default:
                drawIt = false;
                break;
        }
        
        if (drawIt) {
            const float bigNumber = 10000;
            glLineWidth(1.0);
            glColor3ubv(green);
            glBegin(GL_LINES);
            glVertex3f(voxelXYZ[0], -bigNumber, voxelXYZ[2]);
            glVertex3f(voxelXYZ[0],  bigNumber, voxelXYZ[2]);
            glColor3ubv(red);
            glVertex3f(-bigNumber, voxelXYZ[1], voxelXYZ[2]);
            glVertex3f( bigNumber, voxelXYZ[1], voxelXYZ[2]);
            glColor3ubv(blue);
            glVertex3f(voxelXYZ[0], voxelXYZ[1], -bigNumber);
            glVertex3f(voxelXYZ[0], voxelXYZ[1],  bigNumber);
            glEnd();
        }
    }
    
}

/**
 * Draw the volume axes labels.
 * @param brain
 *   Brain that owns the volumes.
 * @param slicePlane
 *   Plane being viewed.
 * @param viewport
 *   Viewport of drawing region.
 */
void 
BrainOpenGLFixedPipeline::drawVolumeAxesLabels(
                          const VolumeSliceViewPlaneEnum::Enum slicePlane,
                          const int32_t viewport[4])
{
    if (SessionManager::get()->getCaretPreferences()->isVolumeAxesLabelsDisplayed()) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glOrtho(0, viewport[2], 0, viewport[3], -1.0, 1.0);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        QString orientLeftSideLabel;
        QString orientRightSideLabel;
        QString orientBottomSideLabel;
        QString orientTopSideLabel;
        
        switch(slicePlane) {
            case VolumeSliceViewPlaneEnum::AXIAL:
                orientLeftSideLabel   = "L";
                orientRightSideLabel  = "R";
                orientBottomSideLabel = "P";
                orientTopSideLabel    = "A";
                break;
            case VolumeSliceViewPlaneEnum::CORONAL:
                orientLeftSideLabel   = "L";
                orientRightSideLabel  = "R";
                orientBottomSideLabel = "V";
                orientTopSideLabel    = "D";
                break;
            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                orientLeftSideLabel   = "A";
                orientRightSideLabel  = "P";
                orientBottomSideLabel = "V";
                orientTopSideLabel    = "D";
                break;
            default:
                break;
        }
        
        /*
         * Switch to the foreground color.
         */
        uint8_t foregroundRGB[3];
        SessionManager::get()->getCaretPreferences()->getColorForeground(foregroundRGB);
        glColor3ubv(foregroundRGB);
        
        this->drawTextWindowCoords(5, 
                                   (viewport[3] / 2), 
                                   orientLeftSideLabel, 
                                   BrainOpenGLTextRenderInterface::X_LEFT,
                                   BrainOpenGLTextRenderInterface::Y_CENTER);

        this->drawTextWindowCoords((viewport[2] - 5), 
                                   (viewport[3] / 2), 
                                   orientRightSideLabel, 
                                   BrainOpenGLTextRenderInterface::X_RIGHT,
                                   BrainOpenGLTextRenderInterface::Y_CENTER);
        
        this->drawTextWindowCoords((viewport[2] / 2), 
                                   5, 
                                   orientBottomSideLabel, 
                                   BrainOpenGLTextRenderInterface::X_CENTER,
                                   BrainOpenGLTextRenderInterface::Y_BOTTOM);
        
        this->drawTextWindowCoords((viewport[2] / 2), 
                                   (viewport[3] - 5), 
                                   orientTopSideLabel, 
                                   BrainOpenGLTextRenderInterface::X_CENTER,
                                   BrainOpenGLTextRenderInterface::Y_TOP);
        
        glPopMatrix();
        
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        
        glMatrixMode(GL_MODELVIEW);
    }
}

/**
 * Draw a single volume orthogonal slice.
 * @param slicePlane
 *    Plane that is drawn
 * @param sliceIndex
 *    Index of slice in plane.
 * @param volumeDrawInfo
 *    Describes volumes that are drawn.
 */
void 
BrainOpenGLFixedPipeline::drawVolumeOrthogonalSlice(const VolumeSliceViewPlaneEnum::Enum slicePlane,
                                       const int64_t sliceIndex,
                                       std::vector<VolumeDrawInfo>& volumeDrawInfo)
{
    const int32_t numberOfVolumesToDraw = static_cast<int32_t>(volumeDrawInfo.size());

    IdentificationItemVoxel* voxelID = 
        this->getIdentificationManager()->getVoxelIdentification();
    bool isSelect = false;
    if (this->isIdentifyMode()) {
        if (voxelID->isEnabledForSelection()) {
            isSelect = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);            
        }
        else {
            return;
        }
    }
    
    /*
     * For slices disable culling since want to see both side
     * and set shading to flat so there is no interpolation of
     * colors within a voxel drawn as a quad.  This allows
     * drawing of voxels using quad strips.
     */
    this->disableLighting();
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);
    
    int64_t dimI, dimJ, dimK, numMaps, numComponents;
    VolumeFile* underlayVolumeFile = volumeDrawInfo[0].volumeFile;
    underlayVolumeFile->getDimensions(dimI, dimJ, dimK, numMaps, numComponents);
    
    const int64_t lastDimI = dimI - 1;
    const int64_t lastDimJ = dimJ - 1;
    const int64_t lastDimK = dimK - 1;
    
    float originX, originY, originZ;
    float xv, yv, zv;
    underlayVolumeFile->indexToSpace((int64_t)0, (int64_t)0, (int64_t)0, originX, originY, originZ);
    underlayVolumeFile->indexToSpace((int64_t)1, (int64_t)1, (int64_t)1, xv, yv, zv);
    const float voxelSizeX = xv - originX;
    const float voxelSizeY = yv - originY;
    const float voxelSizeZ = zv - originZ;
    
    /*
     * Determine range of voxels for drawing
     */
    int64_t iStart = 0;
    int64_t iEnd   = dimI;
    int64_t jStart = 0;
    int64_t jEnd   = dimJ;
    int64_t kStart = 0;
    int64_t kEnd   = dimK;
    switch (slicePlane) {
        case VolumeSliceViewPlaneEnum::ALL:
            CaretAssert(0);
            break;
        case VolumeSliceViewPlaneEnum::AXIAL:
            kStart = sliceIndex;
            kEnd = sliceIndex + 1;
            break;
        case VolumeSliceViewPlaneEnum::CORONAL:
            jStart = sliceIndex;
            jEnd   = sliceIndex + 1;
            break;
        case VolumeSliceViewPlaneEnum::PARASAGITTAL:
            iStart = sliceIndex;
            iEnd   = sliceIndex + 1;
            break;
    }
    
    /*
     * Set colors for each drawn voxel
     * Use a vector so no worries about memory being freed
     */
    const int64_t numVoxels = (iEnd - iStart) * (jEnd - jStart) * (kEnd - kStart);
    std::vector<float> sliceRgbaVector(numVoxels * 4);
    float* sliceRGBA = &sliceRgbaVector[0];
    
    float rgba[4];
    for (int64_t i = iStart; i < iEnd; i++) {
        for (int64_t j = jStart; j < jEnd; j++) {
            for (int64_t k = kStart; k < kEnd; k++) {
                const float x = originX + i * voxelSizeX;
                const float y = originY + j * voxelSizeY;
                const float z = originZ + k * voxelSizeZ;
                for (int32_t iVol = 0; iVol < numberOfVolumesToDraw; iVol++) {
                    VolumeDrawInfo& volInfo = volumeDrawInfo[iVol];
                    VolumeFile* vf = volInfo.volumeFile;
                    const int64_t brickIndex = volInfo.brickIndex;
                    bool valid = false;
                    float voxel = 0;
                    if (iVol == 0) {
                        voxel = vf->getValue(i, j, k, brickIndex);
                        valid = true;
                    }
                    else {
                        int64_t iVoxel, jVoxel, kVoxel;
                        vf->closestVoxel(x, y, z, iVoxel, jVoxel, kVoxel);
                        if (vf->indexValid(iVoxel, jVoxel, kVoxel, brickIndex)) {
                            voxel = vf->getValue(iVoxel, jVoxel, kVoxel, brickIndex);
                            valid = true;
                        }
                    }
                    
                    if (valid) {
                        int64_t sliceRgbaOffset = -1;
                        switch (slicePlane) {
                            case VolumeSliceViewPlaneEnum::ALL:
                                CaretAssert(0);
                                break;
                            case VolumeSliceViewPlaneEnum::AXIAL:
                                sliceRgbaOffset = (i + (j * dimI)) * 4;
                                break;
                            case VolumeSliceViewPlaneEnum::CORONAL:
                                sliceRgbaOffset = (i + (k * dimI)) * 4;
                                break;
                            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                                sliceRgbaOffset = (j + (k * dimJ)) * 4;
                                break;
                        }
                        
                        NodeAndVoxelColoring::colorScalarsWithPalette(volInfo.statistics,
                                                                      volInfo.paletteColorMapping,
                                                                      volInfo.palette,
                                                                      &voxel,
                                                                      &voxel,
                                                                      1,
                                                                      rgba);
                        if(iVol == 0) {
                            sliceRGBA[sliceRgbaOffset] = rgba[0];
                            sliceRGBA[sliceRgbaOffset+1] = rgba[1];
                            sliceRGBA[sliceRgbaOffset+2] = rgba[2];
                            sliceRGBA[sliceRgbaOffset+3] = 1.0;
                        }
                        else if (rgba[3] > 0.0) {
                            sliceRGBA[sliceRgbaOffset] = rgba[0];
                            sliceRGBA[sliceRgbaOffset+1] = rgba[1];
                            sliceRGBA[sliceRgbaOffset+2] = rgba[2];
                            sliceRGBA[sliceRgbaOffset+3] = rgba[3];
                        }
                    }
                }
            } 
        }
    }
    
    bool useQuadStrips = true;
    if (isSelect) {
        useQuadStrips = false;
    }
    if (useQuadStrips) {
        /*
         * Note on quad strips:
         *
         * Each quad receives the color specified at the vertex
         * 2i +2 (for i = 1..N).
         *
         * So, the color used to draw a quad is the color that
         * is specified at vertex 3, 5, 7,.. with the first
         * vertex being 1.
         */
        
        switch (slicePlane) {
            case VolumeSliceViewPlaneEnum::ALL:
                CaretAssert(0);
                break;
            case VolumeSliceViewPlaneEnum::AXIAL:
            {
                if (sliceIndex < dimK) {
                    const int64_t k = sliceIndex;
                    const float z = originZ + (k * voxelSizeZ);
                    float x = originX;
                    for (int64_t i = 0; i < dimI; i++) {
                        glBegin(GL_QUAD_STRIP);
                        {
                            const float x2 = x + voxelSizeX;
                            float y = originY;
                            
                            glVertex3f(x, y, z);
                            glVertex3f(x2, y, z);
                            
                            for (int64_t j = 0; j < dimJ; j++) {
                                const int32_t sliceRgbaOffset = (i + (j * dimI)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                                
                                y += voxelSizeY;
                                glVertex3f(x, y, z);
                                glVertex3f(x2, y, z);
                            }
                            
                            x += voxelSizeX;
                        }
                        glEnd();
                    }
                }
            }
                break;
            case VolumeSliceViewPlaneEnum::CORONAL:
            {
                if (sliceIndex < dimJ) {
                    const int64_t j = sliceIndex;
                    const float y = originY + (j * voxelSizeY);
                    float x = originX;
                    for (int64_t i = 0; i < dimI; i++) {
                        glBegin(GL_QUAD_STRIP);
                        {
                            const float x2 = x + voxelSizeX;
                            float z = originZ;
                            
                            glVertex3f(x, y, z);
                            glVertex3f(x2, y, z);
                            
                            for (int64_t k = 0; k < dimK; k++) {
                                const int32_t sliceRgbaOffset = (i + (k * dimI)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                                
                                z += voxelSizeZ;
                                glVertex3f(x, y, z);
                                glVertex3f(x2, y, z);
                            }
                            
                            x += voxelSizeX;
                        }
                        glEnd();
                    }
                }
            }
                break;
            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
            {
                if (sliceIndex < dimI) {
                    const int64_t i = sliceIndex;
                    const float x = originX + (i * voxelSizeX);
                    float y = originY;
                    for (int64_t j = 0; j < dimJ; j++) {
                        glBegin(GL_QUAD_STRIP);
                        {
                            const float y2 = y + voxelSizeY;
                            float z = originZ;
                            
                            glVertex3f(x, y, z);
                            glVertex3f(x, y2, z);
                            
                            for (int64_t k = 0; k < dimK; k++) {
                                const int32_t sliceRgbaOffset = (j + (k * dimJ)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                                
                                z += voxelSizeZ;
                                glVertex3f(x, y, z);
                                glVertex3f(x, y2, z);
                            }
                            
                            y += voxelSizeY;
                        }
                        glEnd();
                    }
                }
            }
                break;
        }
    }
    else {
        uint8_t rgb[3];
        std::vector<int64_t> idVoxelIndices;
        int64_t idVoxelCounter = 0;
        if (isSelect) {
            const int64_t bigDim = std::max(dimI, std::max(dimJ, dimK));
            idVoxelIndices.reserve(bigDim * 3);
        }
        
        float x1, y1, z1;
        float x2, y2, z2;
        glBegin(GL_QUADS);
        switch (slicePlane) {
            case VolumeSliceViewPlaneEnum::ALL:
                CaretAssert(0);
                break;
            case VolumeSliceViewPlaneEnum::AXIAL:
            {
                if (sliceIndex < dimK) {
                    const int64_t k = sliceIndex;
                    for (int64_t i = 0; i < lastDimI; i++) {
                        for (int64_t j = 0; j < lastDimJ; j++) {
                            if (isSelect) {
                                this->colorIdentification->addItem(rgb, 
                                                                   IdentificationItemDataTypeEnum::VOXEL, 
                                                                   idVoxelCounter);
                                glColor3ubv(rgb);
                                
                                idVoxelIndices.push_back(i);
                                idVoxelIndices.push_back(j);
                                idVoxelIndices.push_back(k);
                                idVoxelCounter++;
                            }
                            else {
                                const int32_t sliceRgbaOffset = (i + (j * dimI)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                            }
                            underlayVolumeFile->indexToSpace(i, j, k, x1, y1, z1);
                            x2 = x1 + voxelSizeX;
                            y2 = y1 + voxelSizeY;
                            glVertex3f(x1, y1, z1);
                            glVertex3f(x2, y1, z1);
                            glVertex3f(x2, y2, z1);
                            glVertex3f(x1, y2, z1);
                        }
                    }
                }
            }
                break;
            case VolumeSliceViewPlaneEnum::CORONAL:
            {
                if (sliceIndex < dimJ) {
                    const int64_t j = sliceIndex;
                    for (int64_t i = 0; i < lastDimI; i++) {
                        for (int64_t k = 0; k < lastDimK; k++) {
                            if (isSelect) {
                                this->colorIdentification->addItem(rgb, 
                                                                   IdentificationItemDataTypeEnum::VOXEL, 
                                                                   idVoxelCounter);
                                glColor3ubv(rgb);
                                
                                idVoxelIndices.push_back(i);
                                idVoxelIndices.push_back(j);
                                idVoxelIndices.push_back(k);
                                idVoxelCounter++;
                            }
                            else {
                                const int32_t sliceRgbaOffset = (i + (k * dimI)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                            }
                            underlayVolumeFile->indexToSpace(i, j, k, x1, y1, z1);
                            x2 = x1 + voxelSizeX;
                            z2 = z1 + voxelSizeZ;
                            glVertex3f(x1, y1, z1);
                            glVertex3f(x2, y1, z1);
                            glVertex3f(x2, y1, z2);
                            glVertex3f(x1, y1, z2);
                        }
                    }
                }
            }
                break;
            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
            {
                if (sliceIndex < dimI) {
                    const int64_t i = sliceIndex;
                    for (int64_t j = 0; j < lastDimJ; j++) {
                        for (int64_t k = 0; k < lastDimK; k++) {
                            if (isSelect) {
                                this->colorIdentification->addItem(rgb, 
                                                                   IdentificationItemDataTypeEnum::VOXEL, 
                                                                   idVoxelCounter);
                                glColor3ubv(rgb);
                                
                                idVoxelIndices.push_back(i);
                                idVoxelIndices.push_back(j);
                                idVoxelIndices.push_back(k);
                                idVoxelCounter++;
                            }
                            else {
                                const int32_t sliceRgbaOffset = (j + (k * dimJ)) * 4;
                                glColor4fv(&sliceRGBA[sliceRgbaOffset]);
                            }
                            underlayVolumeFile->indexToSpace(i, j, k, x1, y1, z1);
                            y2 = y1 + voxelSizeY;
                            z2 = z1 + voxelSizeZ;
                            glVertex3f(x1, y1, z1);
                            glVertex3f(x1, y2, z1);
                            glVertex3f(x1, y2, z2);
                            glVertex3f(x1, y1, z2);
                        }
                    }
                }
            }
                break;
        }
        glEnd();
        
        if (isSelect) {
            int32_t idIndex;
            float depth = -1.0;
            this->getIndexFromColorSelection(IdentificationItemDataTypeEnum::VOXEL, 
                                             this->mouseX, 
                                             this->mouseY,
                                             idIndex,
                                             depth);
            if (idIndex >= 0) {
                int64_t voxelIndices[3] = {
                    idVoxelIndices[idIndex*3],
                    idVoxelIndices[idIndex*3+1],
                    idVoxelIndices[idIndex*3+2]
                };
                voxelID->setVolumeFile(underlayVolumeFile);
                voxelID->setVoxelIJK(voxelIndices);
                voxelID->setScreenDepth(depth);
                
                float voxelXYZ[3];
                underlayVolumeFile->indexToSpace(voxelIndices, voxelXYZ);
                
                this->setIdentifiedItemScreenXYZ(voxelID, voxelXYZ);
                CaretLogFine("Selected Voxel: " + AString::fromNumbers(voxelIndices, 3, ","));  
            }
        }
        
    }
    
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
}

/**
 * Draw surface outlines on volume slices.
 */
void 
BrainOpenGLFixedPipeline::drawVolumeSurfaceOutlines(Brain* brain,
                                                    const VolumeSliceViewPlaneEnum::Enum slicePlane,
                                                    const int64_t sliceIndex,
                                                    VolumeFile* underlayVolume)
{
    CaretAssert(brain);
    CaretAssert(underlayVolume);
    
    const DisplayPropertiesVolume* dpv = brain->getDisplayPropertiesVolume();
    
    std::vector<int64_t> dim;
    underlayVolume->getDimensions(dim);
    
    /*
     * Find three points on the slice so that the equation for a Plane
     * can be formed.
     */
    float p1[3];
    float p2[3];
    float p3[3];
    switch(slicePlane) {
        case VolumeSliceViewPlaneEnum::ALL:
            return;
            break;
        case VolumeSliceViewPlaneEnum::PARASAGITTAL:
        {
            underlayVolume->indexToSpace(sliceIndex, 0, 0, p1);
            underlayVolume->indexToSpace(sliceIndex, dim[1] - 1, 0, p2);
            underlayVolume->indexToSpace(sliceIndex, dim[1] - 1, dim[2] - 1, p3);
        }
            break;
        case VolumeSliceViewPlaneEnum::CORONAL:
        {
            underlayVolume->indexToSpace(0, sliceIndex, 0, p1);
            underlayVolume->indexToSpace(dim[0] - 1, sliceIndex, 0, p2);
            underlayVolume->indexToSpace(dim[0] - 1, sliceIndex, dim[2] - 1, p3);
        }
            break;
        case VolumeSliceViewPlaneEnum::AXIAL:
        {
            underlayVolume->indexToSpace(0, 0, sliceIndex, p1);
            underlayVolume->indexToSpace(dim[0] - 1, 0, sliceIndex, p2);
            underlayVolume->indexToSpace(dim[0] - 1, dim[1] - 1, sliceIndex, p3);
        }
            break;
    }
    
    Plane plane(p1, p2, p3);
    if (plane.isValidPlane() == false) {
        return;
    }
    
    /*
     * Extends the length the lines that are drawn
     * and help prevents small gaps in the surface
     * outline.
     */
    bool stretchLinesFlag = true;
    float stretchDiff = 0.4f;
    float stretchFactor = 1.0f + stretchDiff;
    float stretchFactor2 = 1.0f + stretchDiff * 2.0f;
    
    float intersectionPoint1[3];
    float intersectionPoint2[3];
    
    /*
     * Process each surface outline
     */
    for (int io = 0; 
         io < DisplayPropertiesVolume::MAXIMUM_NUMBER_OF_SURFACE_OUTLINES; 
         io++) {
        const VolumeSurfaceOutlineSelection* outline = dpv->getSurfaceOutlineSelection(io);
        if (outline->isDisplayed()) {
            const Surface* surface = outline->getSurface();
            if (surface != NULL) {
                const float thickness = outline->getThickness();
                
                int numTriangles = surface->getNumberOfTriangles();
                
                const CaretColorEnum::Enum outlineColor = outline->getColor();
                const bool surfaceColorFlag = (outlineColor == CaretColorEnum::SURFACE);
                const float* rgbaColoring = this->browserTabContent->getSurfaceColoring(surface);
                
                glColor3fv(CaretColorEnum::toRGB(outlineColor));
                glLineWidth(thickness);
                
                /*
                 * Examine each triangle to see if it intersects the Plane
                 * in which the slice exists.
                 */
                glBegin(GL_LINES);
                for (int it = 0; it < numTriangles; it++) {
                    const int32_t* triangleNodes = surface->getTriangle(it);
                    const float* c1 = surface->getCoordinate(triangleNodes[0]);
                    const float* c2 = surface->getCoordinate(triangleNodes[1]);
                    const float* c3 = surface->getCoordinate(triangleNodes[2]);
                    
                    if (plane.triangleIntersectPlane(c1, c2, c3,
                                                     intersectionPoint1,
                                                     intersectionPoint2)) {
                        if (surfaceColorFlag) {
                            /*
                             * Use coloring assigned to the first node in the triangle.
                             */
                            glColor3fv(&rgbaColoring[triangleNodes[0] * 3]);
                        }
                        if (stretchLinesFlag) {
                            float dx = intersectionPoint2[0] - intersectionPoint1[0];
                            float dy = intersectionPoint2[1] - intersectionPoint1[1];
                            float dz = intersectionPoint2[2] - intersectionPoint1[2];
                            float d = (float)std::sqrt(dx*dx + dy*dy + dz*dz);
                            if (d > 0.0f) {
                                intersectionPoint2[0] = intersectionPoint1[0] + dx * stretchFactor;
                                intersectionPoint2[1] = intersectionPoint1[1] + dy * stretchFactor;
                                intersectionPoint2[2] = intersectionPoint1[2] + dz * stretchFactor;
                                intersectionPoint1[0] = intersectionPoint2[0] - dx * stretchFactor2;
                                intersectionPoint1[1] = intersectionPoint2[1] - dy * stretchFactor2;
                                intersectionPoint1[2] = intersectionPoint2[2] - dz * stretchFactor2;
                            }
                        }
                        
                        /*
                         * Draw the line where the triangle intersections the slice
                         */
                        switch(slicePlane) {
                            case VolumeSliceViewPlaneEnum::ALL:
                                return;
                                break;
                            case VolumeSliceViewPlaneEnum::PARASAGITTAL:
                                glVertex3f(p1[0], intersectionPoint1[1], intersectionPoint1[2]);
                                glVertex3f(p1[0], intersectionPoint2[1], intersectionPoint2[2]);
                                break;
                            case VolumeSliceViewPlaneEnum::CORONAL:
                                glVertex3f(intersectionPoint1[0], p1[1], intersectionPoint1[2]);
                                glVertex3f(intersectionPoint2[0], p1[1], intersectionPoint2[2]);
                                break;
                            case VolumeSliceViewPlaneEnum::AXIAL:
                                glVertex3f(intersectionPoint1[0], intersectionPoint1[1], p1[2]);
                                glVertex3f(intersectionPoint2[0], intersectionPoint2[1], p1[2]);
                                break;
                        }
                    }
                }
                glEnd();
            }
        }
    }
}

/**
 * Draw the whole brain.
 * @param browserTabContent
 *    Content of the window.
 * @param wholeBrainController
 *    Controller for whole brain.
 * @param viewport
 *    Region for drawing.
 */
void 
BrainOpenGLFixedPipeline::drawWholeBrainController(BrowserTabContent* browserTabContent,
                                      ModelDisplayControllerWholeBrain* wholeBrainController,
                                      const int32_t viewport[4])
{
    this->setViewportAndOrthographicProjection(viewport);
    this->applyViewingTransformations(wholeBrainController, 
                                      this->windowTabIndex,
                                      NULL,
                                      false);
    
    const int32_t tabNumberIndex = browserTabContent->getTabNumber();
    const SurfaceTypeEnum::Enum surfaceType = wholeBrainController->getSelectedSurfaceType(tabNumberIndex);
    
    /*
     * Draw the surfaces. 
     */
    Brain* brain = wholeBrainController->getBrain();
    const int32_t numberOfBrainStructures = brain->getNumberOfBrainStructures();
    for (int32_t i = 0; i < numberOfBrainStructures; i++) {
        BrainStructure* brainStructure = brain->getBrainStructure(i);
        const StructureEnum::Enum structure = brainStructure->getStructure();
        Surface* surface = wholeBrainController->getSelectedSurface(structure, 
                                                                    tabNumberIndex);
        if (surface != NULL) {
            float dx = 0.0;
            float dy = 0.0;
            float dz = 0.0;
            
            bool drawIt = false;
            switch (structure) {
                case StructureEnum::CORTEX_LEFT:
                    drawIt = wholeBrainController->isLeftEnabled(tabNumberIndex);
                    dx = -wholeBrainController->getLeftRightSeparation(tabNumberIndex);
                    if ((surfaceType != SurfaceTypeEnum::SURFACE_TYPE_ANATOMICAL)
                        && (surfaceType != SurfaceTypeEnum::SURFACE_TYPE_RECONSTRUCTION)) {
                        dx -= surface->getBoundingBox()->getMaxX();
                    }
                    break;
                case StructureEnum::CORTEX_RIGHT:
                    drawIt = wholeBrainController->isRightEnabled(tabNumberIndex);
                    dx = wholeBrainController->getLeftRightSeparation(tabNumberIndex);
                    if ((surfaceType != SurfaceTypeEnum::SURFACE_TYPE_ANATOMICAL)
                        && (surfaceType != SurfaceTypeEnum::SURFACE_TYPE_RECONSTRUCTION)) {
                        dx -= surface->getBoundingBox()->getMinX();
                    }
                    break;
                case StructureEnum::CEREBELLUM:
                    drawIt = wholeBrainController->isCerebellumEnabled(tabNumberIndex);
                    dz = wholeBrainController->getCerebellumSeparation(tabNumberIndex);
                    break;
                default:
                    CaretLogWarning("programmer-issure: Surface type not left/right/cerebellum");
                    break;
            }
            
            if (drawIt) {
                glPushMatrix();
                glTranslatef(dx, dy, dz);
                this->drawSurface(surface);
                glPopMatrix();
            }
        }
    }
        
    /*
     * Need depth testing for drawing slices
     */
    glEnable(GL_DEPTH_TEST);

    /*
     * Determine volumes that are to be drawn  
     */
    if (wholeBrainController->getUnderlayVolumeFile(tabNumberIndex) != NULL) {
        std::vector<VolumeDrawInfo> volumeDrawInfo;
        this->setupVolumeDrawInfo(browserTabContent,
                                  wholeBrainController->getBrain()->getPaletteFile(),
                                  volumeDrawInfo);
        if (volumeDrawInfo.empty() == false) {
            const VolumeSliceIndicesSelection* slices = 
            wholeBrainController->getSelectedVolumeSlices(tabNumberIndex);
            if (slices->isSliceAxialEnabled()) {
                this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::AXIAL, 
                                                slices->getSliceIndexAxial(), 
                                                volumeDrawInfo);
                this->drawVolumeSurfaceOutlines(brain, 
                                                VolumeSliceViewPlaneEnum::AXIAL, 
                                                slices->getSliceIndexAxial(), 
                                                volumeDrawInfo[0].volumeFile);
            }
            if (slices->isSliceCoronalEnabled()) {
                this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::CORONAL, 
                                                slices->getSliceIndexCoronal(), 
                                                volumeDrawInfo);
                this->drawVolumeSurfaceOutlines(brain, 
                                                VolumeSliceViewPlaneEnum::CORONAL, 
                                                slices->getSliceIndexCoronal(), 
                                                volumeDrawInfo[0].volumeFile);
            }
            if (slices->isSliceParasagittalEnabled()) {
                this->drawVolumeOrthogonalSlice(VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                slices->getSliceIndexParasagittal(), 
                                                volumeDrawInfo);
                this->drawVolumeSurfaceOutlines(brain, 
                                                VolumeSliceViewPlaneEnum::PARASAGITTAL, 
                                                slices->getSliceIndexParasagittal(), 
                                                volumeDrawInfo[0].volumeFile);
            }
        }
    }
}

/**
 * Setup the orthographic projection.
 * @param viewport
 *    The viewport (x, y, width, height)
 * @param isRightSurfaceLateralMedialYoked
 *    True if the displayed model is a right surface that is 
 *    lateral/medial yoked.
 */
void 
BrainOpenGLFixedPipeline::setOrthographicProjection(const int32_t viewport[4],
                                       const bool isRightSurfaceLateralMedialYoked)
{
    double defaultOrthoWindowSize = BrainOpenGLFixedPipeline::getModelViewingHalfWindowHeight();
    double width = viewport[2];
    double height = viewport[3];
    double aspectRatio = (width / height);
    double orthographicRight  =    defaultOrthoWindowSize * aspectRatio;
    double orthographicLeft   =   -defaultOrthoWindowSize * aspectRatio;
    double orthographicTop    =    defaultOrthoWindowSize;
    double orthographicBottom =   -defaultOrthoWindowSize;
    double orthographicNear   = -1000.0; //-500.0; //-10000.0;
    double orthographicFar    =  1000.0; //500.0; // 10000.0;
    
    if (isRightSurfaceLateralMedialYoked) {
        glOrtho(orthographicRight, orthographicLeft, 
                orthographicBottom, orthographicTop, 
                orthographicFar, orthographicNear);    
    }
    else {
        glOrtho(orthographicLeft, orthographicRight, 
                orthographicBottom, orthographicTop, 
                orthographicNear, orthographicFar);            
    }
}

/**
 * check for an OpenGL Error.
 */
void 
BrainOpenGLFixedPipeline::checkForOpenGLError(const ModelDisplayController* modelController,
                                      const AString& msgIn)
{
    GLenum errorCode = glGetError();
    if (errorCode != GL_NO_ERROR) {
        AString msg;
        if (msgIn.isEmpty() == false) {
            msg += (msgIn + "\n");
        }
        msg += ("OpenGL Error: " + AString((char*)gluErrorString(errorCode)) + "\n");
        msg += ("OpenGL Version: " + AString((char*)glGetString(GL_VERSION)) + "\n");
        msg += ("OpenGL Vendor:  " + AString((char*)glGetString(GL_VENDOR)) + "\n");
        if (modelController != NULL) {
            msg += ("While drawing brain model " + modelController->getNameForGUI(true) + "\n");
        }
        msg += ("In window number " + AString::number(this->windowTabIndex) + "\n");
        GLint nameStackDepth, modelStackDepth, projStackDepth;
        glGetIntegerv(GL_PROJECTION_STACK_DEPTH,
                      &projStackDepth);
        glGetIntegerv(GL_MODELVIEW_STACK_DEPTH,
                      &modelStackDepth);
        glGetIntegerv(GL_NAME_STACK_DEPTH,
                      &nameStackDepth);
        msg += ("Projection Matrix Stack Depth " + AString::number(projStackDepth) + "\n");
        msg += ("Model Matrix Stack Depth " + AString::number(modelStackDepth) + "\n");
        msg += ("Name Matrix Stack Depth " + AString::number(nameStackDepth) + "\n");
        CaretLogSevere(msg);
    }
}

/**
 * Analyze color information to extract identification data.
 * @param dataType
 *    Type of data.
 * @param x
 *    X-coordinate of identification.
 * @param y
 *    X-coordinate of identification.
 * @param indexOut
 *    Index of identified item.
 * @param depthOut
 *    Depth of identified item.
 */
void
BrainOpenGLFixedPipeline::getIndexFromColorSelection(IdentificationItemDataTypeEnum::Enum dataType,
                                        const int32_t x,
                                        const int32_t y,
                                        int32_t& indexOut,
                                        float& depthOut)
{
    // Figure out item was picked using color in color buffer
    //
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uint8_t pixels[3];
    glReadPixels((int)x,
                    (int)y,
                    1,
                    1,
                    GL_RGB,
                    GL_UNSIGNED_BYTE,
                    pixels);
    
    indexOut = -1;
    depthOut = -1.0;
    
    CaretLogFine("ID color is "
                 + QString::number(pixels[0]) + ", "
                 + QString::number(pixels[1]) + ", "
                 + QString::number(pixels[2]));
    
    this->colorIdentification->getItem(pixels, dataType, &indexOut);
    
    if (indexOut >= 0) {
        /*
         * Get depth from depth buffer
         */
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(x,
                     y,
                        1,
                        1,
                        GL_DEPTH_COMPONENT,
                        GL_FLOAT,
                        &depthOut);
    }
    this->colorIdentification->reset();
}

/**
 * Set the identified item's screen coordinates.
 * @param item
 *    Item that has screen coordinates set.
 * @param itemXYZ
 *    Model's coordinate.
 */
void 
BrainOpenGLFixedPipeline::setIdentifiedItemScreenXYZ(IdentificationItem* item,
                                        const float itemXYZ[3])
{
    GLdouble selectionModelviewMatrix[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, selectionModelviewMatrix);
    
    GLdouble selectionProjectionMatrix[16];
    glGetDoublev(GL_PROJECTION_MATRIX, selectionProjectionMatrix);
    
    GLint selectionViewport[4];
    glGetIntegerv(GL_VIEWPORT, selectionViewport);
    
    const double modelXYZ[3] = {
        itemXYZ[0],
        itemXYZ[1],
        itemXYZ[2]
    };
    
    double windowPos[3];
    if (gluProject(modelXYZ[0], 
                   modelXYZ[1], 
                   modelXYZ[2],
                   selectionModelviewMatrix,
                   selectionProjectionMatrix,
                   selectionViewport,
                   &windowPos[0],
                   &windowPos[1],
                   &windowPos[2])) {
        item->setScreenXYZ(windowPos);
        item->setModelXYZ(modelXYZ);
    }
}

/**
 * Draw sphere.
 */
void 
BrainOpenGLFixedPipeline::drawSphere(const double radius)
{
    glPushMatrix();
    glScaled(radius, radius, radius);
    if (this->sphereDisplayList > 0) {
        glCallList(this->sphereDisplayList);
    }
    else {
        this->sphereOpenGL->drawWithQuadStrips();
    }
    glPopMatrix();
}

/**
 * Draw text at the given window coordinates.
 * @param windowX
 *    Window X-coordinate.
 * @param windowY
 *    Window Y-coordinate.
 * @param text
 *    Text that is to be drawn.
 * @param alignment
 *    Alignment of text.
 */
void 
BrainOpenGLFixedPipeline::drawTextWindowCoords(const int windowX,
                                               const int windowY,
                                               const QString& text,
                                               const BrainOpenGLTextRenderInterface::TextAlignmentX alignmentX,
                                               const BrainOpenGLTextRenderInterface::TextAlignmentY alignmentY)
{
    if (this->textRenderer != NULL) {
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        int viewport[4] = {
            vp[0],
            vp[1],
            vp[2],
            vp[3]
        };
        this->textRenderer->drawTextAtWindowCoords(viewport,
                                                   windowX,
                                                   windowY,
                                                   text,
                                                   alignmentX,
                                                   alignmentY);
    }
}

/**
 * Draw text at the given window coordinates.
 * @param modelX
 *    Model X-coordinate.
 * @param modelY
 *    Model Y-coordinate.
 * @param modelZ
 *    Model Z-coordinate.
 * @param text
 *    Text that is to be drawn.
 * @param alignment
 *    Alignment of text.
 */
void 
BrainOpenGLFixedPipeline::drawTextModelCoords(const double modelX,
                                              const double modelY,
                                              const double modelZ,
                                              const QString& text)
{
    if (this->textRenderer != NULL) {
        this->textRenderer->drawTextAtModelCoords(modelX,
                                                  modelY,
                                                  modelZ,
                                                  text);
    }
}

/**
 * Draw the palettes showing how scalars are mapped
 * to colors.
 * @param brain
 *    Brain containing model being drawn.
 * @param viewport
 *    Viewport for the model.
 */
void 
BrainOpenGLFixedPipeline::drawAllPalettes(Brain* brain)
{
    /*
     * Turn off depth testing
     */
    glDisable(GL_DEPTH_TEST);
    

    /*
     * Save the projection matrix, model matrix, and viewport.
     */
    glMatrixMode(GL_PROJECTION);
    GLfloat savedProjectionMatrix[16];
    glGetFloatv(GL_PROJECTION_MATRIX, 
                savedProjectionMatrix);
    glMatrixMode(GL_MODELVIEW);
    GLfloat savedModelviewMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, 
                savedModelviewMatrix);
    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, 
                  savedViewport);
    
    CaretAssert(brain);
    const bool selectFlag = (this->mode == MODE_IDENTIFICATION);
    if (selectFlag) {
        return;
    }
    this->disableLighting();
    
    PaletteFile* paletteFile = brain->getPaletteFile();
    CaretAssert(paletteFile);
    
    std::vector<CaretMappableDataFile*> mapFiles;
    std::vector<int32_t> mapIndices;
    this->browserTabContent->getDisplayedPaletteMapFiles(mapFiles, 
                                                         mapIndices);
    
    /*
     * Each map file has a palette drawn to represent the
     * datas mapping to colors.
     */
    const int32_t numMapFiles = static_cast<int32_t>(mapFiles.size());
    for (int32_t i = 0; i < numMapFiles; i++) {
        const int mapIndex = mapIndices[i];
        const PaletteColorMapping* pcm = mapFiles[i]->getMapPaletteColorMapping(mapIndex);
        const AString paletteName = pcm->getSelectedPaletteName();
        const Palette* palette = paletteFile->getPaletteByName(paletteName);
        if (palette != NULL) {
            const DescriptiveStatistics* statistics = mapFiles[i]->getMapStatistics(mapIndex);
            this->drawPalette(palette, 
                              pcm, 
                              statistics, 
                              i);
        }
        else {
            CaretLogWarning("Palette named "
                            + paletteName
                            + " not found in palette file.");
        }
    }
    
    /*
     * Restore the projection matrix, model matrix, and viewport.
     */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(savedProjectionMatrix);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(savedModelviewMatrix);
    glViewport(savedViewport[0],
               savedViewport[1],
               savedViewport[2],
               savedViewport[3]);
}

/**
 * Draw a palette.
 * @param palette
 *    Palette that is drawn.
 * @param paletteColorMapping
 *    Controls mapping of data to colors.
 * @param statistics
 *    Statistics describing the data that is mapped to the palette.
 * @param paletteDrawingIndex
 *    Counts number of palettes being drawn for the Y-position
 */
void 
BrainOpenGLFixedPipeline::drawPalette(const Palette* palette,
                                      const PaletteColorMapping* paletteColorMapping,
                                      const DescriptiveStatistics* statistics,
                                      const int paletteDrawingIndex)
{
    /*
     * Save viewport.
     */
    GLint modelViewport[4];
    glGetIntegerv(GL_VIEWPORT, modelViewport);
    
    /*
     * Create a viewport for drawing the palettes in the 
     * lower left corner of the window.
     */
    const GLint colorbarViewportWidth = 120;
    const GLint colorbarViewportHeight = 35;    
    const GLint colorbarViewportX = 10;
    
    GLint colorbarViewportY = (10 + (paletteDrawingIndex * colorbarViewportHeight));
    if (paletteDrawingIndex > 0) {
//        colorbarViewportY += 5;
    }
    
    glViewport(colorbarViewportX, 
               colorbarViewportY, 
               colorbarViewportWidth, 
               colorbarViewportHeight);
    
    /*
     * Create an orthographic projection
     */
    //const GLdouble halfWidth = static_cast<GLdouble>(colorbarViewportWidth / 2);
    const GLdouble halfHeight = static_cast<GLdouble>(colorbarViewportHeight / 2);
    const GLdouble margin = 1.1;
    const GLdouble orthoWidth = margin;
    const GLdouble orthoHeight = halfHeight * margin;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-orthoWidth,  orthoWidth, 
            -orthoHeight, orthoHeight, 
            -1.0, 1.0);
    
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();

    /*
     * Use the background color to fill in a rectangle
     * for display of palette, hiding anything currently drawn.
     */
    uint8_t backgroundRGB[3];
    CaretPreferences* prefs = SessionManager::get()->getCaretPreferences();
    prefs->getColorBackground(backgroundRGB);
    glColor3ubv(backgroundRGB);
    glRectf(-orthoWidth, -orthoHeight, orthoWidth, orthoHeight);
    
    /*
     * Always interpolate if the palette has only two colors
     */
    bool interpolateColor = paletteColorMapping->isInterpolatePaletteFlag();
    if (palette->getNumberOfScalarsAndColors() <= 2) {
        interpolateColor = true;
    }
    
    /*
     * Types of values for display
     */
    const bool isPositiveDisplayed = paletteColorMapping->isDisplayPositiveDataFlag();
    const bool isNegativeDisplayed = paletteColorMapping->isDisplayNegativeDataFlag();
    const bool isZeroDisplayed     = paletteColorMapping->isDisplayZeroDataFlag();
    
    /*
     * Draw the colorbar starting with the color assigned
     * to the negative end of the palette.
     * Colorbar scalars range from -1 to 1.
     */
    const int iStart = palette->getNumberOfScalarsAndColors() - 1;
    const int iEnd = 1;
    const int iStep = -1;
    for (int i = iStart; i >= iEnd; i += iStep) {
        /*
         * palette data for 'left' side of a color in the palette.
         */
        const PaletteScalarAndColor* sc = palette->getScalarAndColor(i);
        float scalar = sc->getScalar();
        float rgba[4];
        sc->getColor(rgba);
        
        /*
         * palette data for 'right' side of a color in the palette.
         */
        const PaletteScalarAndColor* nextSC = palette->getScalarAndColor(i - 1);
        float nextScalar = nextSC->getScalar();
        float nextRGBA[4];
        nextSC->getColor(nextRGBA);
        const bool isNoneColorFlag = nextSC->isNoneColor();
        
        /*
         * Exclude negative regions if not displayed.
         *
        if (isNegativeDisplayed == false) {
            if (nextScalar < 0.0) {
                continue;
            }
            else if (scalar < 0.0) {
                scalar = 0.0;
            }
        }
        */
        
        /*
         * Exclude positive regions if not displayed.
         *
        if (isPositiveDisplayed == false) {
            if (scalar > 0.0) {
                continue;
            }
            else if (nextScalar > 0.0) {
                nextScalar = 0.0;
            }
        }
        */
        
        /*
         * Normally, the first entry has a scalar value of -1.
         * If it does not, use the first color draw from 
         * -1 to the first scalar value.
         */
        if (i == iStart) {
            if (sc->isNoneColor() == false) {
                if (scalar > -1.0) {
                    const float xStart = -1.0;
                    const float xEnd   = scalar;
                    glColor3fv(rgba);
                    glBegin(GL_POLYGON);
                    glVertex3f(xStart, 0.0, 0.0);
                    glVertex3f(xStart, -halfHeight, 0.0);
                    glVertex3f(xEnd, -halfHeight, 0.0);
                    glVertex3f(xEnd, 0.0, 0.0);
                    glEnd();
                }
            }
        }
        
        /*
         * If the 'next' color is none, drawing
         * is skipped to let the background show
         * throw the 'none' region of the palette.
         */ 
        if (isNoneColorFlag == false) {
            /*
             * left and right region of an entry in the palette
             */
            const float xStart = scalar;
            const float xEnd   = nextScalar;
            
            /*
             * Unless interpolating, use the 'next' color.
             */
            float* startRGBA = nextRGBA;
            float* endRGBA   = nextRGBA;
            if (interpolateColor) {
                startRGBA = rgba;
            }
            
            /*
             * Draw the region in the palette.
             */
            glBegin(GL_POLYGON);
            glColor3fv(startRGBA);
            glVertex3f(xStart, 0.0, 0.0);
            glVertex3f(xStart, -halfHeight, 0.0);
            glColor3fv(endRGBA);
            glVertex3f(xEnd, -halfHeight, 0.0);
            glVertex3f(xEnd, 0.0, 0.0);
            glEnd();
            
            /*
             * The last scalar value is normally 1.0.  If the last
             * scalar is less than 1.0, then fill in the rest of 
             * the palette from the last scalar to 1.0.
             */
            if (i == iEnd) {
                if (nextScalar < 1.0) {
                    const float xStart = nextScalar;
                    const float xEnd   = 1.0;
                    glColor3fv(nextRGBA);
                    glBegin(GL_POLYGON);
                    glVertex3f(xStart, 0.0, 0.0);
                    glVertex3f(xStart, -halfHeight, 0.0);
                    glVertex3f(xEnd, -halfHeight, 0.0);
                    glVertex3f(xEnd, 0.0, 0.0);
                    glEnd();
                }
            }
        }
    }
    
    /*
     * If positive not displayed, draw over it with background color
     */
    if (isPositiveDisplayed == false) {
        glColor3ubv(backgroundRGB);
        glRectf(0.0, -orthoHeight, orthoWidth, orthoHeight);
    }
    
    /*
     * If negative not displayed, draw over it with background color
     */
    if (isNegativeDisplayed == false) {
        glColor3ubv(backgroundRGB);
        glRectf(-orthoWidth, -orthoHeight, 0.0, orthoHeight);
    }
    
    /*
     * If zeros are not displayed, draw a line in the 
     * background color at zero in the palette.
     */
    if (isZeroDisplayed == false) {
        glLineWidth(1.0);
        glColor3ubv(backgroundRGB);
        glBegin(GL_LINES);
        glVertex2f(0.0, -halfHeight);
        glVertex2f(0.0, 0.0);
        glEnd();
    }
    
    float minMax[4] = { -1.0, 0.0, 0.0, 1.0 };
    switch (paletteColorMapping->getScaleMode()) {
        case PaletteScaleModeEnum::MODE_AUTO_SCALE:
            minMax[0] = statistics->getMostNegativeValue();
            minMax[3] = statistics->getMostPositiveValue();
            break;
        case PaletteScaleModeEnum::MODE_AUTO_SCALE_PERCENTAGE:
        {
            const float negMaxPct = paletteColorMapping->getAutoScalePercentageNegativeMaximum();
            const float negMinPct = paletteColorMapping->getAutoScalePercentageNegativeMinimum();
            const float posMinPct = paletteColorMapping->getAutoScalePercentagePositiveMinimum();
            const float posMaxPct = paletteColorMapping->getAutoScalePercentagePositiveMaximum();
            
            minMax[0] = statistics->getNegativePercentile(negMaxPct);
            minMax[1] = statistics->getNegativePercentile(negMinPct);
            minMax[2] = statistics->getPositivePercentile(posMinPct);
            minMax[3] = statistics->getPositivePercentile(posMaxPct);
        }
            break;
        case PaletteScaleModeEnum::MODE_USER_SCALE:
            minMax[0] = paletteColorMapping->getUserScaleNegativeMaximum();
            minMax[1] = paletteColorMapping->getUserScaleNegativeMinimum();
            minMax[2] = paletteColorMapping->getUserScalePositiveMinimum();
            minMax[3] = paletteColorMapping->getUserScalePositiveMaximum();
            break;
    }
    
    AString textLeft = AString::number(minMax[0], 'f', 1);
    AString textCenterNeg = AString::number(minMax[1], 'f', 1);
    AString textCenterPos = AString::number(minMax[2], 'f', 1);
    AString textCenter = textCenterPos;
    if (textCenterNeg != textCenterPos) {
        if (textCenterNeg != AString("-" + textCenterPos)) {
            textCenter = textCenterNeg + "/" + textCenterPos;
        }
    }
    AString textRight = AString::number(minMax[3], 'f', 1);
    
    /*
     * Reset to the models viewport for drawing text.
     */
    glViewport(modelViewport[0], 
               modelViewport[1], 
               modelViewport[2], 
               modelViewport[3]);
    
    /*
     * Switch to the foreground color.
     */
    uint8_t foregroundRGB[3];
    prefs->getColorForeground(foregroundRGB);
    glColor3ubv(foregroundRGB);
    
    /*
     * Account for margin around colorbar when calculating text locations
     */
    const int textCenterX = colorbarViewportX + (colorbarViewportWidth / 2);
    const int textHalfX   = colorbarViewportWidth / (margin * 2);
    const int textLeftX   = textCenterX - textHalfX;
    const int textRightX  = textCenterX + textHalfX;
    
    const int textY = 2 + colorbarViewportY + (colorbarViewportHeight / 2);
    if (isNegativeDisplayed) {
        this->drawTextWindowCoords(textLeftX, 
                                   textY, 
                                   textLeft,
                                   BrainOpenGLTextRenderInterface::X_LEFT,
                                   BrainOpenGLTextRenderInterface::Y_BOTTOM);
    }
    if (isNegativeDisplayed
        || isZeroDisplayed
        || isPositiveDisplayed) {
        this->drawTextWindowCoords(textCenterX, 
                                   textY, 
                                   textCenter,
                                   BrainOpenGLTextRenderInterface::X_CENTER,
                                   BrainOpenGLTextRenderInterface::Y_BOTTOM);
    }
    if (isPositiveDisplayed) {
        this->drawTextWindowCoords(textRightX, 
                                   textY, 
                                   textRight,
                                   BrainOpenGLTextRenderInterface::X_RIGHT,
                                   BrainOpenGLTextRenderInterface::Y_BOTTOM);
    }
    
    return;
}

//============================================================================
/**
 * Constructor.
 */
BrainOpenGLFixedPipeline::VolumeDrawInfo::VolumeDrawInfo(VolumeFile* volumeFile,
                                                   Palette* palette,
                                                   PaletteColorMapping* paletteColorMapping,
                                                   const DescriptiveStatistics* statistics,
                                                   const int32_t brickIndex,
                                                   const float opacity) 
: statistics(statistics) {
    this->volumeFile = volumeFile;
    this->palette = palette;
    this->paletteColorMapping = paletteColorMapping;
    this->brickIndex = brickIndex;
    this->opacity    = opacity;
}
