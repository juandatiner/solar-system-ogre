// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

#include <plugins/DotPPScene/DotPPSceneLoader.h>

#include <pugixml.hpp>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <pujOgre/vtkPolyData2OgreMesh.h>

#include <Ogre.h>
#include <OgreComponents.h>

#ifdef OGRE_BUILD_COMPONENT_TERRAIN
#  include <OgreTerrain.h>
#  include <OgreTerrainGroup.h>
#endif

#include <plugins/DotPPScene/ParametricSurfaceBuilder.h>

#ifndef OGRE_BUILD_COMPONENT_TERRAIN
namespace Ogre
{
  class TerrainGroup { };
} // end namespace
#endif

namespace
{
  // -----------------------------------------------------------------------
  Ogre::String getAttrib(
    const pugi::xml_node& XMLNode, const Ogre::String& attrib,
    const Ogre::String& defaultValue = ""
    )
  {
    if( auto anode = XMLNode.attribute( attrib.c_str( ) ) )
      return( anode.value( ) );
    else
      return( defaultValue );
  }

  // -----------------------------------------------------------------------
  Ogre::Real getAttribReal(
    const pugi::xml_node& XMLNode, const Ogre::String& attrib,
    Ogre::Real defaultValue = 0
    )
  {
    if( auto anode = XMLNode.attribute( attrib.c_str( ) ) )
      return( Ogre::StringConverter::parseReal( anode.value( ) ) );
    else
      return( defaultValue );
  }

  // -----------------------------------------------------------------------
  bool getAttribBool(
    const pugi::xml_node& XMLNode, const Ogre::String& attrib,
    bool defaultValue = false
    )
  {
    if( auto anode = XMLNode.attribute( attrib.c_str( ) ) )
      return( anode.as_bool( ) );
    else
      return( defaultValue );
  }

  // -----------------------------------------------------------------------
  Ogre::Vector3 parseVector3( const pugi::xml_node& XMLNode )
  {
    return(
      Ogre::Vector3(
        Ogre::StringConverter::parseReal( XMLNode.attribute( "x" ).value( ) ),
        Ogre::StringConverter::parseReal( XMLNode.attribute( "y" ).value( ) ),
        Ogre::StringConverter::parseReal( XMLNode.attribute( "z" ).value( ) )
        )
      );
  }

  // -----------------------------------------------------------------------
  Ogre::Quaternion parseQuaternion( const pugi::xml_node& XMLNode )
  {
    Ogre::Quaternion orientation;
    if( XMLNode.attribute( "qw" ) )
    {
      orientation.w = Ogre::StringConverter::parseReal( XMLNode.attribute( "qw" ).value( ) );
      orientation.x = Ogre::StringConverter::parseReal( XMLNode.attribute( "qx" ).value( ) );
      orientation.y = Ogre::StringConverter::parseReal( XMLNode.attribute( "qy" ).value( ) );
      orientation.z = Ogre::StringConverter::parseReal( XMLNode.attribute( "qz" ).value( ) );
    }
    else if( XMLNode.attribute( "axisX" ) )
    {
      Ogre::Vector3 axis;
      axis.x = Ogre::StringConverter::parseReal( XMLNode.attribute( "axisX" ).value( ) );
      axis.y = Ogre::StringConverter::parseReal( XMLNode.attribute( "axisY" ).value( ) );
      axis.z = Ogre::StringConverter::parseReal( XMLNode.attribute( "axisZ" ).value( ) );
      Ogre::Real angle = Ogre::StringConverter::parseReal( XMLNode.attribute( "angle" ).value( ) );
      orientation.FromAngleAxis( Ogre::Radian( angle ), axis );
    }
    else if( XMLNode.attribute( "angleX" ) )
    {
      Ogre::Matrix3 rot;
      rot.FromEulerAnglesXYZ(
        Ogre::StringConverter::parseAngle( XMLNode.attribute( "angleX" ).value( ) ),
        Ogre::StringConverter::parseAngle( XMLNode.attribute( "angleY" ).value( ) ),
        Ogre::StringConverter::parseAngle( XMLNode.attribute( "angleZ" ).value( ) )
        );
      orientation.FromRotationMatrix( rot );
    }
    else if( XMLNode.attribute( "x" ) )
    {
      orientation.x = Ogre::StringConverter::parseReal( XMLNode.attribute( "x" ).value( ) );
      orientation.y = Ogre::StringConverter::parseReal( XMLNode.attribute( "y" ).value( ) );
      orientation.z = Ogre::StringConverter::parseReal( XMLNode.attribute( "z" ).value( ) );
      orientation.w = Ogre::StringConverter::parseReal( XMLNode.attribute( "w" ).value( ) );
    }
    else if( XMLNode.attribute( "w" ) )
    {
      orientation.w = Ogre::StringConverter::parseReal( XMLNode.attribute( "w" ).value( ) );
      orientation.x = Ogre::StringConverter::parseReal( XMLNode.attribute( "x" ).value( ) );
      orientation.y = Ogre::StringConverter::parseReal( XMLNode.attribute( "y" ).value( ) );
      orientation.z = Ogre::StringConverter::parseReal( XMLNode.attribute( "z" ).value( ) );
    } // end if
    return( orientation );
  }

  // -----------------------------------------------------------------------
  Ogre::ColourValue parseColour( pugi::xml_node& XMLNode )
  {
    return(
      Ogre::ColourValue(
        Ogre::StringConverter::parseReal( XMLNode.attribute( "r" ).value( ) ),
        Ogre::StringConverter::parseReal( XMLNode.attribute( "g" ).value( ) ),
        Ogre::StringConverter::parseReal( XMLNode.attribute( "b" ).value( ) ),
        ( XMLNode.attribute( "a" ) != NULL )
        ? Ogre::StringConverter::parseReal( XMLNode.attribute( "a" ).value( ) )
        : 1
        )
      );
  }
} // end namespace

// -------------------------------------------------------------------------
Ogre::DotPPSceneLoader::
DotPPSceneLoader( )
  : m_SceneMgr( 0 ),
    m_BackgroundColour( Ogre::ColourValue::Black ),
    m_ItemPrefix( "" )
{
}

// -------------------------------------------------------------------------
Ogre::DotPPSceneLoader::
~DotPPSceneLoader( )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
load(
  Ogre::DataStreamPtr& stream,
  const Ogre::String& groupName,
  Ogre::SceneNode* rootNode
  )
{
  this->m_sGroupName = groupName;
  this->m_SceneMgr = rootNode->getCreator( );

  pugi::xml_document XMLDoc;

  auto optsAny
    =
    rootNode->getUserObjectBindings( ).getUserAny( "_DotSceneLoaderOptions" );
  if( optsAny.has_value( ) )
  {
    std::map< Ogre::String, Ogre::Any > options
      =
      Ogre::any_cast< std::map< Ogre::String, Ogre::Any > >( optsAny );
    if( options.find( "namePrefix" ) != options.end( ) )
    {
      Ogre::Any optionValue = options[ "namePrefix" ];
      if( optionValue.has_value( ) )
        this->m_ItemPrefix = Ogre::any_cast< Ogre::String >( optionValue );
    } // end if
  } // end if

  auto result
    =
    XMLDoc.load_buffer( stream->getAsString( ).c_str( ), stream->size( ) );
  if( !result )
  {
    Ogre::LogManager::getSingleton( ).logError(
      "DotPPSceneLoader - " + Ogre::String( result.description( ) )
      );
    return;
  } // end if

  // Grab the scene node
  auto XMLRoot = XMLDoc.child( "scene" );

  // Validate the File
  if( !XMLRoot.attribute( "formatVersion" ) )
  {
    Ogre::LogManager::getSingleton( ).logError(
      "DotSceneLoader - Invalid .scene File. "
      "Missing <scene formatVersion='x.y' >"
      );
    return;
  } // end if

  // figure out where to attach any nodes we create
  this->m_AttachNode = rootNode;

  // Process the scene
  this->processScene( XMLRoot );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
exportScene( Ogre::SceneNode* rootNode, const Ogre::String& outFileName )
{
}

// -------------------------------------------------------------------------
const Ogre::ColourValue& Ogre::DotPPSceneLoader::
getBackgroundColour( )
{
  return( this->m_BackgroundColour );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
writeNode( pugi::xml_node& parentXML, const Ogre::SceneNode* n )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processScene( pugi::xml_node& XMLRoot )
{
  // Process the scene parameters
  Ogre::String version = getAttrib( XMLRoot, "formatVersion", "unknown" );
  Ogre::String message
    =
    "[DotSceneLoader] Parsing dotScene file with version " + version;
  if( XMLRoot.attribute( "sceneManager" ) )
    message
      += ", scene manager "
      + Ogre::String( XMLRoot.attribute( "sceneManager" ).value( ) );
  if( XMLRoot.attribute( "minOgreVersion" ) )
    message
      += ", min. Ogre version "
      + Ogre::String( XMLRoot.attribute( "minOgreVersion" ).value( ) );
  if( XMLRoot.attribute( "author" ) )
    message
      += ", author "
      + Ogre::String( XMLRoot.attribute( "author" ).value( ) );

  Ogre::LogManager::getSingleton( ).logMessage( message );

  // Process environment ( ? )
  if( auto pElement = XMLRoot.child( "environment" ) )
    this->processEnvironment( pElement );

  // Process parametric surface definitions ( ? ) -- before the nodes that reference them
  if( auto pElement = XMLRoot.child( "parametricSurfaces" ) )
    this->processParametricSurfaceDefinitions( pElement );

  // Process nodes ( ? )
  if( auto pElement = XMLRoot.child( "nodes" ) )
    this->processNodes( pElement );

  // Process externals ( ? )
  if( auto pElement = XMLRoot.child( "externals" ) )
    this->processExternals( pElement );

  // Process userDataReference ( ? )
  if( auto pElement = XMLRoot.child( "userData" ) )
    this->processUserData( pElement, this->m_AttachNode->getUserObjectBindings( ) );

  // Process light ( ? )
  if( auto pElement = XMLRoot.child( "light" ) )
    this->processLight( pElement );

  // Process camera ( ? )
  if( auto pElement = XMLRoot.child( "camera" ) )
    this->processCamera( pElement );

  // Process terrain ( ? )
  if( auto pElement = XMLRoot.child( "terrainGroup" ) )
    this->processTerrainGroup( pElement );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processNodes( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Nodes...", Ogre::LML_TRIVIAL
    );

  // Process node ( * )
  for( auto pElement: XMLNode.children( "node" ) )
    this->processNode( pElement );

  // Process position ( ? )
  if( auto pElement = XMLNode.child( "position" ) )
    this->m_AttachNode->setPosition( parseVector3( pElement ) );

  // Process rotation ( ? )
  if( auto pElement = XMLNode.child( "rotation" ) )
    this->m_AttachNode->setOrientation( parseQuaternion( pElement ) );

  // Process scale ( ? )
  if( auto pElement = XMLNode.child( "scale" ) )
    this->m_AttachNode->setScale( parseVector3( pElement ) );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processExternals( pugi::xml_node& XMLNode )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processEnvironment( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Environment...", Ogre::LML_TRIVIAL
    );

  // Process camera ( ? )
  if( auto pElement = XMLNode.child( "camera" ) )
    this->processCamera( pElement );

  // Process fog ( ? )
  if( auto pElement = XMLNode.child( "fog" ) )
    this->processFog( pElement );

  // Process skyBox ( ? )
  if( auto pElement = XMLNode.child( "skyBox" ) )
    this->processSkyBox( pElement );

  // Process skyDome ( ? )
  if( auto pElement = XMLNode.child( "skyDome" ) )
    this->processSkyDome( pElement );

  // Process skyPlane ( ? )
  if( auto pElement = XMLNode.child( "skyPlane" ) )
    this->processSkyPlane( pElement );

  // Process colourAmbient ( ? )
  if( auto pElement = XMLNode.child( "colourAmbient" ) )
    this->m_SceneMgr->setAmbientLight( parseColour( pElement ) );

  // Process colourBackground ( ? )
  if( auto pElement = XMLNode.child( "colourBackground" ) )
    this->m_BackgroundColour = parseColour( pElement );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processTerrainGroup( pugi::xml_node& XMLNode )
{
#ifdef OGRE_BUILD_COMPONENT_TERRAIN
  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Terrain Group...", Ogre::LML_TRIVIAL
    );

  Ogre::Real worldSize = getAttribReal( XMLNode, "worldSize" );
  int mapSize
    =
    Ogre::StringConverter::parseInt( XMLNode.attribute( "size" ).value( ) );
  int compositeMapDistance
    =
    Ogre::StringConverter::parseInt(
      XMLNode.attribute( "tuningCompositeMapDistance" ).value( )
      );
  int maxPixelError
    =
    Ogre::StringConverter::parseInt(
      XMLNode.attribute( "tuningMaxPixelError" ).value( )
      );

  auto terrainGlobalOptions = Ogre::TerrainGlobalOptions::getSingletonPtr( );
  OgreAssert( terrainGlobalOptions, "TerrainGlobalOptions not available" );

  terrainGlobalOptions->setMaxPixelError( ( Ogre::Real )maxPixelError );
  terrainGlobalOptions->setCompositeMapDistance( ( Ogre::Real )compositeMapDistance );

  auto terrainGroup = std::make_shared< Ogre::TerrainGroup >(
    this->m_SceneMgr, Ogre::Terrain::ALIGN_X_Z, mapSize, worldSize
    );
  terrainGroup->setOrigin( Ogre::Vector3::ZERO );
  terrainGroup->setResourceGroup( this->m_sGroupName );

  // Process terrain pages ( * )
  for( auto pPageElement: XMLNode.children( "terrain" ) )
  {
    int pageX = Ogre::StringConverter::parseInt( pPageElement.attribute( "x" ).value( ) );
    int pageY = Ogre::StringConverter::parseInt( pPageElement.attribute( "y" ).value( ) );
    terrainGroup->defineTerrain( pageX, pageY, pPageElement.attribute( "dataFile" ).value( ) );
  } // end for
  terrainGroup->loadAllTerrains( true );

  terrainGroup->freeTemporaryResources( );

  this->m_AttachNode->getUserObjectBindings( )
    .setUserAny( "TerrainGroup", terrainGroup );
#else
  OGRE_EXCEPT(
    Exception::ERR_INVALID_CALL, "recompile with Terrain component"
    );
#endif
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processBlendmaps( pugi::xml_node& XMLNode )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processUserData( pugi::xml_node& XMLNode, Ogre::UserObjectBindings& userData )
{
  // Process node ( * )
  for( auto pElement: XMLNode.children( "property" ) )
  {
    Ogre::String name = getAttrib( pElement, "name" );
    Ogre::String type = getAttrib( pElement, "type" );
    Ogre::String data = getAttrib( pElement, "data" );

    Ogre::Any value;
    if( type == "bool" )
      value = Ogre::StringConverter::parseBool( data );
    else if( type == "float" )
      value = Ogre::StringConverter::parseReal( data );
    else if( type == "int" )
      value = Ogre::StringConverter::parseInt( data );
    else
      value = data;

    userData.setUserAny( name, value );
  } // end for
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processLight( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Process attributes
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Light: " + name, Ogre::LML_TRIVIAL
    );

  // Create the light
  Ogre::Light* pLight = this->m_SceneMgr->createLight( name );
  if( pParent )
    pParent->attachObject( pLight );

  Ogre::String sValue = getAttrib( XMLNode, "type" );
  if( sValue == "point" )
    pLight->setType( Ogre::Light::LT_POINT );
  else if( sValue == "directional" )
    pLight->setType( Ogre::Light::LT_DIRECTIONAL );
  else if( sValue == "spot" )
    pLight->setType( Ogre::Light::LT_SPOTLIGHT );
  else if( sValue == "radPoint" )
    pLight->setType( Ogre::Light::LT_POINT );
  else if( sValue == "rect" )
    pLight->setType( Ogre::Light::LT_RECTLIGHT );

  pLight->setVisible( getAttribBool( XMLNode, "visible", true ) );
  pLight->setCastShadows( getAttribBool( XMLNode, "castShadows", true ) );
  pLight->setPowerScale( getAttribReal( XMLNode, "powerScale", 1.0 ) );

  // Process colourDiffuse ( ? )
  if( auto pElement = XMLNode.child( "colourDiffuse" ) )
    pLight->setDiffuseColour( parseColour( pElement ) );

  // Process colourSpecular ( ? )
  if( auto pElement = XMLNode.child( "colourSpecular" ) )
    pLight->setSpecularColour( parseColour( pElement ) );

  if( sValue != "directional" )
  {
    // Process lightRange ( ? )
    if( auto pElement = XMLNode.child( "lightRange" ) )
      this->processLightRange( pElement, pLight );

    // Process lightAttenuation ( ? )
    if( auto pElement = XMLNode.child( "lightAttenuation" ) )
      this->processLightAttenuation( pElement, pLight );
  } // end if

  if( sValue == "rect" )
  {
    // Process lightSourceSize ( ? )
    if( auto pElement = XMLNode.child( "lightSourceSize" ) )
      this->processLightSourceSize( pElement, pLight );
  } // end if

  // Process userDataReference ( ? )
  if( auto pElement = XMLNode.child( "userData" ) )
    this->processUserData( pElement, pLight->getUserObjectBindings( ) );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processCamera( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Process attributes
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Camera: " + name, Ogre::LML_TRIVIAL
    );

  // Real fov = getAttribReal( XMLNode, "fov", 45 );
  Ogre::Real aspectRatio = getAttribReal( XMLNode, "aspectRatio", 1.3333 );
  Ogre::String projectionType = getAttrib( XMLNode, "projectionType", "perspective" );

  // Create the camera
  Ogre::Camera* pCamera = this->m_SceneMgr->createCamera( name );

  // construct a scenenode is no parent
  if( !pParent )
    pParent = this->m_AttachNode->createChildSceneNode( name );
  pParent->attachObject( pCamera );

  // Set the field-of-view
  //! @todo Is this always in degrees?
  // pCamera->setFOVy( Degree( fov ) );

  // Set the aspect ratio
  pCamera->setAspectRatio( aspectRatio );

  // Set the projection type
  if( projectionType == "perspective" )
    pCamera->setProjectionType( PT_PERSPECTIVE );
  else if( projectionType == "orthographic" )
    pCamera->setProjectionType( PT_ORTHOGRAPHIC );

  // Process clipping ( ? )
  if( auto pElement = XMLNode.child( "clipping" ) )
  {
    Ogre::Real nearDist = getAttribReal( pElement, "near" );
    pCamera->setNearClipDistance( nearDist );

    Ogre::Real farDist = getAttribReal( pElement, "far" );
    pCamera->setFarClipDistance( farDist );
  } // end if

  // Process userDataReference ( ? )
  if( auto pElement = XMLNode.child( "userData" ) )
    this->processUserData(
      pElement,
      static_cast< Ogre::MovableObject* >( pCamera )
      ->getUserObjectBindings( )
      );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processNode( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Construct the node's name
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage(
    "[DotSceneLoader] Processing Node: " + name, Ogre::LML_TRIVIAL
    );

  // Create the scene node
  Ogre::SceneNode* pNode;
  if( name.empty( ) )
  {
    // Let Ogre choose the name
    if( pParent )
      pNode = pParent->createChildSceneNode( );
    else
      pNode = this->m_AttachNode->createChildSceneNode( );
  }
  else
  {
    // Provide the name
    if( pParent )
      pNode = pParent->createChildSceneNode( this->m_ItemPrefix + name );
    else
      pNode = this->m_AttachNode->createChildSceneNode( this->m_ItemPrefix + name );
  } // end if

  // Process other attributes
  // Process position ( ? )
  if( auto pElement = XMLNode.child( "position" ) )
    pNode->setPosition( parseVector3( pElement ) );

  // Process rotation ( ? )
  if( auto pElement = XMLNode.child( "rotation" ) )
    pNode->setOrientation( parseQuaternion( pElement ) );

  // Process scale ( ? )
  if( auto pElement = XMLNode.child( "scale" ) )
    pNode->setScale( parseVector3( pElement ) );

  // Process lookTarget ( ? )
  if( auto pElement = XMLNode.child( "lookTarget" ) )
    this->processLookTarget( pElement, pNode );

  // Process trackTarget ( ? )
  if( auto pElement = XMLNode.child( "trackTarget" ) )
    this->processTrackTarget( pElement, pNode );

  // Process node ( * )
  for( auto pElement : XMLNode.children( "node" ) )
    this->processNode( pElement, pNode );

  // Process entity ( * )
  for( auto pElement : XMLNode.children( "entity" ) )
    this->processEntity( pElement, pNode );

  // Process light ( * )
  for( auto pElement : XMLNode.children( "light" ) )
    this->processLight( pElement, pNode );

  // Process camera ( * )
  for( auto pElement : XMLNode.children( "camera" ) )
    this->processCamera( pElement, pNode );

  // Process particleSystem ( * )
  for( auto pElement : XMLNode.children( "particleSystem" ) )
    this->processParticleSystem( pElement, pNode );

  // Process billboardSet ( * )
  for( auto pElement : XMLNode.children( "billboardSet" ) )
    this->processBillboardSet( pElement, pNode );

  // Process plane ( * )
  for( auto pElement : XMLNode.children( "plane" ) )
    this->processPlane( pElement, pNode );

  // Process parametricSurface ( * )
  for( auto pElement : XMLNode.children( "parametricSurface" ) )
    this->processParametricSurface( pElement, pNode );

  // Process userDataReference ( ? )
  if( auto pElement = XMLNode.child( "userData" ) )
    this->processUserData( pElement, pNode->getUserObjectBindings( ) );

  // Process node animations ( ? )
  if( auto pElement = XMLNode.child( "animations" ) )
    this->processNodeAnimations( pElement, pNode );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processLookTarget( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  //! @todo Is this correct? Cause I don't have a clue actually

  // Process attributes
  Ogre::String nodeName = getAttrib( XMLNode, "nodeName" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Look Target, nodeName: " + nodeName, Ogre::LML_TRIVIAL );

  Node::TransformSpace relativeTo = Node::TS_PARENT;
  Ogre::String sValue = getAttrib( XMLNode, "relativeTo" );
  if( sValue == "local" )
    relativeTo = Node::TS_LOCAL;
  else if( sValue == "parent" )
    relativeTo = Node::TS_PARENT;
  else if( sValue == "world" )
    relativeTo = Node::TS_WORLD;

  // Process position ( ? )
  Vector3 position;
  if( auto pElement = XMLNode.child( "position" ) )
    position = parseVector3( pElement );

  // Process localDirection ( ? )
  Vector3 localDirection = Vector3::NEGATIVE_UNIT_Z;
  if( auto pElement = XMLNode.child( "localDirection" ) )
    localDirection = parseVector3( pElement );

  // Setup the look target
  try
  {
    if( !nodeName.empty( ) )
    {
      SceneNode* pLookNode = this->m_SceneMgr->getSceneNode( nodeName );
      position = pLookNode->_getDerivedPosition( );
    }

    pParent->lookAt( position, relativeTo, localDirection );
  }
  catch ( const Exception& e )
  {
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - " + e.getDescription( ) );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processTrackTarget( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Process attributes
  Ogre::String nodeName = getAttrib( XMLNode, "nodeName" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Track Target, nodeName: " + nodeName, Ogre::LML_TRIVIAL );

  // Process localDirection ( ? )
  Vector3 localDirection = Vector3::NEGATIVE_UNIT_Z;
  if( auto pElement = XMLNode.child( "localDirection" ) )
    localDirection = parseVector3( pElement );

  // Process offset ( ? )
  Vector3 offset = Vector3::ZERO;
  if( auto pElement = XMLNode.child( "offset" ) )
    offset = parseVector3( pElement );

  // Setup the track target
  try
  {
    SceneNode* pTrackNode = this->m_SceneMgr->getSceneNode( nodeName );
    pParent->setAutoTracking( true, pTrackNode, localDirection, offset );
  }
  catch ( const Exception& e )
  {
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - " + e.getDescription( ) );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processEntity( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Process attributes
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Entity: " + this->m_ItemPrefix + name, Ogre::LML_TRIVIAL );

  Ogre::String meshFile = getAttrib( XMLNode, "meshFile" );
  Ogre::String staticGeometry = getAttrib( XMLNode, "static" );
  Ogre::String instancedManager = getAttrib( XMLNode, "instanced" );
  Ogre::String material = getAttrib( XMLNode, "material" );
  bool castShadows = getAttribBool( XMLNode, "castShadows", true );
  bool visible = getAttribBool( XMLNode, "visible", true );

  // Create the entity
  MovableObject* pEntity = 0;

  try
  {
    // If the Entity is instanced then the creation path is different
    if( !instancedManager.empty( ) )
    {
      Ogre::LogManager::getSingleton( ).logMessage(
        "[DotSceneLoader] Adding entity: " + name + " to Instance Manager: " + instancedManager, Ogre::LML_TRIVIAL );

      // Load the Mesh to get the material name of the first submesh
      Ogre::MeshPtr mesh = MeshManager::getSingletonPtr( )->load( meshFile, this->m_sGroupName );

      // Get the material name of the entity
      if( !material.empty( ) )
        pEntity = this->m_SceneMgr->createInstancedEntity( material, instancedManager );
      else
        pEntity = this->m_SceneMgr->createInstancedEntity( mesh->getSubMesh( 0 )->getMaterialName( ), instancedManager );

      pParent->attachObject( static_cast<InstancedEntity*>( pEntity ) );
    }
    else
    {
      pEntity = this->m_SceneMgr->createEntity( this->m_ItemPrefix + name, meshFile, this->m_sGroupName );

      static_cast<Entity*>( pEntity )->setCastShadows( castShadows );
      static_cast<Entity*>( pEntity )->setVisible( visible );

      if( !material.empty( ) )
        static_cast<Entity*>( pEntity )->setMaterialName( material );

      // If the Entity belongs to a Static Geometry group then it doesn't get attached to a node
      // * TODO * : Clean up nodes without attached entities or children nodes? ( should be done afterwards
      // if the hierarchy is being processed )
      if( !staticGeometry.empty( ) )
      {
        Ogre::LogManager::getSingleton( ).logMessage(
          "[DotSceneLoader] Adding entity: " + name + " to Static Group: " + staticGeometry, Ogre::LML_TRIVIAL );
        this->m_SceneMgr->getStaticGeometry( staticGeometry )
          ->addEntity( static_cast<Entity*>( pEntity ), pParent->_getDerivedPosition( ),
                       pParent->_getDerivedOrientation( ), pParent->_getDerivedScale( ) );
      }
      else
      {
        Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] pParent->attachObject( ): " + name, Ogre::LML_TRIVIAL );
        pParent->attachObject( static_cast<Entity*>( pEntity ) );
      }
    }
  }
  catch ( const Exception& e )
  {
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - " + e.getDescription( ) );
    return;
  }

  // Process userDataReference ( ? )
  if( auto pElement = XMLNode.child( "userData" ) )
    processUserData( pElement, pEntity->getUserObjectBindings( ) );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processParticleSystem( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
// Process attributes
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Particle System: " + name, Ogre::LML_TRIVIAL );

  Ogre::String templateName = getAttrib( XMLNode, "template" );

  if( templateName.empty( ) )
    templateName = getAttrib( XMLNode, "file" ); // compatibility with old scenes

  // Create the particle system
  try
  {
    ParticleSystem* pParticles = this->m_SceneMgr->createParticleSystem( name, templateName );
    pParent->attachObject( pParticles );
  }
  catch ( const Exception& e )
  {
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - " + e.getDescription( ) );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processBillboardSet( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processPlane( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Plane: " + name, Ogre::LML_TRIVIAL );

  Real distance = getAttribReal( XMLNode, "distance" );
  Real width = getAttribReal( XMLNode, "width" );
  Real height = getAttribReal( XMLNode, "height" );
  int xSegments = Ogre::StringConverter::parseInt( getAttrib( XMLNode, "xSegments" ) );
  int ySegments = Ogre::StringConverter::parseInt( getAttrib( XMLNode, "ySegments" ) );
  int numTexCoordSets = Ogre::StringConverter::parseInt( getAttrib( XMLNode, "numTexCoordSets" ) );
  Real uTile = getAttribReal( XMLNode, "uTile" );
  Real vTile = getAttribReal( XMLNode, "vTile" );
  Ogre::String material = getAttrib( XMLNode, "material" );
  bool hasNormals = getAttribBool( XMLNode, "hasNormals" );
  Vector3 normal = parseVector3( XMLNode.child( "normal" ) );
  Vector3 up = parseVector3( XMLNode.child( "upVector" ) );

  Plane plane( normal, distance );
  MeshPtr res =
    MeshManager::getSingletonPtr( )->createPlane( name + "mesh", this->m_sGroupName, plane, width, height, xSegments,
                                                  ySegments, hasNormals, numTexCoordSets, uTile, vTile, up );
  Entity* ent = this->m_SceneMgr->createEntity( name, name + "mesh" );

  ent->setMaterialName( material );

  pParent->attachObject( ent );
}

// -------------------------------------------------------------------------
namespace
{
  std::uint32_t gParametricAutoName = 0;
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processParametricSurface( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  Ogre::String name = getAttrib( XMLNode, "name" );
  Ogre::String ref = getAttrib( XMLNode, "ref" );
  Ogre::String entityName = name.empty( ) ? "ParametricSurface" + Ogre::StringConverter::toString( gParametricAutoName++ ) : name;

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing ParametricSurface: " + this->m_ItemPrefix +
                                                entityName + ( ref.empty( ) ? "" : " ( ref " + ref + " )" ),
                                                Ogre::LML_TRIVIAL );

  bool castShadows = getAttribBool( XMLNode, "castShadows", true );
  bool visible = getAttribBool( XMLNode, "visible", true );
  Ogre::String material = getAttrib( XMLNode, "material" ); // empty: keep the mesh's material

  try
  {
    MeshPtr mesh;
    if( !ref.empty( ) )
    {
      // Instance of a surface defined elsewhere ( in <parametricSurfaces> or inline earlier )
      mesh = MeshManager::getSingleton( ).getByName( this->m_ItemPrefix + ref + ".parametric", this->m_sGroupName );
      if( !mesh )
        OGRE_EXCEPT( Exception::ERR_ITEM_NOT_FOUND,
                     "no parametric surface named '" + ref + "' has been defined before this point" );
    }
    else
    {
      // Inline definition: builds the mesh ( or reuses one with the same name )
      mesh = createParametricMesh( XMLNode, entityName );
    }

    Entity* ent = this->m_SceneMgr->createEntity( this->m_ItemPrefix + entityName, mesh );
    if( !material.empty( ) )
      ent->setMaterialName( material );
    ent->setCastShadows( castShadows );
    ent->setVisible( visible );
    pParent->attachObject( ent );

    if( auto pElement = XMLNode.child( "userData" ) )
      processUserData( pElement, ent->getUserObjectBindings( ) );
  }
  catch ( const std::exception& e ) // Ogre::Exception derives from std::exception
  {
    Ogre::LogManager::getSingleton( ).logError( "[DotSceneLoader] parametricSurface '" + entityName + "': " + e.what( ) );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processParametricSurfaceDefinitions( pugi::xml_node& XMLNode )
{
// <parametricSurfaces> only builds meshes; nodes create entities with ref="name"
  for( auto pElement : XMLNode.children( "parametricSurface" ) )
  {
    Ogre::String name = getAttrib( pElement, "name" );
    if( name.empty( ) )
    {
      Ogre::LogManager::getSingleton( ).logError(
        "[DotSceneLoader] parametricSurface definitions need a 'name' attribute - skipped" );
      continue;
    }

    Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Defining ParametricSurface: " + this->m_ItemPrefix + name,
                                                  Ogre::LML_TRIVIAL );
    try
    {
      createParametricMesh( pElement, name );
    }
    catch ( const std::exception& e ) // Ogre::Exception derives from std::exception
    {
      Ogre::LogManager::getSingleton( ).logError( "[DotSceneLoader] parametricSurface '" + name + "': " + e.what( ) );
    }
  }
}

// -------------------------------------------------------------------------
Ogre::MeshPtr Ogre::DotPPSceneLoader::
createParametricMesh( pugi::xml_node& XMLNode, const Ogre::String& name )
{
  Ogre::String meshName = this->m_ItemPrefix + name + ".parametric";

  // Already defined ( earlier in this file, or the same scene loaded before )
  if( MeshPtr mesh = MeshManager::getSingleton( ).getByName( meshName, this->m_sGroupName ) )
    return mesh;

  vtkSmartPointer<vtkPolyData> polyData = buildParametricPolyData( XMLNode );
  return pujOgre::vtkPolyData2OgreMesh(
    polyData, meshName, getAttrib( XMLNode, "material", "BaseWhite" ),
    /*generateNormalsIfMissing=*/true, /*flipV=*/true, this->m_sGroupName
    );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processNodeAnimations( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Node Animations for SceneNode: " + pParent->getName( ), Ogre::LML_TRIVIAL );

  // Process node animations ( * )
  for( auto pElement : XMLNode.children( "animation" ) )
  {
    processNodeAnimation( pElement, pParent );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processNodeAnimation( pugi::xml_node& XMLNode, Ogre::SceneNode* pParent )
{
  // Process node animation ( * )

  // Construct the animation name
  Ogre::String name = getAttrib( XMLNode, "name" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Node Animation: " + name, Ogre::LML_TRIVIAL );

  Real length = getAttribReal( XMLNode, "length" );

  Animation* anim = this->m_SceneMgr->createAnimation( name, length );

  bool enable = getAttribBool( XMLNode, "enable", false );
  bool loop = getAttribBool( XMLNode, "loop", false );

  Ogre::String interpolationMode = getAttrib( XMLNode, "interpolationMode" );

  if( interpolationMode == "linear" )
    anim->setInterpolationMode( Animation::IM_LINEAR );
  else if( interpolationMode == "spline" )
    anim->setInterpolationMode( Animation::IM_SPLINE );
  else
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - Invalid interpolationMode: " + interpolationMode );

  Ogre::String rotationInterpolationMode = getAttrib( XMLNode, "rotationInterpolationMode" );

  if( rotationInterpolationMode == "linear" )
    anim->setRotationInterpolationMode( Animation::RIM_LINEAR );
  else if( rotationInterpolationMode == "spherical" )
    anim->setRotationInterpolationMode( Animation::RIM_SPHERICAL );
  else
    Ogre::LogManager::getSingleton( ).logError( "DotSceneLoader - Invalid rotationInterpolationMode: " + rotationInterpolationMode );

  // create a track to animate the camera's node
  NodeAnimationTrack* track = anim->createNodeTrack( 0, pParent );
  pParent->setInitialState( );

  // Process keyframes ( * )
  for( auto pElement : XMLNode.children( "keyframe" ) )
  {
    processKeyframe( pElement, track );
  }

  // create a new animation state to track this
  auto animState = this->m_SceneMgr->createAnimationState( name );
  animState->setEnabled( enable );
  animState->setLoop( loop );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processKeyframe( pugi::xml_node& XMLNode, Ogre::NodeAnimationTrack* pTrack )
{
  // Process node animation keyframe ( * )
  Real time = getAttribReal( XMLNode, "time" );

  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Keyframe: " + Ogre::StringConverter::toString( time ), Ogre::LML_TRIVIAL );

  auto keyframe = pTrack->createNodeKeyFrame( time );

  // Process translation ( ? )
  if( auto pElement = XMLNode.child( "position" ) ) {
    Vector3 translation = parseVector3( pElement );
    keyframe->setTranslate( translation );
  }

  // Process rotation ( ? )
  //Quaternion rotation = Quaternion::IDENTITY;
  if( auto pElement = XMLNode.child( "rotation" ) ) {
    Quaternion rotation = parseQuaternion( pElement );
    keyframe->setRotation( rotation );
  }

  // Process scale ( ? )
  //Vector3 scale = parseVector3( XMLNode.child( "scale" ) );
  if( auto pElement = XMLNode.child( "scale" ) ) {
    Vector3 scale = parseVector3( pElement );
    keyframe->setScale( scale );
  }
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processFog( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing Fog...", Ogre::LML_TRIVIAL );

  // Process attributes
  Real expDensity = getAttribReal( XMLNode, "density", 0.001 );
  Real linearStart = getAttribReal( XMLNode, "start", 0.0 );
  Real linearEnd = getAttribReal( XMLNode, "end", 1.0 );

  FogMode mode = FOG_NONE;
  Ogre::String sMode = getAttrib( XMLNode, "mode" );
  if( sMode == "none" )
    mode = FOG_NONE;
  else if( sMode == "exp" )
    mode = FOG_EXP;
  else if( sMode == "exp2" )
    mode = FOG_EXP2;
  else if( sMode == "linear" )
    mode = FOG_LINEAR;
  else
    mode = ( FogMode )Ogre::StringConverter::parseInt( sMode );

  // Process colourDiffuse ( ? )
  ColourValue colourDiffuse = ColourValue::White;

  if( auto pElement = XMLNode.child( "colour" ) )
    colourDiffuse = parseColour( pElement );

  // Setup the fog
  this->m_SceneMgr->setFog( mode, colourDiffuse, expDensity, linearStart, linearEnd );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processSkyBox( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing SkyBox...", Ogre::LML_TRIVIAL );

  // Process attributes
  Ogre::String material = getAttrib( XMLNode, "material", "BaseWhite" );
  Real distance = getAttribReal( XMLNode, "distance", 5000 );
  bool drawFirst = getAttribBool( XMLNode, "drawFirst", true );

  // Process rotation ( ? )
  Quaternion rotation = Quaternion::IDENTITY;

  if( auto pElement = XMLNode.child( "rotation" ) )
    rotation = parseQuaternion( pElement );

  // Setup the sky box
  this->m_SceneMgr->setSkyBox( true, material, distance, drawFirst, rotation, this->m_sGroupName );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processSkyDome( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing SkyDome...", Ogre::LML_TRIVIAL );

  // Process attributes
  Ogre::String material = XMLNode.attribute( "material" ).value( );
  Real curvature = getAttribReal( XMLNode, "curvature", 10 );
  Real tiling = getAttribReal( XMLNode, "tiling", 8 );
  Real distance = getAttribReal( XMLNode, "distance", 4000 );
  bool drawFirst = getAttribBool( XMLNode, "drawFirst", true );
  bool active = getAttribBool( XMLNode, "active", false );
  if( !active )
    return;

  // Process rotation ( ? )
  Quaternion rotation = Quaternion::IDENTITY;
  if( auto pElement = XMLNode.child( "rotation" ) )
    rotation = parseQuaternion( pElement );

  // Setup the sky dome
  this->m_SceneMgr->setSkyDome(
    true, material, curvature, tiling, distance, drawFirst,
    rotation, 16, 16, -1, this->m_sGroupName
    );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processSkyPlane( pugi::xml_node& XMLNode )
{
  Ogre::LogManager::getSingleton( ).logMessage( "[DotSceneLoader] Processing SkyPlane...", Ogre::LML_TRIVIAL );

  // Process attributes
  Ogre::String material = getAttrib( XMLNode, "material" );
  Real planeX = getAttribReal( XMLNode, "planeX", 0 );
  Real planeY = getAttribReal( XMLNode, "planeY", -1 );
  Real planeZ = getAttribReal( XMLNode, "planeZ", 0 );
  Real planeD = getAttribReal( XMLNode, "planeD", 5000 );
  Real scale = getAttribReal( XMLNode, "scale", 1000 );
  Real bow = getAttribReal( XMLNode, "bow", 0 );
  Real tiling = getAttribReal( XMLNode, "tiling", 10 );
  bool drawFirst = getAttribBool( XMLNode, "drawFirst", true );

  // Setup the sky plane
  Plane plane;
  plane.normal = Vector3( planeX, planeY, planeZ );
  plane.d = planeD;
  this->m_SceneMgr->setSkyPlane( true, plane, material, scale, tiling, drawFirst, bow, 1, 1, this->m_sGroupName );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processLightRange( pugi::xml_node& XMLNode, Ogre::Light* pLight )
{
  // Process attributes
  Real inner = getAttribReal( XMLNode, "inner" );
  Real outer = getAttribReal( XMLNode, "outer" );
  Real falloff = getAttribReal( XMLNode, "falloff", 1.0 );

  // Setup the light range
  pLight->setSpotlightRange( Radian( inner ), Radian( outer ), falloff );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processLightAttenuation( pugi::xml_node& XMLNode, Ogre::Light* pLight )
{
// Process attributes
  Real range = getAttribReal( XMLNode, "range" );
  Real constant = getAttribReal( XMLNode, "constant" );
  Real linear = getAttribReal( XMLNode, "linear" );
  Real quadratic = getAttribReal( XMLNode, "quadratic" );

  // Setup the light attenuation
  pLight->setAttenuation( range, constant, linear, quadratic );
}

// -------------------------------------------------------------------------
void Ogre::DotPPSceneLoader::
processLightSourceSize( pugi::xml_node& XMLNode, Ogre::Light* pLight )
{
  // Process attributes
  Real width = getAttribReal( XMLNode, "width" );
  Real height = getAttribReal( XMLNode, "height" );

  // Setup the light range
  pLight->setSourceSize( width, height );
}

/**
 */
struct DotPPSceneCodec
  : public Ogre::Codec
{
  Ogre::String magicNumberToFileExt(const char* magicNumberPtr, size_t maxbytes) const override { return ""; }
  Ogre::String getType() const override { return "scenepp"; }
  void decode(const Ogre::DataStreamPtr& stream, const Ogre::Any& output) const override
    {
      Ogre::DataStreamPtr _stream(stream);
      Ogre::DotPPSceneLoader loader;
      loader.load(_stream, Ogre::ResourceGroupManager::getSingleton().getWorldResourceGroupName(),
                  Ogre::any_cast<Ogre::SceneNode*>(output));
    }

  void encodeToFile(const Ogre::Any& input, const Ogre::String& outFileName) const override
    {
      Ogre::DotPPSceneLoader loader;
      loader.exportScene(Ogre::any_cast<Ogre::SceneNode*>(input), outFileName);
    }
};

// -------------------------------------------------------------------------
const Ogre::String& Ogre::DotPPScenePlugin::
getName( ) const
{
  static Ogre::String name = "DotPPScene Loader";
  return name;
}

// -------------------------------------------------------------------------
void Ogre::DotPPScenePlugin::
install( )
{
  this->m_Codec = new DotPPSceneCodec( );
  Ogre::Codec::registerCodec( this->m_Codec );}

// -------------------------------------------------------------------------
void Ogre::DotPPScenePlugin::
initialise( )
{
}

// -------------------------------------------------------------------------
void Ogre::DotPPScenePlugin::
uninstall( )
{
  Ogre::Codec::unregisterCodec( this->m_Codec );
  delete this->m_Codec;
}

// -------------------------------------------------------------------------
void Ogre::DotPPScenePlugin::
shutdown( )
{
}

#ifndef OGRE_STATIC_LIB

// -------------------------------------------------------------------------
extern "C" void Plugin_pujOgreDotPPSceneCodec_EXPORT dllStartPlugin( );
extern "C" void Plugin_pujOgreDotPPSceneCodec_EXPORT dllStopPlugin( );
 
// -------------------------------------------------------------------------
static Ogre::DotPPScenePlugin plugin;
 
// -------------------------------------------------------------------------
extern "C" void Plugin_pujOgreDotPPSceneCodec_EXPORT dllStartPlugin( )
{
  Ogre::Root::getSingleton( ).installPlugin( &plugin );
}

// -------------------------------------------------------------------------
extern "C" void Plugin_pujOgreDotPPSceneCodec_EXPORT dllStopPlugin( )
{
  Ogre::Root::getSingleton( ).uninstallPlugin( &plugin );
}

#endif

// eof - DotPPSceneLoader.cxx
