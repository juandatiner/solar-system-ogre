// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

#include <pujOgre/vtkPolyData2OgreMesh.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <OgreMeshManager.h>
#include <OgreSubMesh.h>
#include <OgreHardwareBufferManager.h>
#include <OgreVertexIndexData.h>
#include <OgreAxisAlignedBox.h>

#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkCellArray.h>
#include <vtkIdList.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>

// -------------------------------------------------------------------------
void pujOgre::
FillOgreMeshFromvtkPolyData(
  vtkPolyData* polyData,
  Ogre::Mesh* mesh,
  const Ogre::String& materialName,
  bool generateNormalsIfMissing,
  bool flipV
  )
{
  if( mesh == nullptr )
    throw std::invalid_argument(
      "[pujOgre::FillOgreMeshFromvtkPolyData]: null Ogre::Mesh"
      );
  if( polyData == nullptr )
    throw std::invalid_argument(
      "[pujOgre::FillOgreMeshFromvtkPolyData]: null vtkPolyData"
      );
  if( polyData->GetNumberOfPoints( ) == 0 )
    throw std::invalid_argument(
      "[pujOgre::FillOgreMeshFromvtkPolyData]: empty vtkPolyData"
      );

  // 1. Triangulate polygons + strips (point data is passed through).
  auto triangulator = vtkSmartPointer< vtkTriangleFilter >::New( );
  triangulator->SetInputData( polyData );
  triangulator->PassVertsOff( );
  triangulator->PassLinesOff( );
  triangulator->Update( );
  vtkSmartPointer< vtkPolyData > surface = triangulator->GetOutput( );

  // 1.1. Optionally generate smooth point normals.
  if( generateNormalsIfMissing && !surface->GetPointData( )->GetNormals( ) )
  {
    auto normalsFilter = vtkSmartPointer< vtkPolyDataNormals >::New( );
    normalsFilter->SetInputData( surface );
    normalsFilter->ComputePointNormalsOn( );
    normalsFilter->ComputeCellNormalsOff( );
    normalsFilter->SplittingOff( );
    normalsFilter->Update( );
    surface = normalsFilter->GetOutput( );
  } // end if
  vtkPoints* points = surface->GetPoints( );
  if( !points || points->GetNumberOfPoints( ) == 0 )
    throw std::runtime_error(
      "[pujOgre::FillOgreMeshFromvtkPolyData]: no points after triangulation"
      );
  vtkDataArray* normals = surface->GetPointData( )->GetNormals( );
  if( normals != nullptr && normals->GetNumberOfComponents( ) != 3 )
    normals = nullptr;
  vtkDataArray* tcoords = surface->GetPointData( )->GetTCoords( );
  int tcComps = ( tcoords != nullptr )? tcoords->GetNumberOfComponents( ): 0;
  if( tcComps < 1 || 3 < tcComps )
  {
    tcoords = nullptr;
    tcComps = 0;
  } // end if

  // 2. Collect triangle indices.
  vtkCellArray* polys = surface->GetPolys( );
  std::vector< std::uint32_t > indices;
  indices.reserve( std::size_t( polys->GetNumberOfCells( ) ) * 3 );

  auto ids = vtkSmartPointer< vtkIdList >::New( );
  polys->InitTraversal( );
  while( polys->GetNextCell( ids ) )
  {
    if( ids->GetNumberOfIds( ) == 3 )
    {
      for( vtkIdType k = 0; k < 3; ++k )
        indices.push_back( std::uint32_t( ids->GetId( k ) ) );
    } // end if
  } // end while
  if( indices.empty( ) )
    throw std::runtime_error(
      "[pujOgre::FillOgreMeshFromvtkPolyData]: vtkPolyData has no"
      " polygons or strips"
      );

  // 3. Interleaved vertex data: position [+ normal] [+ texcoord].
  std::size_t vertexCount = points->GetNumberOfPoints( );
  std::size_t floatsPerVertex
    =
    ( ( normals != nullptr )? 3: 0 ) + std::size_t( tcComps ) + 3;

  std::vector< float > vertices( vertexCount * floatsPerVertex );
  float* dst = vertices.data( );

  const Ogre::Real inf = std::numeric_limits< Ogre::Real >::max( );
  Ogre::Vector3 minB( inf, inf, inf );
  Ogre::Vector3 maxB( -inf, -inf, -inf );
  Ogre::Real maxSqRadius = 0;

  double p[ 3 ], n[ 3 ], t[ 3 ];
  for( vtkIdType i = 0; i < vtkIdType( vertexCount ); ++i )
  {
    points->GetPoint( i, p );
    Ogre::Vector3 pos( p[ 0 ], p[ 1 ], p[ 2 ] );
    *dst++ = float( pos.x );
    *dst++ = float( pos.y );
    *dst++ = float( pos.z );

    minB.makeFloor( pos );
    maxB.makeCeil( pos );
    maxSqRadius = std::max( maxSqRadius, pos.squaredLength( ) );

    if( normals != nullptr )
    {
      normals->GetTuple( i, n );
      Ogre::Vector3 nv( n[ 0 ], n[ 1 ], n[ 2 ] );
      nv.normalise( );
      *dst++ = float( nv.x );
      *dst++ = float( nv.y );
      *dst++ = float( nv.z );
    } // end if

    if( tcoords != nullptr )
    {
      tcoords->GetTuple( i, t );
      if( flipV && tcComps >= 2 )
        t[ 1 ] = 1.0 - t[ 1 ];
      for( int c = 0; c < tcComps; ++c )
        *dst++ = float( t[ c ] );
    } // end if
  } // end for

  // 4. Build the Ogre mesh.
  Ogre::SubMesh* sub = mesh->createSubMesh( );
  sub->useSharedVertices = false;

  sub->vertexData = new Ogre::VertexData( );
  sub->vertexData->vertexStart = 0;
  sub->vertexData->vertexCount = vertexCount;

  Ogre::VertexDeclaration* decl = sub->vertexData->vertexDeclaration;
  size_t offset = 0;
  offset += decl->addElement(
    0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION
    ).getSize( );
  if( normals != nullptr )
    offset += decl->addElement(
      0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL
      ).getSize( );
  if( tcoords != nullptr )
    offset += decl->addElement(
      0, offset,
      Ogre::VertexElement::multiplyTypeCount( Ogre::VET_FLOAT1, tcComps ),
      Ogre::VES_TEXTURE_COORDINATES, 0
      ).getSize( );

  Ogre::HardwareVertexBufferSharedPtr vbuf
    =
    Ogre::HardwareBufferManager::getSingleton( ).createVertexBuffer(
      decl->getVertexSize( 0 ), vertexCount,
      Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY
      );
  vbuf->writeData( 0, vbuf->getSizeInBytes( ), vertices.data( ), true );
  sub->vertexData->vertexBufferBinding->setBinding( 0, vbuf );

  // Index buffer: 16-bit when it fits, else 32-bit.
  bool use32 = ( vertexCount > 65535 );
  Ogre::HardwareIndexBufferSharedPtr ibuf
    =
    Ogre::HardwareBufferManager::getSingleton( ).createIndexBuffer(
      ( use32 )
      ? Ogre::HardwareIndexBuffer::IT_32BIT
      : Ogre::HardwareIndexBuffer::IT_16BIT,
      indices.size( ),
      Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY
      );

  if( use32 )
    ibuf->writeData( 0, ibuf->getSizeInBytes( ), indices.data( ), true );
  else
  {
    std::vector< std::uint16_t > indices16( indices.begin( ), indices.end( ) );
    ibuf->writeData( 0, ibuf->getSizeInBytes( ), indices16.data( ), true );
  } // end if

  sub->indexData->indexBuffer = ibuf;
  sub->indexData->indexStart = 0;
  sub->indexData->indexCount = indices.size( );
  sub->setMaterialName( materialName );

  mesh->_setBounds( Ogre::AxisAlignedBox( minB, maxB ), false );
  mesh->_setBoundingSphereRadius( std::sqrt( maxSqRadius ) );
}

// -------------------------------------------------------------------------
Ogre::MeshPtr pujOgre::
vtkPolyData2OgreMesh(
  vtkPolyData* polyData,
  const Ogre::String& meshName,
  const Ogre::String& materialName,
  bool generateNormalsIfMissing,
  bool flipV,
  const Ogre::String& group
  )
{
  Ogre::MeshPtr mesh
    =
    Ogre::MeshManager::getSingleton( ).createManual( meshName, group );
  try
  {
    pujOgre::FillOgreMeshFromvtkPolyData(
      polyData, mesh.get( ), materialName, generateNormalsIfMissing, flipV
      );
  }
  catch( ... )
  {
    Ogre::MeshManager::getSingleton( ).remove( mesh );
    throw;
  } // end try
  mesh->load( );
  return( mesh );
}

// eof - vtkPolyData2OgreMesh.cxx
