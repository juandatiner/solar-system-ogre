// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================
#ifndef __SimpleParametricExpressionViewer__Application__h__
#define __SimpleParametricExpressionViewer__Application__h__

#include <string>
#include <utility>
#include <vector>
#include <OgreApplicationContext.h>
#include <OgreCameraMan.h>
#include <OgreTrays.h>

/**
 */
class Application
  : public OgreBites::ApplicationContext,
    public OgreBites::InputListener
{
public:
  using Self      = Application;
  using TContext  = OgreBites::ApplicationContext;
  using TListener = OgreBites::InputListener;

public:
  Application( int& ac, char** av );
  virtual ~Application( ) override;

  // Configuration
  virtual void loadResources( ) override;
  virtual void locateResources( ) override;
  virtual void setup( ) override;

  // Interactions
  virtual bool keyPressed( const OgreBites::KeyboardEvent& evt ) override;
  virtual void frameRendered( const Ogre::FrameEvent& evt ) override;

  // Main method
  void go( );

protected:
  // An elliptical orbit, around the parent of the moving node
  struct Orbit
  {
    double           SemiMajor;
    double           Eccentricity;
    double           Period;    // days
    double           Anomaly;   // mean anomaly at day zero, in degrees
    Ogre::Quaternion Frame;     // orientation of the orbital plane
  };

  void _loadExtraPlugins( );
  void _initSceneManager( );
  void _configureCamera( );
  void _loadScene( );
  void _readNodeProperties( Ogre::SceneNode* node );
  void _setFocus( std::size_t index );
  void _createFocusMenu( );
  void _updateFocusMenu( );
  void _updateTimeLabel( );
  void _createOrbitLine( Ogre::SceneNode* node, const Orbit& orbit );
  void _createGlow( Ogre::SceneNode* node, Ogre::Real size );
  void _createBelt( Ogre::SceneNode* node, int count, double inner, double outer );
  void _createCometTail( Ogre::SceneNode* node );
  void _advance( double days );
  void _updateBelt( );
  void _updateCometTail( );
  Ogre::Real _lunarEclipseAmount( ) const;
  Ogre::Real _solarEclipseAngle( ) const;
  void _updateShadows( );
  void _jumpToEclipse( bool solar );
  static Ogre::Vector3 _orbitPosition( const Orbit& orbit, double days );
  static void _setConstant(
    const Ogre::MaterialPtr& material,
    const Ogre::String& name, const Ogre::Vector4& value
    );
  static void _bounds( Ogre::SceneNode* node, Ogre::AxisAlignedBox* box );

protected:
  std::string m_ExecutionDir { "" };
  std::string m_SceneFile    { "" };

  // Simulated days since the start, and speed in days per second
  double m_Days      { 0 };
  double m_TimeScale { 16 };
  bool   m_Paused    { false };

  // Nodes with a "spinSpeed" property (degrees per day)
  struct Rotating
  {
    Ogre::SceneNode* Node;
    double           Speed;
    Ogre::Quaternion Base;      // orientation given by the scene file
    double           Angle { 0 };
  };
  std::vector< Rotating > m_Rotating;

  // Nodes with orbital elements
  std::vector< std::pair< Ogre::SceneNode*, Orbit > > m_Orbiting;

  // Nodes with a "followCamera" property: background centered on the camera
  std::vector< Ogre::SceneNode* > m_Background;

  // Asteroid belt: each rock follows its own orbit
  std::vector< std::pair< Ogre::Billboard*, Orbit > > m_Asteroids;

  // Comet tail: always points away from the Sun
  Ogre::SceneNode*    m_Comet     { nullptr };
  Ogre::BillboardSet* m_CometTail { nullptr };

  // Nodes with a "cameraDistance" property, in scene order: the camera orbits
  // around one of them, and the arrow keys switch between them
  std::vector< std::pair< Ogre::SceneNode*, Ogre::Real > > m_Focus;
  std::size_t            m_FocusIndex { 0 };
  Ogre::Vector3          m_FocusPosition { Ogre::Vector3::ZERO };
  OgreBites::CameraMan*  m_CameraMan { nullptr };

  // Camera flight towards a newly selected node
  static constexpr Ogre::Real FLIGHT_DURATION { 1.2f };
  Ogre::Real    m_FlightTime  { FLIGHT_DURATION };
  Ogre::Vector3 m_FlightStart { Ogre::Vector3::ZERO };

  // Bodies and materials involved in eclipses and ring shadows
  Ogre::SceneNode*  m_Sun        { nullptr };
  Ogre::SceneNode*  m_Earth      { nullptr };
  Ogre::SceneNode*  m_EarthBody  { nullptr };
  Ogre::SceneNode*  m_Moon       { nullptr };
  Ogre::SceneNode*  m_Saturn     { nullptr };
  Ogre::SceneNode*  m_SaturnBody { nullptr };
  Ogre::MaterialPtr m_MoonMaterial;
  Ogre::MaterialPtr m_EarthMaterial;
  Ogre::MaterialPtr m_SaturnMaterial;
  Ogre::MaterialPtr m_RingMaterial;

  // On-screen menu, simulation speed and data of the selected body
  OgreBites::TrayManager*          m_Trays { nullptr };
  std::vector< std::string >       m_FocusNames;
  std::vector< std::string >       m_FocusInfo;
  std::vector< OgreBites::Label* > m_FocusLabels;
  OgreBites::Label*                m_TimeLabel { nullptr };
  OgreBites::ParamsPanel*          m_InfoPanel { nullptr };
};

#endif // __SimpleParametricExpressionViewer__Application__h__

// eof - Application.h
