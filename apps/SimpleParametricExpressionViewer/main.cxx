// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

#include "Application.h"
#include <iostream>

int main( int argc, char** argv )
{
  if( argc < 2 )
  {
    std::cerr << "Usage: " << argv[ 0 ] << " file.scenepp" << std::endl;
    return( EXIT_FAILURE );
  } // end if
  
  try
  {
    Application app( argc, argv );
    app.go( );
    return( EXIT_SUCCESS );
  }
  catch( const std::exception& e )
  {
    Ogre::LogManager* log = Ogre::LogManager::getSingletonPtr( );
    if( log != nullptr )
      log->logError( e.what( ) );
    std::cerr << "Error: " << e.what( ) << std::endl;
    return( EXIT_FAILURE );
  } // end try
}

// eof - main.cxx
