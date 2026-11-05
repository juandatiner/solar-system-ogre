// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

#include "Application.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <sstream>

#include <Ogre.h>
#include <OgreCameraMan.h>

// -------------------------------------------------------------------------
namespace fs = std::filesystem;

// -------------------------------------------------------------------------
namespace
{
  Ogre::Real real_property(
    const Ogre::UserObjectBindings& data, const Ogre::String& name, Ogre::Real value
    )
  {
    const Ogre::Any& any = data.getUserAny( name );
    return( any.has_value( )? Ogre::any_cast< Ogre::Real >( any ): value );
  }

  Ogre::Quaternion orbit_frame( double node, double inclination, double perihelion )
  {
    using D = Ogre::Degree;
    return(
      Ogre::Quaternion( D( Ogre::Real( node ) ), Ogre::Vector3::UNIT_Y )
      *
      Ogre::Quaternion( D( Ogre::Real( inclination ) ), Ogre::Vector3::UNIT_X )
      *
      Ogre::Quaternion( D( Ogre::Real( perihelion ) ), Ogre::Vector3::UNIT_Y )
      );
  }
} // end namespace

// -------------------------------------------------------------------------
Application::
Application( int& ac, char** av )
  : TContext( fs::path( av[ 0 ] ).filename( ).stem( ).string( ) ),
    TListener( )
{
  this->m_ExecutionDir
    = fs::canonical( fs::path( av[ 0 ] ).parent_path( ) ).string( );
  this->m_SceneFile = av[ 1 ];
}

// -------------------------------------------------------------------------
Application::
~Application( )
{
}

// -------------------------------------------------------------------------
void Application::
loadResources( )
{
  this->enableShaderCache( );
  auto rgm = Ogre::ResourceGroupManager::getSingletonPtr( );
  try
  {
    rgm->initialiseAllResourceGroups( );
    rgm->loadResourceGroup( Ogre::RGN_DEFAULT );
  }
  catch( ... )
  {
    // Do nothing
  }
}

// -------------------------------------------------------------------------
void Application::
locateResources( )
{
  // Required by Ogre itself
  const Ogre::String ogreMedia = this->getDefaultMediaDir( );
  auto rgm = Ogre::ResourceGroupManager::getSingletonPtr( );
  rgm->addResourceLocation(
    ogreMedia + "/Main", "FileSystem", Ogre::RGN_INTERNAL
    );
  rgm->addResourceLocation(
    ogreMedia + "/RTShaderLib", "FileSystem", Ogre::RGN_INTERNAL
    );
  rgm->addResourceLocation(
    ogreMedia + "/packs/SdkTrays.zip", "Zip", "Essential"
    );

  std::string media_dir
    =
    fs::canonical( fs::path( this->m_SceneFile ).parent_path( ) ).string( );
  rgm->addResourceLocation( media_dir, "FileSystem", Ogre::RGN_DEFAULT );
}

// -------------------------------------------------------------------------
void Application::
setup( )
{
  this->_loadExtraPlugins( );
  this->_initSceneManager( );
  this->_loadScene( );
  this->_configureCamera( );
}

// -------------------------------------------------------------------------
void Application::
go( )
{
  this->initApp( );
  this->getRoot( )->startRendering( );
  delete this->m_Trays;
  this->m_Trays = nullptr;
  this->closeApp( );
}

// -------------------------------------------------------------------------
bool Application::
keyPressed( const OgreBites::KeyboardEvent& evt )
{
  if( evt.keysym.sym == OgreBites::SDLK_ESCAPE )
  {
    this->getRoot( )->queueEndRendering( );
    return( true );
  }
  else if(
    !this->m_Focus.empty( ) &&
    ( evt.keysym.sym == OgreBites::SDLK_RIGHT ||
      evt.keysym.sym == OgreBites::SDLK_DOWN ||
      evt.keysym.sym == OgreBites::SDLK_LEFT ||
      evt.keysym.sym == OgreBites::SDLK_UP )
    )
  {
    const bool next
      =
      evt.keysym.sym == OgreBites::SDLK_RIGHT ||
      evt.keysym.sym == OgreBites::SDLK_DOWN;
    const std::size_t n = this->m_Focus.size( );
    this->_setFocus( ( this->m_FocusIndex + ( next? 1: n - 1 ) ) % n );
    return( true );
  }
  else if( evt.keysym.sym == 'e' || evt.keysym.sym == 's' )
  {
    this->_jumpToEclipse( evt.keysym.sym == 's' );
    return( true );
  }
  else if( evt.keysym.sym == OgreBites::SDLK_SPACE )
  {
    this->m_Paused = !this->m_Paused;
    this->_updateTimeLabel( );
    return( true );
  }
  else if(
    evt.keysym.sym == '+' || evt.keysym.sym == '=' ||
    evt.keysym.sym == OgreBites::SDLK_KP_PLUS
    )
  {
    this->m_TimeScale = std::min( this->m_TimeScale * 2, 16384.0 );
    this->_updateTimeLabel( );
    return( true );
  }
  else if( evt.keysym.sym == '-' || evt.keysym.sym == OgreBites::SDLK_KP_MINUS )
  {
    this->m_TimeScale = std::max( this->m_TimeScale / 2, 0.0625 );
    this->_updateTimeLabel( );
    return( true );
  }
  else
    return( false ); // let the camera handle everything else
}

// -------------------------------------------------------------------------
void Application::
frameRendered( const Ogre::FrameEvent& evt )
{
  this->_advance(
    this->m_Paused? 0: double( evt.timeSinceLastFrame ) * this->m_TimeScale
    );

  if( this->m_CameraMan == nullptr )
    return;
  auto* camera = this->m_CameraMan->getCamera( );

  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  scnMgr->getRootSceneNode( )->_update( true, false );
  this->_updateBelt( );
  this->_updateCometTail( );
  this->_updateShadows( );

  if( !this->m_Focus.empty( ) )
  {
    auto [ node, distance ] = this->m_Focus[ this->m_FocusIndex ];
    const Ogre::Vector3 position = node->_getDerivedPosition( );

    if( this->m_FlightTime < FLIGHT_DURATION )
    {
      // Fly to the selected node, along an arc that avoids the bodies in between
      this->m_FlightTime += evt.timeSinceLastFrame;
      const Ogre::Real t
        =
        std::min< Ogre::Real >( this->m_FlightTime / FLIGHT_DURATION, 1 );
      const Ogre::Real s = t * t * ( 3 - ( 2 * t ) );

      const Ogre::Quaternion& q = camera->getOrientation( );
      const Ogre::Vector3 end = position + ( q * Ogre::Vector3::UNIT_Z * distance );
      const Ogre::Vector3 path = end - this->m_FlightStart;
      const Ogre::Real arc
        =
        Ogre::Math::Sin( Ogre::Math::PI * s ) * 0.25f * path.length( );
      camera->setPosition(
        this->m_FlightStart + ( path * s ) + ( q * Ogre::Vector3::UNIT_Y * arc )
        );
    }
    else
    {
      // The orbit camera does not follow a moving target: carry it along
      camera->translate( position - this->m_FocusPosition, Ogre::Node::TS_WORLD );
    } // end if
    this->m_FocusPosition = position;
  } // end if

  for( auto* node: this->m_Background )
    node->setPosition( camera->getPosition( ) );
}

// -------------------------------------------------------------------------
void Application::
_loadExtraPlugins( )
{
  auto* root = this->getRoot( );
  std::istringstream in( pujOgre_PLUGINS );
  std::string plugin;
  while( std::getline( in, plugin, '|' ) )
  {
    std::string plg_name
      = std::filesystem::path( this->m_ExecutionDir ) / plugin;
    plg_name += pujOgre_SHARED_EXT;
    root->loadPlugin( plg_name );
  } // end while
}

// -------------------------------------------------------------------------
void Application::
_initSceneManager( )
{
  this->TContext::setup( );
  this->addInputListener( this );

  auto* root = this->getRoot( );
  auto mgr = root->createSceneManager( "DefaultSceneManager", "scnMgr" );
  mgr->setAmbientLight( Ogre::ColourValue( 0.9, 0.9, 0.9 ) );

  // register our scene with the RTSS
  auto* shadergen = Ogre::RTShader::ShaderGenerator::getSingletonPtr( );
  shadergen->addSceneManager( mgr );

  // Required to draw the on-screen menu
  mgr->addRenderQueueListener( this->getOverlaySystem( ) );
}

// -------------------------------------------------------------------------
void Application::
_configureCamera( )
{
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  const auto& cameras = scnMgr->getCameras( );
  Ogre::Camera* cam = cameras.empty( )? nullptr: cameras.begin( )->second;

  // If no camera, create one
  if( cam == nullptr )
  {
    Ogre::SceneNode* root = scnMgr->getRootSceneNode( );
    root->_update( true, false );
    Ogre::AxisAlignedBox box;
    Self::_bounds( root, &box );

    Ogre::Vector3 corner = box.getMaximum( );
    Ogre::Vector3 target = box.getCenter( );
    Ogre::Real size = std::max< Ogre::Real >( box.getSize( ).length( ), 1e-3f );
    Ogre::Vector3 dir = corner - target;
    if( dir.squaredLength( ) < 1e-12f )
      dir = Ogre::Vector3( 1, 1, 1 );
    Ogre::Vector3 eye = target + ( dir * 2.f );

    cam = scnMgr->createCamera( "MainCamera" );
    // The smallest selectable bodies need a close near plane
    cam->setNearClipDistance(
      this->m_Focus.empty( )? size * 0.001f: std::min( size * 0.001f, 0.05f )
      );
    cam->setFarClipDistance( ( eye - target ).length( ) + ( size * 2 ) );
    cam->setFOVy( Ogre::Degree( 45 ) );

    Ogre::SceneNode* camNode
      =
      scnMgr->getRootSceneNode( )->createChildSceneNode( "MainCameraNode" );
    camNode->setFixedYawAxis( true );
    camNode->setPosition( eye );
    camNode->lookAt( target, Ogre::Node::TS_WORLD );
    camNode->attachObject( cam );
  } // end if

  cam->setAutoAspectRatio( true );

  auto* camman = new OgreBites::CameraMan( cam->getParentSceneNode( ) );
  camman->setStyle( OgreBites::CS_ORBIT );
  this->m_CameraMan = camman;
  if( !this->m_Focus.empty( ) )
  {
    camman->setTarget( this->m_Focus[ 0 ].first );
    camman->setYawPitchDist(
      Ogre::Degree( 0 ), Ogre::Degree( 20 ), this->m_Focus[ 0 ].second
      );
    this->m_FocusPosition = this->m_Focus[ 0 ].first->_getDerivedPosition( );
    this->_createFocusMenu( );
  } // end if
  this->addInputListener( camman );
  Ogre::Viewport* vp = this->getRenderWindow( )->addViewport( cam );
  vp->setBackgroundColour( Ogre::ColourValue( 0.5f, 0.5f, 0.52f ) );
}

// -------------------------------------------------------------------------
void Application::
_loadScene( )
{
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  auto* root = scnMgr->getRootSceneNode( );
  root->loadChildren( this->m_SceneFile );
  this->_readNodeProperties( root );
  this->_advance( 0 );

  // Bodies and materials with special shadows, if the scene has them
  auto node = [ scnMgr ]( const Ogre::String& name ) -> Ogre::SceneNode*
    {
      return( scnMgr->hasSceneNode( name )? scnMgr->getSceneNode( name ): nullptr );
    };
  auto material = []( const Ogre::String& name )
    {
      return( Ogre::MaterialManager::getSingleton( ).getByName( name ) );
    };
  this->m_Sun = node( "Sun" );
  this->m_Earth = node( "Earth" );
  this->m_EarthBody = node( "EarthBody" );
  this->m_Moon = node( "Moon" );
  this->m_Saturn = node( "Saturn" );
  this->m_SaturnBody = node( "SaturnBody" );
  this->m_MoonMaterial = material( "MoonTextured" );
  this->m_EarthMaterial = material( "EarthTextured" );
  this->m_SaturnMaterial = material( "SaturnTextured" );
  this->m_RingMaterial = material( "SaturnRingTextured" );

  /* TODO
     <userData>
       <property name="tag" type="float" data="[value]"/>
       ...
     </userData>
  */

  /* TODO
     const auto& ud = [node/entity]->getUserObjectBindings();
     const Ogre::Any& [value] = ud.getUserAny( "[tag]" );
     if( [value].has_value( ) )
       Ogre::any_cast< Ogre::Real >( [value] );

     bool [switch] = Ogre::any_cast< bool >( ud.getUserAny( "[tag]" ) );
     Ogre::String [str] = Ogre::any_cast< Ogre::String >(ud.getUserAny( "[tag]" ));
   */
}

// -------------------------------------------------------------------------
void Application::
_readNodeProperties( Ogre::SceneNode* node )
{
  const auto& data = node->getUserObjectBindings( );

  const Ogre::Any& semiMajor = data.getUserAny( "orbitSemiMajor" );
  if( semiMajor.has_value( ) )
  {
    Orbit orbit;
    orbit.SemiMajor = Ogre::any_cast< Ogre::Real >( semiMajor );
    orbit.Eccentricity = real_property( data, "orbitEccentricity", 0 );
    orbit.Period = real_property( data, "orbitPeriod", 1 );
    orbit.Anomaly = real_property( data, "orbitAnomaly", 0 );
    orbit.Frame
      =
      orbit_frame(
        real_property( data, "orbitNode", 0 ),
        real_property( data, "orbitInclination", 0 ),
        real_property( data, "orbitPerihelion", 0 )
        );
    this->m_Orbiting.emplace_back( node, orbit );
    this->_createOrbitLine( node, orbit );
  } // end if

  const Ogre::Any& spin = data.getUserAny( "spinSpeed" );
  if( spin.has_value( ) )
    this->m_Rotating.push_back(
      { node, Ogre::any_cast< Ogre::Real >( spin ), node->getOrientation( ) }
      );

  if( data.getUserAny( "followCamera" ).has_value( ) )
  {
    this->m_Background.push_back( node );
    for( Ogre::MovableObject* obj: node->getAttachedObjects( ) )
      obj->setRenderQueueGroup( Ogre::RENDER_QUEUE_SKIES_EARLY );
  } // end if

  const Ogre::Any& glow = data.getUserAny( "glowSize" );
  if( glow.has_value( ) )
    this->_createGlow( node, Ogre::any_cast< Ogre::Real >( glow ) );

  const Ogre::Any& belt = data.getUserAny( "beltCount" );
  if( belt.has_value( ) )
    this->_createBelt(
      node, Ogre::any_cast< int >( belt ),
      real_property( data, "beltInner", 1 ), real_property( data, "beltOuter", 2 )
      );

  if( data.getUserAny( "cometTail" ).has_value( ) )
    this->_createCometTail( node );

  const Ogre::Any& distance = data.getUserAny( "cameraDistance" );
  if( distance.has_value( ) )
  {
    this->m_Focus.emplace_back( node, Ogre::any_cast< Ogre::Real >( distance ) );

    const Ogre::Any& label = data.getUserAny( "label" );
    this->m_FocusNames.push_back(
      label.has_value( )
      ? Ogre::any_cast< Ogre::String >( label )
      : node->getName( )
      );

    const Ogre::Any& info = data.getUserAny( "info" );
    this->m_FocusInfo.push_back(
      info.has_value( )? Ogre::any_cast< Ogre::String >( info ): ""
      );
  } // end if

  for( Ogre::Node* child: node->getChildren( ) )
    this->_readNodeProperties( static_cast< Ogre::SceneNode* >( child ) );
}

// -------------------------------------------------------------------------
void Application::
_setFocus( std::size_t index )
{
  auto [ node, distance ] = this->m_Focus[ index ];
  this->m_FocusIndex = index;
  this->m_FocusPosition = node->_getDerivedPosition( );
  this->m_CameraMan->setTarget( node );

  // frameRendered( ) moves the camera; the viewing direction does not change
  this->m_FlightStart = this->m_CameraMan->getCamera( )->getPosition( );
  this->m_FlightTime = 0;
  this->_updateFocusMenu( );
}

// -------------------------------------------------------------------------
Ogre::Vector3 Application::
_orbitPosition( const Orbit& orbit, double days )
{
  // Kepler's equation: M = E - e * sin( E )
  const double two_pi = 2.0 * Ogre::Math::PI;
  const double e = orbit.Eccentricity;
  const double M
    =
    std::fmod(
      ( orbit.Anomaly * Ogre::Math::PI / 180.0 ) + ( two_pi * days / orbit.Period ),
      two_pi
      );
  double E = ( e > 0.8 )? Ogre::Math::PI: M;
  for( int i = 0; i < 8; ++i )
    E -= ( E - ( e * std::sin( E ) ) - M ) / ( 1.0 - ( e * std::cos( E ) ) );

  const double a = orbit.SemiMajor;
  return(
    orbit.Frame
    *
    Ogre::Vector3(
      Ogre::Real( a * ( std::cos( E ) - e ) ),
      0,
      Ogre::Real( -a * std::sqrt( 1.0 - ( e * e ) ) * std::sin( E ) )
      )
    );
}

// -------------------------------------------------------------------------
void Application::
_advance( double days )
{
  this->m_Days += days;

  // Each angle is kept as a total in double precision: adding tiny rotations
  // to a float quaternion every frame would drift or stall.
  for( auto& r: this->m_Rotating )
  {
    r.Angle = std::fmod( r.Angle + ( r.Speed * days ), 360.0 );
    r.Node->setOrientation(
      r.Base
      *
      Ogre::Quaternion(
        Ogre::Degree( Ogre::Real( r.Angle ) ), Ogre::Vector3::UNIT_Y
        )
      );
  } // end for

  for( auto& [ node, orbit ]: this->m_Orbiting )
    node->setPosition( Self::_orbitPosition( orbit, this->m_Days ) );
}

// -------------------------------------------------------------------------
void Application::
_updateBelt( )
{
  for( auto& [ rock, orbit ]: this->m_Asteroids )
    rock->setPosition( Self::_orbitPosition( orbit, this->m_Days ) );
}

// -------------------------------------------------------------------------
void Application::
_updateCometTail( )
{
  if( this->m_CometTail == nullptr || this->m_Sun == nullptr )
    return;

  // The closer to the Sun, the longer the tail
  const Ogre::Vector3 away
    =
    this->m_Comet->_getDerivedPosition( ) - this->m_Sun->_getDerivedPosition( );
  const Ogre::Real distance = away.length( );
  const Ogre::Real length = std::clamp< Ogre::Real >( 900 / distance, 1, 40 );
  const Ogre::Vector3 direction = away / distance;

  const int n = this->m_CometTail->getNumBillboards( );
  for( int i = 0; i < n; ++i )
  {
    const Ogre::Real t = Ogre::Real( i ) / Ogre::Real( n );
    auto* puff = this->m_CometTail->getBillboard( i );
    puff->setPosition( direction * length * t );
    const Ogre::Real size = ( 0.3f + ( t * 0.12f * length ) );
    puff->setDimensions( size, size );
  } // end for
}

// -------------------------------------------------------------------------
Ogre::Real Application::
_lunarEclipseAmount( ) const
{
  // Seen from the Earth: angle between the Moon and the point opposite to the
  // Sun. Sizes and distances are compressed in the scene, but this angle is
  // not, so real angular radii give eclipses as frequent as the real ones.
  // In degrees, the shadow of the Earth at the Moon's distance is 1.25
  // (penumbra) and 0.72 (umbra), and the Moon is 0.26.
  static const Ogre::Real start = 1.25f + 0.26f;
  static const Ogre::Real full = 0.72f - 0.26f;

  const Ogre::Vector3 earth = this->m_Earth->_getDerivedPosition( );
  const Ogre::Vector3 moon = this->m_Moon->_getDerivedPosition( ) - earth;
  const Ogre::Vector3 opposite = earth - this->m_Sun->_getDerivedPosition( );
  const Ogre::Real angle = moon.angleBetween( opposite ).valueDegrees( );

  const Ogre::Real t
    =
    std::clamp< Ogre::Real >( ( start - angle ) / ( start - full ), 0, 1 );
  return( t * t * ( 3 - ( 2 * t ) ) );
}

// -------------------------------------------------------------------------
Ogre::Real Application::
_solarEclipseAngle( ) const
{
  const Ogre::Vector3 earth = this->m_Earth->_getDerivedPosition( );
  const Ogre::Vector3 moon = this->m_Moon->_getDerivedPosition( ) - earth;
  const Ogre::Vector3 sun = this->m_Sun->_getDerivedPosition( ) - earth;
  return( moon.angleBetween( sun ).valueDegrees( ) );
}

// -------------------------------------------------------------------------
void Application::
_setConstant(
  const Ogre::MaterialPtr& material,
  const Ogre::String& name, const Ogre::Vector4& value
  )
{
  material->load( );
  material->getTechnique( 0 )->getPass( 0 )->getFragmentProgramParameters( )
    ->setNamedConstant( name, value );
}

// -------------------------------------------------------------------------
void Application::
_updateShadows( )
{
  if(
    this->m_Sun != nullptr && this->m_Earth != nullptr &&
    this->m_EarthBody != nullptr && this->m_Moon != nullptr &&
    this->m_MoonMaterial && this->m_EarthMaterial
    )
  {
    // Lunar eclipse: the little sunlight that reaches the Moon is reddish
    const Ogre::Real amount = this->_lunarEclipseAmount( );
    const Ogre::Vector3 tint
      =
      Ogre::Vector3::UNIT_SCALE
      +
      ( ( Ogre::Vector3( 0.25f, 0.1f, 0.06f ) - Ogre::Vector3::UNIT_SCALE ) * amount );
    Self::_setConstant(
      this->m_MoonMaterial, "tint", Ogre::Vector4( tint.x, tint.y, tint.z, 1 )
      );

    // Solar eclipse: the Moon's shadow on the Earth. At its real distance, the
    // Moon would be 60 Earth radii away, and the penumbra has 0.55 of them.
    const Ogre::Vector3 earth = this->m_Earth->_getDerivedPosition( );
    const Ogre::Vector3 moon = this->m_Moon->_getDerivedPosition( ) - earth;
    const Ogre::Vector3 sun
      =
      ( this->m_Sun->_getDerivedPosition( ) - earth ).normalisedCopy( );
    const Ogre::Real radius = this->m_EarthBody->getScale( ).x;
    const Ogre::Real along = moon.dotProduct( sun );
    const Ogre::Vector3 offset
      =
      ( moon - ( sun * along ) ) * ( 60 * radius / moon.length( ) );
    const bool visible = along > 0 && offset.length( ) < 3 * radius;
    const Ogre::Vector3 spot = earth + offset;
    Self::_setConstant(
      this->m_EarthMaterial, "spot",
      Ogre::Vector4( spot.x, spot.y, spot.z, visible? 0.55f * radius: 0 )
      );
  } // end if

  if(
    this->m_Saturn != nullptr && this->m_SaturnBody != nullptr &&
    this->m_SaturnMaterial && this->m_RingMaterial
    )
  {
    // Shadow of the rings on Saturn, and of Saturn on the rings
    const Ogre::Vector3 center = this->m_Saturn->_getDerivedPosition( );
    const Ogre::Vector3 normal
      =
      this->m_SaturnBody->_getDerivedOrientation( ) * Ogre::Vector3::UNIT_Y;
    const Ogre::Real radius = this->m_SaturnBody->getScale( ).x;
    const Ogre::Vector4 position( center.x, center.y, center.z, radius );
    Self::_setConstant( this->m_SaturnMaterial, "ringCenter", position );
    Self::_setConstant(
      this->m_SaturnMaterial, "ringNormal",
      Ogre::Vector4( normal.x, normal.y, normal.z, 0 )
      );
    Self::_setConstant(
      this->m_SaturnMaterial, "ringRadii",
      Ogre::Vector4( 1.24f * radius, 2.27f * radius, 0, 0 )
      );
    Self::_setConstant( this->m_RingMaterial, "planet", position );
  } // end if
}

// -------------------------------------------------------------------------
void Application::
_jumpToEclipse( bool solar )
{
  if(
    this->m_Sun == nullptr || this->m_Earth == nullptr ||
    this->m_Moon == nullptr || this->m_CameraMan == nullptr
    )
    return;
  auto* root = this->getRoot( )->getSceneManager( "scnMgr" )->getRootSceneNode( );

  // Look for the next deep eclipse, for up to about 11 years
  bool clear = false;
  bool found = false;
  for( int i = 0; i < 200000 && !found; ++i )
  {
    this->_advance( 0.02 );
    root->_update( true, false );
    if( solar )
    {
      const Ogre::Real angle = this->_solarEclipseAngle( );
      clear = clear || angle > 2;
      found = clear && angle < 0.3f;
    }
    else
    {
      const Ogre::Real amount = this->_lunarEclipseAmount( );
      clear = clear || amount < 0.01f;
      found = clear && amount >= 0.9f;
    } // end if
  } // end for
  if( !found )
    return;

  // Start a little earlier, slow enough to watch it happen
  this->_advance( -0.15 );
  root->_update( true, false );
  this->m_TimeScale = 0.0625;
  this->m_Paused = false;
  this->_updateTimeLabel( );

  // Look at the sunlit side of the eclipsed body, from the side of the Sun
  this->m_CameraMan->getCamera( )->setDirection(
    this->m_Earth->_getDerivedPosition( ) - this->m_Sun->_getDerivedPosition( ),
    Ogre::Node::TS_WORLD
    );
  const Ogre::SceneNode* target = solar? this->m_Earth: this->m_Moon;
  for( std::size_t i = 0; i < this->m_Focus.size( ); ++i )
    if( this->m_Focus[ i ].first == target )
      this->_setFocus( i );
}

// -------------------------------------------------------------------------
void Application::
_createOrbitLine( Ogre::SceneNode* node, const Orbit& orbit )
{
  // One full revolution, sampled in time
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  auto* line = scnMgr->createManualObject( node->getName( ) + "Line" );
  line->begin( "OrbitLine", Ogre::RenderOperation::OT_LINE_STRIP );
  const int segments = 512;
  for( int i = 0; i <= segments; ++i )
  {
    line->position(
      Self::_orbitPosition( orbit, orbit.Period * i / double( segments ) )
      );
    line->colour( 0.38f, 0.42f, 0.55f, 0.6f );
  } // end for
  line->end( );
  line->setCastShadows( false );

  // On the parent: its position is the center of the orbit
  node->getParentSceneNode( )->attachObject( line );
}

// -------------------------------------------------------------------------
void Application::
_createGlow( Ogre::SceneNode* node, Ogre::Real size )
{
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  auto* glow = scnMgr->createBillboardSet( node->getName( ) + "Glow", 1 );
  glow->setMaterialName( "SunGlow" );
  glow->setDefaultDimensions( size, size );
  glow->createBillboard(
    Ogre::Vector3::ZERO, Ogre::ColourValue( 1.0f, 0.72f, 0.38f )
    );
  glow->setCastShadows( false );
  node->attachObject( glow );
}

// -------------------------------------------------------------------------
void Application::
_createBelt( Ogre::SceneNode* node, int count, double inner, double outer )
{
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  auto* rocks = scnMgr->createBillboardSet( node->getName( ) + "Rocks", count );
  rocks->setMaterialName( "AsteroidDot" );
  rocks->setDefaultDimensions( 1.6f, 1.6f );
  rocks->setBounds(
    Ogre::AxisAlignedBox( Ogre::AxisAlignedBox::EXTENT_INFINITE ), 1e6f
    );
  rocks->setCastShadows( false );
  node->attachObject( rocks );

  // Always the same belt; real periods come from the real distances, 2.2 to
  // 3.3 astronomical units, and Kepler's third law.
  std::mt19937 generator( 48 );
  std::uniform_real_distribution< double > uniform( 0.0, 1.0 );
  this->m_Asteroids.reserve( count );
  for( int i = 0; i < count; ++i )
  {
    const double u = uniform( generator );
    Orbit orbit;
    orbit.SemiMajor = inner + ( u * ( outer - inner ) );
    orbit.Eccentricity = 0.2 * uniform( generator );
    orbit.Period = 365.25 * std::pow( 2.2 + ( 1.1 * u ), 1.5 );
    orbit.Anomaly = 360.0 * uniform( generator );
    orbit.Frame
      =
      orbit_frame(
        360.0 * uniform( generator ),
        15.0 * uniform( generator ),
        360.0 * uniform( generator )
        );
    const Ogre::Real grey = 0.6f + ( 0.4f * Ogre::Real( uniform( generator ) ) );
    this->m_Asteroids.emplace_back(
      rocks->createBillboard(
        Ogre::Vector3::ZERO, Ogre::ColourValue( grey, grey * 0.95f, grey * 0.88f )
        ),
      orbit
      );
  } // end for
}

// -------------------------------------------------------------------------
void Application::
_createCometTail( Ogre::SceneNode* node )
{
  auto* scnMgr = this->getRoot( )->getSceneManager( "scnMgr" );
  const int n = 40;
  this->m_Comet = node;
  this->m_CometTail = scnMgr->createBillboardSet( node->getName( ) + "Tail", n );
  this->m_CometTail->setMaterialName( "CometPuff" );
  this->m_CometTail->setBounds(
    Ogre::AxisAlignedBox( Ogre::AxisAlignedBox::EXTENT_INFINITE ), 1e6f
    );
  this->m_CometTail->setCastShadows( false );
  for( int i = 0; i < n; ++i )
  {
    const Ogre::Real fade = 1 - ( Ogre::Real( i ) / Ogre::Real( n ) );
    this->m_CometTail->createBillboard(
      Ogre::Vector3::ZERO,
      Ogre::ColourValue( 0.55f * fade, 0.7f * fade, 0.95f * fade )
      );
  } // end for
  node->attachObject( this->m_CometTail );
}

// -------------------------------------------------------------------------
void Application::
_createFocusMenu( )
{
  this->m_Trays = new OgreBites::TrayManager( "FocusMenu", this->getRenderWindow( ) );
  this->m_Trays->hideCursor( );
  for( std::size_t i = 0; i < this->m_Focus.size( ); ++i )
    this->m_FocusLabels.push_back(
      this->m_Trays->createLabel(
        OgreBites::TL_TOPLEFT, "Focus" + std::to_string( i ),
        this->m_FocusNames[ i ], 160
        )
      );
  this->m_TimeLabel
    =
    this->m_Trays->createLabel( OgreBites::TL_BOTTOMLEFT, "Time", "", 160 );
  this->m_InfoPanel
    =
    this->m_Trays->createParamsPanel(
      OgreBites::TL_TOPRIGHT, "Info", 300,
      { "Nombre", "Radio", "Distancia", "Orbita", "Giro", "Eje" }
      );
  this->_updateFocusMenu( );
  this->_updateTimeLabel( );
}

// -------------------------------------------------------------------------
void Application::
_updateTimeLabel( )
{
  if( this->m_TimeLabel == nullptr )
    return;
  this->m_TimeLabel->setCaption(
    ( this->m_Paused? "Pausa: ": "" )
    + Ogre::StringConverter::toString( Ogre::Real( this->m_TimeScale ) )
    + " dias/s"
    );
}

// -------------------------------------------------------------------------
void Application::
_updateFocusMenu( )
{
  for( std::size_t i = 0; i < this->m_FocusLabels.size( ); ++i )
    this->m_FocusLabels[ i ]->setCaption(
      ( ( i == this->m_FocusIndex )? "> ": "" ) + this->m_FocusNames[ i ]
      );

  if( this->m_InfoPanel == nullptr )
    return;
  this->m_InfoPanel->setParamValue(
    Ogre::uint( 0 ), this->m_FocusNames[ this->m_FocusIndex ]
    );
  const auto values
    =
    Ogre::StringUtil::split( this->m_FocusInfo[ this->m_FocusIndex ], "|" );
  for( unsigned int i = 1; i < 6; ++i )
    this->m_InfoPanel->setParamValue(
      i, ( i - 1 < values.size( ) )? values[ i - 1 ]: "-"
      );
}

// -------------------------------------------------------------------------
void Application::
_bounds( Ogre::SceneNode* node, Ogre::AxisAlignedBox* box )
{
  // The background must not affect the camera clipping distances
  if( node->getUserObjectBindings( ).getUserAny( "followCamera" ).has_value( ) )
    return;

  for( Ogre::MovableObject* obj: node->getAttachedObjects( ) )
  {
    const Ogre::String& type = obj->getMovableType( );
    if(
      obj->isVisible( ) &&
      type != "Camera" && type != "Light" && type != "BillboardSet"
      )
      box->merge( obj->getWorldBoundingBox( true ) );
  } // end for

  for( Ogre::Node* child: node->getChildren( ) )
    Self::_bounds( static_cast< Ogre::SceneNode* >( child ), box );
}

// eof - Application.cxx
