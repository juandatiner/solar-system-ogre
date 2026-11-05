// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================
#ifndef __pujOgre__vtkPolyData2OgreMesh__h__
#define __pujOgre__vtkPolyData2OgreMesh__h__

#include <pujOgre/Export.h>
#include <OgreMesh.h>
#include <OgreResourceGroupManager.h>

// Forward declarations
class vtkPolyData;

namespace pujOgre
{
  /**
   * Fills an existing (empty) Ogre::Mesh from any vtkPolyData: adds one
   * SubMesh with hardware buffers and sets the bounds. Does NOT call
   * mesh->load(), so it can be used both on manual meshes and from inside
   * a Codec while Ogre is loading the mesh.
   *
   * - Polygons and triangle strips are triangulated; vertices and lines
   *   are ignored.
   * - Point normals are used if present (3 components); otherwise they are
   *   generated
   *   only if generateNormalsIfMissing is true.
   * - Point texture coordinates are used if present (1, 2 or 3 components).
   *   flipV converts VTK's bottom-left texture origin to Ogre's top-left one.
   * - 16-bit indices are used when possible, otherwise 32-bit.
   */
  void pujOgre_EXPORT FillOgreMeshFromvtkPolyData(
    vtkPolyData* polyData,
    Ogre::Mesh* mesh,
    const Ogre::String& materialName = "BaseWhite",
    bool generateNormalsIfMissing = false,
    bool flipV = true
    );

  /**
   * Creates a new manual mesh named meshName, fills it from polyData and
   * loads it.
   * Throws Ogre::Exception if a mesh with that name already exists.
   */
  Ogre::MeshPtr pujOgre_EXPORT vtkPolyData2OgreMesh(
    vtkPolyData* polyData,
    const Ogre::String& meshName,
    const Ogre::String& materialName = "BaseWhite",
    bool generateNormalsIfMissing = false,
    bool flipV = true,
    const Ogre::String& group
    =
    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
    );

} // end namespace

#endif // __pujOgre__vtkPolyData2OgreMesh__h__

// eof - vtkPolyData2OgreMesh.h
