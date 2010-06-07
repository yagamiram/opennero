//--------------------------------------------------------
// OpenNero : SceneObject
//  Responsible for maintaining object presence on-screen
//--------------------------------------------------------

#include "core/Common.h"
#include "core/Preprocessor.h"
#include "render/SceneObject.h"
#include "render/Shader.h"
#include "render/LineSet.h"
#include "render/Camera.h"
#include "game/factories/SimFactory.h"
#include "game/factories/IrrFactory.h"
#include "game/SimEntityData.h"
#include "utils/Performance.h"
#include "game/Kernel.h"
#include "game/SimContext.h"
#include "math/Random.h"
#include "game/Simulation.h"

#include <sstream>
#include <list>

// strip out stat code in final builds
#define SCENEOBJECT_ENABLE_STATS !NERO_FINAL_RELEASE

namespace OpenNero
{
    namespace
    {
        ISceneManager_IPtr GetSceneManager()
        {
            return Kernel::GetSimContext()->GetSceneManager();
        }

        /// Converter function object that goes from std::string to IrrMaterialType
        class IrrMaterialTypeConverter
        {
        private:

            SimFactoryPtr mFactory; /// The factory used to load shaders

        public:

            /// constructor that saves the SimFactoryPtr
            IrrMaterialTypeConverter( SimFactoryPtr fac ) : mFactory(fac) {}

            /**
             * Converter function that transforms a string into a material type. If the
             * string is a key irrlicht shader type that it recognizes, it will assign that
             * material type, otherwise it will try to load the change at the typeString path.
             * @param outType the output material type variable
             * @param typeString the material type string
             * @return true if we got a valid material
            */
            bool operator() ( IrrMaterialType& outType, const std::string& typeString )
            {
                // default value
                outType = EMT_SOLID;

                // convert this string to lowercase
                std::string lowString = typeString;
                std::transform( lowString.begin(), lowString.end(), lowString.begin(), ::tolower );

                if(      lowString == "solid" )                     outType = EMT_SOLID;
                else if( lowString == "lightmap" )                  outType = EMT_LIGHTMAP;
                else if( lowString == "lightmapadd" )               outType = EMT_LIGHTMAP_ADD;
                else if( lowString == "lightmap_mod2" )             outType = EMT_LIGHTMAP_M2;
                else if( lowString == "lightmap_mod4" )             outType = EMT_LIGHTMAP_M4;
                else if( lowString == "lighting" )                  outType = EMT_LIGHTMAP_LIGHTING;
                else if( lowString == "lighting_mod2" )             outType = EMT_LIGHTMAP_LIGHTING_M2;
                else if( lowString == "lighting_mod4" )             outType = EMT_LIGHTMAP_LIGHTING_M4;
                else if( lowString == "detail" )                    outType = EMT_DETAIL_MAP;
                else if( lowString == "spheremap" )                 outType = EMT_SPHERE_MAP;
                else if( lowString == "reflection2layer" )          outType = EMT_REFLECTION_2_LAYER;
                else if( lowString == "transparentaddcolor" )       outType = EMT_TRANSPARENT_ADD_COLOR;
                else if( lowString == "transparent_alpha" )         outType = EMT_TRANSPARENT_ALPHA_CHANNEL;
                else if( lowString == "transparent_cutoff" )        outType = EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
                else if( lowString == "transparent_vertex" )        outType = EMT_TRANSPARENT_VERTEX_ALPHA;
                else if( lowString == "transparent_refl_2layer" )   outType = EMT_TRANSPARENT_REFLECTION_2_LAYER;
                else if( lowString == "normalmap" )                 outType = EMT_NORMAL_MAP_SOLID;
                else if( lowString == "parallaxmap" )               outType = EMT_PARALLAX_MAP_SOLID;

            // try to load a shader
            else
            {
                IrrFactory& irrFac = mFactory->getIrrFactory();

                if( int32_t shader = irrFac.LoadGlslShader( typeString + ".vert",
                                                            typeString + ".frag" ) )
                {
                    outType = (IrrMaterialType)shader;
                }
            }

                return true;
            }
        };

        /// converter function object that goes from std::string to an IrrMaterialFlag
        class IrrMaterialFlagConverter
        {
            private:

            std::string mFlagName; /// The name of the flag to search for

            public:

            /// constructor which saves the flag name
            IrrMaterialFlagConverter( const std::string& flagName ) : mFlagName(flagName)
            {
                std::transform( mFlagName.begin(), mFlagName.end(), mFlagName.begin(), ::toupper );
            }

            /**
             * Converter operator which goes string -> IrrMaterialFlag.
             *  Checks if the flagname we constructed this with matches any of the
             *  recognized types. If it does, set it to the boolean version of inVal,
             *  otherwise return false
             * @param outVal the output variable
             * @param inVal the input string to convert
             * @return true if we have a valid conversion, false otherwise
             */
            bool operator() ( IrrMaterialFlag& outVal, const std::string& inVal )
            {
                std::string val = inVal;
                std::transform( val.begin(), val.end(), val.begin(), ::tolower );
                bool bVal = val == "true" || val == "1";

                // EMF_WIREFRAME   Draw as wireframe or filled triangles? Default: false.
                if ( mFlagName == "WIREFRAME" ) outVal = IrrMaterialFlag( EMF_WIREFRAME, bVal );
                // EMF_POINTCLOUD  Draw as point cloud or filled triangles? Default: false.
                else if ( mFlagName == "POINTCLOUD" ) outVal = IrrMaterialFlag( EMF_POINTCLOUD, bVal );
                // EMF_GOURAUD_SHADING     Flat or Gouraud shading? Default: true.
                else if ( mFlagName == "GOURAUD_SHADING" ) outVal = IrrMaterialFlag( EMF_GOURAUD_SHADING, bVal );
                // EMF_LIGHTING    Will this material be lighted? Default: true.
                else if ( mFlagName == "LIGHTING" ) outVal = IrrMaterialFlag( EMF_LIGHTING, bVal );
                // EMF_ZBUFFER     Is the ZBuffer enabled? Default: true.
                else if ( mFlagName == "ZBUFFER" ) outVal = IrrMaterialFlag( EMF_ZBUFFER, bVal );
                // EMF_ZWRITE_ENABLE   May be written to the zbuffer or is it readonly. Default: true This flag is ignored, if the material type is a transparent type.
                else if ( mFlagName == "ZWRITE_ENABLE" ) outVal = IrrMaterialFlag( EMF_ZWRITE_ENABLE, bVal );
                // EMF_BACK_FACE_CULLING   Is backfaceculling enabled? Default: true.
                else if ( mFlagName == "BACK_FACE_CULLING" ) outVal = IrrMaterialFlag( EMF_BACK_FACE_CULLING, bVal );
                // EMF_BILINEAR_FILTER     Is bilinear filtering enabled? Default: true.
                else if ( mFlagName == "BILINEAR_FILTER" ) outVal = IrrMaterialFlag( EMF_BILINEAR_FILTER, bVal );
                // EMF_TRILINEAR_FILTER    Is trilinear filtering enabled? Default: false If the trilinear filter flag is enabled, the bilinear filtering flag is ignored.
                else if ( mFlagName == "TRILINEAR_FILTER" ) outVal = IrrMaterialFlag( EMF_TRILINEAR_FILTER, bVal );
                // EMF_ANISOTROPIC_FILTER  Is anisotropic filtering? Default: false In Irrlicht you can use anisotropic texture filtering in conjunction with bilinear or trilinear texture filtering to improve rendering results. Primitives will look less blurry with this flag switched on.
                else if ( mFlagName == "ANISOTROPIC_FILTER" ) outVal = IrrMaterialFlag( EMF_ANISOTROPIC_FILTER, bVal );
                // EMF_FOG_ENABLE  Is fog enabled? Default: false.
                else if ( mFlagName == "FOG_ENABLE" ) outVal = IrrMaterialFlag( EMF_FOG_ENABLE, bVal );
                // EMF_NORMALIZE_NORMALS   Normalizes normals.You can enable this if you need to scale a dynamic lighted model. Usually, its normals will get scaled too then and it will get darker. If you enable the EMF_NORMALIZE_NORMALS flag, the normals will be normalized again, and the model will look as bright as it should.
                else if ( mFlagName == "NORMALIZE_NORMALS" ) outVal = IrrMaterialFlag( EMF_NORMALIZE_NORMALS, bVal );
                // EMF_TEXTURE_WRAP    Access to all layers texture wrap settings. Overwrites separate layer settings.
                else if ( mFlagName == "TEXTURE_WRAP" ) outVal = IrrMaterialFlag( EMF_TEXTURE_WRAP, bVal );
                else outVal = IrrMaterialFlag();

                return true;
            }
        };
    }

    SimId ConvertSceneIdToSimId(uint32_t id)
    {
        return static_cast<SimId>(id >> SceneObject::BITMASK_SIZE);
    }

    uint32_t ConvertSimIdToSceneId(SimId sceneId, uint32_t type)
    {
        Assert(type < (1 << (SceneObject::BITMASK_SIZE + 1)));
        return static_cast<uint32_t>( (sceneId << SceneObject::BITMASK_SIZE) | type );
    }

    /// a template for footprints
	struct FootprintTemplate
	{
        /// number of frames
		U32 frames;
        /// footprint trail
		U32 trail;
        /// a string representing an object
		std::string object;
        /// counter variable
		U32 counter;
        /// constructor
		FootprintTemplate(U32 f, U32 t, const std::string& o) :
		frames(f), trail(t), object(o), counter(0) {}
        /// perform a step
		bool DoStep() { return counter++ % frames == 0; }
        /// get the offset
		Vector3f GetOffset();
	};

	/// a template for a First Person camera
	struct FPSCameraTemplate
	{
		Vector3f attach_point; ///< where to attach the camera (relative to the body center of mass)
		Vector3f target;       ///< where the camera is looking
		F32 near_plane;        ///< near plane of the camera
		F32 far_plane;         ///< far plane of the camera
        Vector3f lastRotation; ///< last rotation
        Vector3f lastPosition; ///< last position

		/// construct the template from a property map
		FPSCameraTemplate(const std::string& prefix, const PropertyMap& propMap)
			: attach_point()
			, target(100,0,0)
			, near_plane(10)
			, far_plane(1000)
		{
			propMap.getValue(attach_point, prefix + ".attach_point");
			propMap.getValue(target, prefix+ ".target");
			propMap.getValue(near_plane, prefix + ".near_plane");
			propMap.getValue(far_plane, prefix + ".far_plane");
		}

		/// Attach a camera to a sim entity - this has the effect of the camera
		/// moving (and rotating) with the object.
		/// @param cam camera to attach
		/// @param sim SimEntity to attach to
        void AttachCamera(CameraPtr cam, SimEntityDataPtr sim)
        {
            lastPosition = sim->GetPosition();
            lastRotation = sim->GetRotation();
            cam->setPosition( attach_point );
            cam->setTarget( target );
            cam->setNearPlane( near_plane );
            cam->setFarPlane( far_plane );
            LOG_F_MSG("render", "attached camera " << cam);
        }
        
        /// Update rotation of camera
        /// @param cam camera
        /// @param rotation amount camera is rotated
        void UpdateRotation(const SimEntityData* sim, CameraPtr cam)
        {
            Vector3f rotor(sim->GetRotation() - lastRotation);
            Vector3f pos(sim->GetPosition());
            Vector3f t(cam->getTarget());
            t.rotateXYBy(rotor.Z, pos);
            cam->setTarget(t);
            lastRotation = sim->GetRotation();
        }
        
        void UpdatePosition(const SimEntityData* sim, CameraPtr cam)
        {
            Vector3f displacement(sim->GetPosition() - lastPosition);
            Vector3f target(cam->getTarget());
            target = target + displacement;
            cam->setTarget(target);
            lastPosition = sim->GetPosition();
        }
	};

	std::ostream& operator<<(std::ostream& out, FPSCameraTemplatePtr a)
	{
		if (a)
		{
			out << "<FPSCamera attach_point=\"" << a->attach_point
				<< "\" target=\"" << a->target
				<< "\" near_plane=\"" << a->near_plane
				<< "\" far_plane=\"" << a->far_plane << "\"/>";
		}
		else
		{
			out << NULL;
		}
		return out;
	}

    /// copy ctor for scene object template
    SceneObjectTemplate::SceneObjectTemplate( const SceneObjectTemplate& objTempl )
                                    : ObjectTemplate(objTempl)
    {
        mAniMesh            = objTempl.mAniMesh;
        mTextures           = objTempl.mTextures;
        mHeightmap          = objTempl.mHeightmap;
        mMaterialType       = objTempl.mMaterialType;
        mScale              = objTempl.mScale;
        mScaleTexture       = objTempl.mScaleTexture;
        mParticleSystem     = objTempl.mParticleSystem;
        mCastsShadow        = objTempl.mCastsShadow;
        mDrawBoundingBox    = objTempl.mDrawBoundingBox;
        mDrawLabel          = objTempl.mDrawLabel;
        mFPSCamera          = objTempl.mFPSCamera;
        mAnimationSpeed     = objTempl.mAnimationSpeed;
        mFootprints         = objTempl.mFootprints;

        // copy over the textures and the material flags
        std::copy( objTempl.mTextures.begin(), objTempl.mTextures.end(), mTextures.begin() );
        std::copy( objTempl.mMaterialFlags.begin(), objTempl.mMaterialFlags.end(), mMaterialFlags.begin() );
    }

    /**
     * Factory method to create the appropriate type of SceneObjectTemplate depending on the XML data provided
     * @param factory SimFactory used to grab resources
     * @param propMap property map that contains the information from the XML file
     * @return an instance of an SceneObjectTemplate child suitable for generating new Scene objects
     */
    SceneObjectTemplatePtr SceneObjectTemplate::createTemplate(SimFactoryPtr factory, const PropertyMap& propMap)
    {
        // we only have one type of SceneObjectTemplate for now, so return that type unconditionally
        SceneObjectTemplatePtr p(new SceneObjectTemplate(factory, propMap));
        return p;
    }

    /// constructor
    SceneObjectTemplate::SceneObjectTemplate( SimFactoryPtr factory, const PropertyMap& propMap ) :
		ObjectTemplate( factory, propMap ),
		mScaleTexture(1,1),
		mTextures(),
		mMaterialFlags(),
		mMaterialType( EMT_SOLID ),
		mHeightmap(),
		mParticleSystem(),
		mAniMesh(NULL),
        mCastsShadow(false),
        mDrawBoundingBox(false),
        mDrawLabel(false),
        mFPSCamera(),
        mAnimationSpeed(25.0f),
        mFootprints()
    {
        AssertMsg( factory, "Invalid sim factory" );

        std::string val, aniMeshFile;
        IrrFactory& irrFac = factory->getIrrFactory();

        // get the mesh (possibly)
        if( propMap.getValue( aniMeshFile, "Template.Render.AniMesh" ) )
        {
            mAniMesh = irrFac.LoadAniMesh( aniMeshFile.c_str() );
            SafeIrrGrab(mAniMesh);
        }

        if( propMap.hasSection( "Template.Render.CastsShadow" ) )
        {
            propMap.getValue( mCastsShadow, "Template.Render.CastsShadow" );
        }

        if( propMap.hasSection( "Template.Render.DrawBoundingBox" ) )
        {
            propMap.getValue( mDrawBoundingBox, "Template.Render.DrawBoundingBox" );
        }

        if( propMap.hasSection( "Template.Render.DrawLabel" ) )
        {
            propMap.getValue( mDrawLabel, "Template.Render.DrawLabel" );
        }

        if (propMap.hasSection( "Template.Render.FPSCamera" ) )
        {
			mFPSCamera.reset(new FPSCameraTemplate("Template.Render.FPSCamera", propMap));
            LOG_F_DEBUG( "render", "object uses an FPS camera " << mFPSCamera );
        }

        if (propMap.hasSection( "Template.Render.AnimationSpeed" ) )
        {
            propMap.getValue( mAnimationSpeed, "Template.Render.AnimationSpeed" );
            LOG_F_DEBUG( "render", "object animation speed: " << mAnimationSpeed );
        }

        static const std::string kFootprints("Template.Render.Footprints");
        if (propMap.hasSection( kFootprints ) )
        {
            U32 frames = 0;
            U32 trail = 0;
            std::string object = "";
            propMap.getValue(frames, kFootprints + ".Frames");
            propMap.getValue(object, kFootprints + ".Object");
            propMap.getValue(trail, kFootprints + ".Trail");
            mFootprints.reset(new FootprintTemplate(frames,trail,object));
        }

        // get the heightmap (possibly)
        propMap.getValue( mHeightmap, "Template.Render.Terrain" );

        // get the particle system (possibly)
        propMap.getValue( mParticleSystem, "Template.Render.ParticleSystem" );

        PropertyMap::ChildPropVector renderProps;
        propMap.getPropChildren( renderProps, "Template.Render" );

        PropertyMap::ChildPropVector::const_iterator itr = renderProps.begin();
        PropertyMap::ChildPropVector::const_iterator end = renderProps.end();

        // get all the properties of Render
        for( ; itr != end; ++itr )
        {
            // if it starts with the word "Texture" in it (ex: Texture0, Texture1, ...)
            if( itr->first.find("Texture") == 0 )
            {
                ITexture* tex = irrFac.LoadTexture( itr->second );
                if( tex )
                {
                    mTextures.push_back( ITexture_IPtr(tex) );
                }
            }

            // if it contains the word "MaterialFlag" in it( ex: MaterialFlagLighting, ... )
            if( itr->first.find("MaterialFlag") != std::string::npos )
            {
                IrrMaterialFlag flag;

                IrrMaterialFlagConverter c( itr->first.substr(12) );
                c( flag, itr->second );

                if( flag.mFlag != (E_MATERIAL_FLAG)-1 )
                    mMaterialFlags.push_back( flag );
            }
        }

        // get the material type
        propMap.getValue( mMaterialType, "Template.Render.MaterialType", IrrMaterialTypeConverter(factory) );

        // get the scale and texture scale
        propMap.getValue( mScale,           "Template.Render.Scale" );
        propMap.getValue( mScaleTexture,    "Template.Render.ScaleTexture" );

    #if SCENEOBJECT_ENABLE_STATS
        // display debug scene object info
        if( mAniMesh )
        {
            const aabbox3df& bbox = mAniMesh->getBoundingBox();
            vector3df  dim  = bbox.MaxEdge - bbox.MinEdge;
            dim.X *= mScale.X;
            dim.Y *= mScale.Y;
            dim.Z *= mScale.Z;

            float32_t vol = dim.X * dim.Y * dim.Z;
            vol = (vol<0) ? -vol : vol;

            LOG_F_MSG( "render", "Loaded animated mesh " << aniMeshFile << " with custom scale (" << mScale << ")" );
            LOG_F_MSG( "render", "   Dim: (" << dim.X << ", " << dim.Y << ", " << dim.Z << ")" );
            LOG_F_MSG( "render", "   Volume: " << vol );
        }
    #endif // SCENEOBJECT_ENABLE_STATS
    }

    /// destructor
    SceneObjectTemplate::~SceneObjectTemplate()
    {
        SafeIrrDrop( mAniMesh );
    }

    /// constructor
    SceneObject::SceneObject(SimEntityPtr parent)
		: SimEntityComponent(parent)
		, mSceneNode(NULL)
		, mAniSceneNode(NULL)
		, mTerrSceneNode(NULL)
		, mParticleSystemNode(NULL)
        , mTextNode(NULL)
		, mSceneObjectTemplate()
        , mStartFrame(0)
        , mEndFrame(0)
        , mFootprints()
    {}

    /**
     * The copy constructor should not be called
     */
    SceneObject::SceneObject( const SceneObject& so ) :
    	SimEntityComponent(so.mParent.lock()),
    	mSceneNode(NULL),
    	mAniSceneNode(NULL),
    	mTerrSceneNode(NULL),
    	mParticleSystemNode(NULL),
        mTextNode(NULL),
    	mSceneObjectTemplate(),
        mStartFrame(0),
        mEndFrame(0),
        mFootprints()
    {
        // TODO : Add this if needed (be pointer safe)
        Assert(false);
    }

    /// dtor
    SceneObject::~SceneObject()
    {
        // allow irrlicht to clean up
        if( mSceneNode )
        {
            // remove this node from the scene
            mSceneNode->remove();

            // remove our reference to the node
            SafeIrrDrop(mSceneNode);
        }
        std::list<SimId>::iterator iter;
        for (iter = mFootprints.begin(); iter != mFootprints.end(); ++iter)
        {
            Kernel::GetSimContext()->getSimulation()->Remove(*iter);
        }
    }

    /// assignment operator, tracks references
    SceneObject& SceneObject::operator=( const SceneObject& obj )
    {
        // assign ptrs
        // Note: Only 1 of these (aside from mSceneNode) should be non-null
        mAniSceneNode           = obj.mAniSceneNode;
        mTerrSceneNode          = obj.mTerrSceneNode;
        mSceneObjectTemplate    = obj.mSceneObjectTemplate;
        mParticleSystemNode     = obj.mParticleSystemNode;
        mSceneNode              = obj.mSceneNode;

        // grab a ref to the scene node
        SafeIrrGrab(mSceneNode);

        return *this;
    }

    /**
     * Load this object from a template
     * @param objTemplate template to load from
     * @return true if success
    */
    bool SceneObject::LoadFromTemplate( ObjectTemplatePtr objTemplate, const SimEntityData& data )
    {
        //adziuk
        if( !objTemplate )
            return false;

        Assert( objTemplate->mpSimFactory );

        // cast to object template to the type we expect
        mSceneObjectTemplate = shared_static_cast< SceneObjectTemplate, ObjectTemplate>( objTemplate );

        IrrFactory& irrFactory = mSceneObjectTemplate->mpSimFactory->getIrrFactory();

        // are we an animated mesh?
        if( mSceneObjectTemplate->mAniMesh )
        {
            mAniSceneNode = irrFactory.addAnimatedMeshSceneNode( mSceneObjectTemplate->mAniMesh );
            if (mSceneObjectTemplate->mCastsShadow)
            {
                mAniSceneNode->addShadowVolumeSceneNode();
            }
            mFPSCamera = mSceneObjectTemplate->mFPSCamera; // reminder to attach camera later

			mAniSceneNode->setAnimationSpeed(0);
            mStartFrame = mAniSceneNode->getStartFrame();
            mEndFrame = mAniSceneNode->getEndFrame();
            mAniSceneNode->setFrameLoop(0,0);
            mAniSceneNode->setCurrentFrame(0);

			mSceneNode = mAniSceneNode;
            
            // add triangle selector for a mesh node
            ITriangleSelector* triangleSelector = GetSceneManager()->createTriangleSelector(mAniSceneNode);
            AssertMsg(triangleSelector, "Could not create a collision object for id: " << data.GetId());
            mAniSceneNode->setTriangleSelector(triangleSelector);
            IMetaTriangleSelector* meta = GetSceneManager()->createMetaTriangleSelector();
            meta->addTriangleSelector(mAniSceneNode->getParent()->getTriangleSelector());
            meta->addTriangleSelector(triangleSelector);
            mAniSceneNode->getParent()->setTriangleSelector(meta);
            triangleSelector->drop();
            meta->drop();
        }

        // are we a terrain?
        else if( mSceneObjectTemplate->mHeightmap != "" )
        {
            mTerrSceneNode = irrFactory.addTerrainSceneNode( mSceneObjectTemplate->mHeightmap.c_str() );
            mSceneNode     = mTerrSceneNode;
            mTerrSceneNode->scaleTexture( mSceneObjectTemplate->mScaleTexture.X, mSceneObjectTemplate->mScaleTexture.Y );
            // add triangle selector for a terrain node
            ITriangleSelector* triangleSelector = GetSceneManager()->createTerrainTriangleSelector(mTerrSceneNode);
            AssertMsg(triangleSelector, "Could not create a collision object for id: " << data.GetId());
            mTerrSceneNode->setTriangleSelector(triangleSelector);
            triangleSelector->drop();
        }

        // are we a particle system?
        else if( mSceneObjectTemplate->mParticleSystem != "" )
        {
            mParticleSystemNode = irrFactory.addParticleSystemNode( mSceneObjectTemplate->mParticleSystem );
            mSceneNode          = mParticleSystemNode;
            // don't add a triangle selector for a particle node
        }

        if( mSceneNode )
        {
            // assign the textures
            for( uint32_t i = 0; i < (uint32_t)mSceneObjectTemplate->mTextures.size(); ++i )
                mSceneNode->setMaterialTexture( i, mSceneObjectTemplate->mTextures[i].get() );

            // set the material flags
            std::vector<IrrMaterialFlag>::const_iterator flagItr = mSceneObjectTemplate->mMaterialFlags.begin();
            std::vector<IrrMaterialFlag>::const_iterator flagEnd = mSceneObjectTemplate->mMaterialFlags.end();
            for( ; flagItr != flagEnd; ++flagItr )
                mSceneNode->setMaterialFlag( flagItr->mFlag, flagItr->mValue );

            // set the material type
            mSceneNode->setMaterialType( mSceneObjectTemplate->mMaterialType );

            // set the node scale
            Vector3f scale = mSceneObjectTemplate->mScale;
            if (mSharedData)
            {
                /// we can optionally multiply by a custom scale
                scale.X = scale.X * mSharedData->GetScale().X;
                scale.Y = scale.Y * mSharedData->GetScale().Y;
                scale.Z = scale.Z * mSharedData->GetScale().Z;
            }
            mSceneNode->setScale( ConvertNeroToIrrlichtPosition(scale) );

            // add a ref to the scene node
            SafeIrrGrab(mSceneNode);
            
            // make the id of the scene node the same as the SimId of our object
            mSceneNode->setID(ConvertSimIdToSceneId(data.GetId(), data.GetType()));

        #if SCENEOBJECT_ENABLE_STATS
            // debug information
            if( mTerrSceneNode )
            {
                const aabbox3df& bbox = mTerrSceneNode->getBoundingBox();

                vector3df  dim  = bbox.MaxEdge - bbox.MinEdge;

                float32_t vol = dim.X * dim.Y * dim.Z;
                vol = (vol<0) ? -vol : vol;

                LOG_F_MSG( "render", "Added terrain with heightmap: " << mSceneObjectTemplate->mHeightmap );
                LOG_F_MSG( "render", "   Dim: (" << dim.X << ", " << dim.Y << ", " << dim.Z << ")" );
                LOG_F_MSG( "render", "   Volume: " << vol );
            }
        #endif // end SCENEOBJECT_ENABLE_STATS
        }

        return true;
    }

    void SceneObject::SetText(const std::string& str)
    {
        if (str.empty())
        {
            if (mTextNode)
            {
                mTextNode->remove();
                mTextNode = NULL;
            }
        }
        else
        {
            size_t length = str.size() + 1;
            wchar_t* wstr = new wchar_t[length];
            mbstowcs(wstr, str.c_str(), length);
            if (!mTextNode)
            {
                Font* f = Kernel::GetSimContext()->GetFont();
                mTextNode = GetSceneManager()->addTextSceneNode(f, wstr, SColor(255,128,0,0), mSceneNode, ConvertNeroToIrrlichtPosition(Vector3f(0,0,10)));
            }
            else
            {
                mTextNode->setText(wstr);
            }
        }
    }

    Vector3f FootprintTemplate::GetOffset()
    {
        return Vector3f(RANDOM.randF() - 0.5f, RANDOM.randF() - 0.5f, -1.5f);
    }

    void SceneObject::LeaveFootprints()
    {
        if ( mSceneObjectTemplate->mFootprints->DoStep() )
        {
            // add footprint
            SimId id =
                Kernel::GetSimContext()->AddObject(
                  mSceneObjectTemplate->mFootprints->object,
                  mSharedData->GetPosition() + mSceneObjectTemplate->mFootprints->GetOffset(),
                  mSharedData->GetRotation() );
            mFootprints.push_back(id);
            if (mFootprints.size() > mSceneObjectTemplate->mFootprints->trail)
            {
                id = *mFootprints.begin();
                mFootprints.pop_front();
                Kernel::GetSimContext()->getSimulation()->Remove(id);
            }
        }
    }

    /// Move forward the simulation of this sim object by a time delta
    /// @param dt the amount of time to simulate forward
    void SceneObject::ProcessTick( float32_t dt )
    {
        NERO_PERF_EVENT_SCOPED( SceneObject__ProcessTick );

        Assert( mSharedData );

        // set the position and rotation of our object
        if( mSceneNode )
        {
            // Note: jsheblak 28July2007
            // Setting the position of some large meshes in irrlicht every frame
            // causes them to flicker or disappear. Compare the position first and
            // update if necessary.
            // TODO(ikarpov): currently this is a bit fragile because it assumes ownership of
            //  the shared SimEntityData's dirty bits - we should really have our own view of the
            //  shared cache
            if( mSharedData->IsDirty(SimEntityData::kDB_Position) )
            {
                if (mCamera && mFPSCamera)
                {
                    mFPSCamera->UpdatePosition(mSharedData, mCamera);
                }
            
                // convert from open nero's coordinate system to irrlicht's
                mSceneNode->setPosition( ConvertNeroToIrrlichtPosition(mSharedData->GetPosition()) );

                if (mAniSceneNode && mSceneObjectTemplate->mFootprints)
                {
                    LeaveFootprints();
                }
            }

            if( mSharedData->IsDirty(SimEntityData::kDB_Rotation) )
            {
                if (mCamera && mFPSCamera)
                {
                    mFPSCamera->UpdateRotation(mSharedData, mCamera);
                }

                // Irrlicht expects a left handed basis with the x-z plane being horizontal and y being up
                // OpenNero uses a right handed basis with x-y plane being horizontal and z being up
                mSceneNode->setRotation( ConvertNeroToIrrlichtRotation(mSharedData->GetRotation()) );

            }

            if ( mSharedData->IsDirty(SimEntityData::kDB_Scale) )
            {
                // set the node scale
                Vector3f scale = mSceneObjectTemplate->mScale;
                /// we can optionally multiply by a custom scale
                scale.X = scale.X * mSharedData->GetScale().X;
                scale.Y = scale.Y * mSharedData->GetScale().Y;
                scale.Z = scale.Z * mSharedData->GetScale().Z;
                // convert from open nero's coordinate system to irrlicht's
                mSceneNode->setScale( ConvertNeroToIrrlichtPosition(scale) );
            }

            if ( mSharedData->IsDirty(SimEntityData::kDB_Label) && mSceneObjectTemplate->mDrawLabel )
            {
                SetText(mSharedData->GetLabel());
            }

            if ( mSharedData->IsDirty(SimEntityData::kDB_Color) )
            {
                if (mAniSceneNode) {
                    // The above workaround is not necessary anymore in Irrlicht 1.5
                    mAniSceneNode->getMaterial(0).DiffuseColor = mSharedData->GetColor();
                }
            }

            // add our bounding box to the lineset
            if( mSceneNode && mSceneObjectTemplate->mDrawBoundingBox )
            {
                BBoxf bbox = getTransformedBoundingBox();
                const LineSet::LineColor kGreen(255,0,255,0);

                irr::core::vector3df verts[8];
                bbox.getEdges(verts);

                LineSet::instance().AddSegment( verts[0], verts[1], kGreen );
                LineSet::instance().AddSegment( verts[1], verts[3], kGreen );
                LineSet::instance().AddSegment( verts[3], verts[2], kGreen );
                LineSet::instance().AddSegment( verts[2], verts[0], kGreen );
                LineSet::instance().AddSegment( verts[4], verts[5], kGreen );
                LineSet::instance().AddSegment( verts[5], verts[7], kGreen );
                LineSet::instance().AddSegment( verts[7], verts[6], kGreen );
                LineSet::instance().AddSegment( verts[6], verts[4], kGreen );
                LineSet::instance().AddSegment( verts[4], verts[0], kGreen );
                LineSet::instance().AddSegment( verts[5], verts[1], kGreen );
                LineSet::instance().AddSegment( verts[6], verts[2], kGreen );
                LineSet::instance().AddSegment( verts[7], verts[3], kGreen );
            }

            if (mFPSCamera && !mCamera)
            {
                Kernel::GetSimContext()->getActiveCamera()->attach(this->GetEntity());
            }

            mSharedData->ClearDirtyBits();
        }
    }

    /// get the object template this scene object uses
    ObjectTemplatePtr SceneObject::GetObjectTemplate()
    {
        return mSceneObjectTemplate;
    }

    /// Setup a pointer to the shared data for the parent sim entity
    void SceneObject::SetSharedState( SimEntityData* sharedData )
    {
        Assert( sharedData );
        mSharedData = sharedData;
    }

    /// Get the object space bounding box for the object
    BBoxf SceneObject::getBoundingBox() const
    {
        if ( mSceneNode )
        {
            BBoxf box = mSceneNode->getBoundingBox();
            return BBoxf( ConvertIrrlichtToNeroPosition( box.MinEdge),
                          ConvertIrrlichtToNeroPosition( box.MaxEdge) );
        }

        return BBoxf();
    }

    /// Get the world space bounding box for the object
    BBoxf SceneObject::getTransformedBoundingBox() const
    {
        if ( mSceneNode )
        {
            BBoxf box = mSceneNode->getTransformedBoundingBox();
            return BBoxf( ConvertIrrlichtToNeroPosition( box.MinEdge),
                          ConvertIrrlichtToNeroPosition( box.MaxEdge) );
        }

        return BBoxf();
    }

    /// Transform the given vector by applying the object's matrix
    Vector3f SceneObject::transformVector(const Vector3f& vect) const
    {
        if ( mSceneNode )
        {

            Vector3f result;
            mSceneNode->getAbsoluteTransformation().transformVect(result, ConvertNeroToIrrlichtPosition(vect));
            return ConvertIrrlichtToNeroPosition(result);
        }

        return Vector3f();
    }

    // get the scene object id
    SceneObjectId SceneObject::GetId()
    {
        return mSceneNode? mSceneNode->getID() : -1;
    }

    /// The the mesh buffer for this scene object
    bool SceneObject::getMeshBuffer(MeshBuffer &mb, S32 lod) const
    {
        if ( mTerrSceneNode )
        {
            mTerrSceneNode->getMeshBufferForLOD(mb,lod);
            return true;
        }
        else
        {
            return false;
        }
    }

    /// Get the current scaling factor for this object
    Vector3f SceneObject::getScale() const
    {
        Assert(mSceneNode);
        return ConvertIrrlichtToNeroPosition(mSceneNode->getScale());
    }

    /// Get the current world position for this object
    Vector3f SceneObject::getPosition() const
    {
        Assert( mSceneNode );
        return ConvertIrrlichtToNeroPosition(mSceneNode->getPosition());
    }

    void SceneObject::attachCamera( CameraPtr cam )
    {
        AssertMsg( cam->getFunctionality() == Camera::kFunc_FPS, "Cannot attach non-FPS cameras" );
		AssertMsg( mFPSCamera, "missing information about how to attach the camera" );
        mCamera = cam;
        if (mCamera && mCamera != cam)
        {
            // if we have another camera attached, detach it first
            mCamera->setFunctionality(Camera::kFunc_Nero);
        }
    }

	bool SceneObject::SetAnimation( const std::string& animation_type, const float32_t& animation_speed )
	{
		// convert this string to lowercase
		std::string lower_animation_type = animation_type;
		std::transform( lower_animation_type.begin(), lower_animation_type.end(), lower_animation_type.begin(), ::tolower );

		if (mAniSceneNode) {
			// if we have a node to animate
			// TODO: extend to non-MD2 animations
			if (lower_animation_type == "stand") mAniSceneNode->setMD2Animation(EMAT_STAND);
			else if (lower_animation_type == "run") mAniSceneNode->setMD2Animation(EMAT_RUN);
			else if (lower_animation_type == "attack") mAniSceneNode->setMD2Animation(EMAT_ATTACK);
			else if (lower_animation_type == "pain_a") mAniSceneNode->setMD2Animation(EMAT_PAIN_A);
			else if (lower_animation_type == "pain_b") mAniSceneNode->setMD2Animation(EMAT_PAIN_B);
			else if (lower_animation_type == "pain_c") mAniSceneNode->setMD2Animation(EMAT_PAIN_C);
			else if (lower_animation_type == "jump") mAniSceneNode->setMD2Animation(EMAT_JUMP);
			else if (lower_animation_type == "flip") mAniSceneNode->setMD2Animation(EMAT_FLIP);
			else if (lower_animation_type == "salute") mAniSceneNode->setMD2Animation(EMAT_SALUTE);
			else if (lower_animation_type == "fallback") mAniSceneNode->setMD2Animation(EMAT_FALLBACK);
			else if (lower_animation_type == "wave") mAniSceneNode->setMD2Animation(EMAT_WAVE);
			else if (lower_animation_type == "point") mAniSceneNode->setMD2Animation(EMAT_POINT);
			else if (lower_animation_type == "crouch_stand") mAniSceneNode->setMD2Animation(EMAT_CROUCH_STAND);
			else if (lower_animation_type == "crouch_walk") mAniSceneNode->setMD2Animation(EMAT_CROUCH_WALK);
			else if (lower_animation_type == "crouch_attack") mAniSceneNode->setMD2Animation(EMAT_CROUCH_ATTACK);
			else if (lower_animation_type == "crouch_pain") mAniSceneNode->setMD2Animation(EMAT_CROUCH_PAIN);
			else if (lower_animation_type == "crouch_death") mAniSceneNode->setMD2Animation(EMAT_CROUCH_DEATH);
			else if (lower_animation_type == "death_fallback") mAniSceneNode->setMD2Animation(EMAT_DEATH_FALLBACK);
			else if (lower_animation_type == "death_fallforward") mAniSceneNode->setMD2Animation(EMAT_DEATH_FALLFORWARD);
			else if (lower_animation_type == "death_fallbackslow") mAniSceneNode->setMD2Animation(EMAT_DEATH_FALLBACKSLOW);
			else if (lower_animation_type == "boom") mAniSceneNode->setMD2Animation(EMAT_BOOM);
			else {
				LOG_F_WARNING("render", "Could not set animation to " << animation_type);
				return false;
			}
            return true;
		} else {
			LOG_F_WARNING("render", "Node is not animated when trying to set animation to " << animation_type);
			return false;
		}

	}

};//end OpenNero
