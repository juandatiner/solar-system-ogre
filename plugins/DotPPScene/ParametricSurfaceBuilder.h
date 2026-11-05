// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================
#ifndef __pujOgre__Plugin__ParametricSurfaceBuilder__h__
#define __pujOgre__Plugin__ParametricSurfaceBuilder__h__

#include <vtkSmartPointer.h>

// Forward declarations
class vtkPolyData;
namespace pugi { class xml_node; }

/**
 * Builds a vtkPolyData from a <parametricSurface> element of a .scenepp file,
 * using vtkParametricFunctionSource. Throws std::runtime_error on invalid
 * input.
 *
 *   <parametricSurface name="..." type="expression|torus|ellipsoid|superEllipsoid"
 *                      material="..." uResolution="50" vResolution="50"
 *                      textureCoordinates="true" flipWinding="false"
 *                      castShadows="true" visible="true">
 *       <uRange min="0" max="6.283185" join="true"/>     optional
 *       <vRange min="0" max="6.283185" join="true"/>     optional
 *       <param name="R" value="2"/>                      zero or more
 *       <point  x="..." y="..." z="..."/>                type="expression" only
 *       <normal x="..." y="..." z="..."/>                optional, type="expression" only
 *       <userData> ... </userData>                       optional
 *   </parametricSurface>
 */
vtkSmartPointer< vtkPolyData > buildParametricPolyData(
  const pugi::xml_node& xml
  );

#endif // __pujOgre__Plugin__ParametricSurfaceBuilder__h__

// eof - ParametricSurfaceBuilder.h
