// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

// Ogre plugin that registers VTK file formats as mesh codecs (Ogre 13+),
// so that sceneMgr->createEntity("model.vtp") just works.
//
// Load with plugins.cfg:   Plugin=Plugin_VtkCodec
// or in code:              root->loadPlugin("Plugin_VtkCodec");

#include <pujOgre/vtkPolyData2OgreMesh.h>
#include <plugins/VTKCodec/Export.h>

#include <vector>

#include <OgrePlugin.h>
#include <OgreRoot.h>
#include <OgreCodec.h>
#include <OgreMesh.h>
#include <OgreDataStream.h>
#include <OgreException.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkPolyDataReader.h>

namespace
{
  /**
   */
  class Plugin_pujOgreVTKCodec_EXPORT pujOgreVTKCodec
    : public Ogre::Codec
  {
  public:
    explicit pujOgreVTKCodec( const Ogre::String& extension )
      : mType( extension )
      {
      }

    virtual Ogre::String getType( ) const override
      {
        return( this->mType );
      }

    virtual
    Ogre::String magicNumberToFileExt( const char*, std::size_t ) const
    override
      {
        return( "" );
      }

    virtual
    void decode(
      const Ogre::DataStreamPtr& input, const Ogre::Any& output
      ) const
    override
    {
      Ogre::Mesh* mesh = Ogre::any_cast< Ogre::Mesh* >( output );
      const std::string data = input->getAsString( );

      vtkSmartPointer< vtkPolyData > polyData;
      if( this->mType == "vtp" )
      {
        auto reader = vtkSmartPointer< vtkXMLPolyDataReader >::New( );
        reader->ReadFromInputStringOn( );
        reader->SetInputString( data );
        reader->Update( );
        polyData = reader->GetOutput( );
      }
      else
      {
        auto reader = vtkSmartPointer< vtkPolyDataReader >::New( );
        reader->ReadFromInputStringOn( );
        reader->SetBinaryInputString( data.data( ), int( data.size( ) ) );
        reader->Update( );
        polyData = reader->GetOutput( );
      } // end if

      if(
        polyData.GetPointer( ) == nullptr
        ||
        polyData->GetNumberOfPoints( ) == 0
        )
        OGRE_EXCEPT(
          Ogre::Exception::ERR_INVALIDPARAMS,
          "No polydata could be read from '" + input->getName( ) + "'",
          "pujOgreVTKCodec::decode"
          );

      // Material can be changed later with entity->setMaterialName(...)
      pujOgre::FillOgreMeshFromvtkPolyData(
        polyData, mesh, "BaseWhite",
        /*generateNormalsIfMissing=*/true, /*flipV=*/true
        );
    }

  private:
    Ogre::String mType;
  };

  /**
   */
  class pujOgreVTKPlugin
    : public Ogre::Plugin
  {
  public:
    virtual const Ogre::String& getName( ) const override
      {
        static const Ogre::String name = "pujOgre VTK PolyData Codec";
        return( name );
      }

    virtual void install( ) override
      {
        for( const char* ext: { "vtp", "vtk" } )
        {
          auto* codec = new pujOgreVTKCodec( ext );
          Ogre::Codec::registerCodec( codec );
          this->mCodecs.push_back( codec );
        } // end for
      }

    virtual void initialise( ) override { }
    virtual void shutdown  ( ) override { }

    virtual void uninstall( ) override
    {
      for( auto* codec: this->mCodecs )
      {
        Ogre::Codec::unregisterCodec( codec );
        delete codec;
      } // end for
      this->mCodecs.clear( );
    }

  private:
    std::vector< Ogre::Codec* > mCodecs;
  };

  pujOgreVTKPlugin* gPlugin = nullptr;

} // end namespace

// Entry points Ogre looks up when it loads the shared library.
extern "C" void Plugin_pujOgreVTKCodec_EXPORT dllStartPlugin( )
{
  gPlugin = new pujOgreVTKPlugin( );
  Ogre::Root::getSingleton( ).installPlugin( gPlugin );
}

extern "C" void Plugin_pujOgreVTKCodec_EXPORT dllStopPlugin( )
{
  Ogre::Root::getSingleton( ).uninstallPlugin( gPlugin );
  delete gPlugin;
  gPlugin = nullptr;
}

// eof - OgreVTKCodec.cxx
