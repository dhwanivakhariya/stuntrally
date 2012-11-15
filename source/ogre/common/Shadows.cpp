#include "pch.h"
#include "../common/Defines.h"
#include "../common/RenderConst.h"

#ifdef ROAD_EDITOR
	#include "../../editor/OgreApp.h"
	#include "../../editor/settings.h"
	#include "../../road/Road.h"
#else
	#include "../OgreGame.h"
	#include "../../vdrift/settings.h"
	#include "../../road/Road.h"
	#include "../SplitScreen.h"
#endif
#include "../../paged-geom/PagedGeometry.h"
#include "../../paged-geom/GrassLoader.h"

#include <OgreTerrain.h>
#include <OgreShadowCameraSetupLiSPSM.h>
#include <OgreShadowCameraSetupPSSM.h>
#include <OgreMaterialManager.h>
#include <OgreOverlay.h>
#include <OgreOverlayContainer.h>
#include <OgreOverlayManager.h>

#include "../../shiny/Main/Factory.hpp"
#include "../../shiny/Platforms/Ogre/OgreMaterial.hpp"

#include "../common/QTimer.h"

using namespace Ogre;
using namespace Forests;


///  Shadows config
//---------------------------------------------------------------------------------------------------
void App::changeShadows()
{	
	QTimer ti;  ti.update();  /// time
	
	//  get settings
	bool enabled = pSet->shadow_type != 0;
	bool bDepth = pSet->shadow_type >= Sh_Depth;
	bool bSoft = pSet->shadow_type == Sh_Soft;
	
	pSet->shadow_size = std::max(0,std::min(ciShadowNumSizes-1, pSet->shadow_size));
	int fTex = ciShadowSizesA[pSet->shadow_size], fTex2 = fTex/2;
	int num = pSet->shadow_count;

	sh::Vector4* fade = new sh::Vector4(
		pSet->shadow_dist,
		pSet->shadow_dist * 0.8, // fade start
		0, 0);

	mFactory->setSharedParameter("shadowFar_fadeStart", sh::makeProperty<sh::Vector4>(fade));

	if (terrain)
	{
		sh::Factory::getInstance().setSharedParameter("terrainWorldSize", sh::makeProperty<sh::FloatValue>(new sh::FloatValue(terrain->getWorldSize())));
		sh::Factory::getInstance().setTextureAlias("TerrainLightMap", terrain->getLightmap()->getName());
	}
		
	// disable 4 shadow textures (does not work because no texcoord's left in shader)
	if (num == 4)  num = 3;


	if (!enabled)  {
		mSceneMgr->setShadowTechnique(SHADOWTYPE_NONE);  /*return;*/ //
	}
	else
	{
		// General scene setup
		//mSceneMgr->setShadowTechnique(SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);
		mSceneMgr->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
		mSceneMgr->setShadowFarDistance(pSet->shadow_dist);  // 3000
		mSceneMgr->setShadowTextureCountPerLightType(Light::LT_DIRECTIONAL, num);

		if (num == 1)  // 1 tex, fast
		{
			ShadowCameraSetupPtr mShadowCameraSetup = ShadowCameraSetupPtr(new LiSPSMShadowCameraSetup());
			mSceneMgr->setShadowCameraSetup(mShadowCameraSetup);
		}else
		{	if (mPSSMSetup.isNull())  // pssm
			{
				PSSMShadowCameraSetup* pssmSetup = new PSSMShadowCameraSetup();
				#ifndef ROAD_EDITOR
				pssmSetup->setSplitPadding(mSplitMgr->mCameras.front()->getNearClipDistance());
				pssmSetup->calculateSplitPoints(num, mSplitMgr->mCameras.front()->getNearClipDistance(), mSceneMgr->getShadowFarDistance());
				#else
				pssmSetup->setSplitPadding(mCamera->getNearClipDistance());
				pssmSetup->calculateSplitPoints(num, mCamera->getNearClipDistance(), mSceneMgr->getShadowFarDistance());
				#endif
				for (int i=0; i < num; ++i)
				{	//int size = i==0 ? fTex : fTex2;
					const Real cAdjfA[5] = {2, 1, 0.5, 0.25, 0.125};
					pssmSetup->setOptimalAdjustFactor(i, cAdjfA[std::min(i, 4)]);
				}
				mPSSMSetup.bind(pssmSetup);
			}
			mSceneMgr->setShadowCameraSetup(mPSSMSetup);
		}

		mSceneMgr->setShadowTextureCount(num);
		for (int i=0; i < num; ++i)
		{	int size = i==0 ? fTex : fTex2;
		
			PixelFormat pf;
			if (bDepth && !bSoft) pf = PF_FLOAT32_R;
			else if (bSoft) pf = PF_FLOAT16_RGB;
			else pf = PF_X8B8G8R8;
			
			mSceneMgr->setShadowTextureConfig(i, size, size, pf);
		}
		
		mSceneMgr->setShadowTextureSelfShadow(bDepth ? true : false);  //-?
		mSceneMgr->setShadowCasterRenderBackFaces((bDepth && !bSoft) ? true : false);
		
		String shadowCasterMat;
		if (bDepth) shadowCasterMat = "PSSM/shadow_caster";

		else shadowCasterMat = StringUtil::BLANK;
		
		mSceneMgr->setShadowTextureCasterMaterial(shadowCasterMat);
	}

	mSceneMgr->setShadowColour(Ogre::ColourValue(0,0,0,1));


	mFactory->setGlobalSetting("shadows", "false");
	mFactory->setGlobalSetting("shadows_pssm", b2s(pSet->shadow_type != Sh_None));
	mFactory->setGlobalSetting("shadows_depth", b2s(pSet->shadow_type >= Sh_Depth));
	mFactory->setGlobalSetting("terrain_specular", b2s(pSet->ter_mtr >= 1));
	mFactory->setGlobalSetting("terrain_normal",   b2s(pSet->ter_mtr >= 2));
	mFactory->setGlobalSetting("terrain_parallax", b2s(pSet->ter_mtr >= 3));
	mFactory->setGlobalSetting("terrain_triplanar",b2s(pSet->ter_mtr >= 4));

	mFactory->setGlobalSetting("water_reflect", b2s(pSet->water_reflect));
	mFactory->setGlobalSetting("water_refract", b2s(pSet->water_refract));


#if !ROAD_EDITOR
	mFactory->setGlobalSetting("soft_particles", b2s(pSet->all_effects && pSet->softparticles));
	mFactory->setGlobalSetting("mrt_output", b2s(NeedMRTBuffer()));
#endif

	#if 0
	// shadow tex overlay
	// add the overlay elements to show the shadow maps:
	// init overlay elements
	OverlayManager& mgr = OverlayManager::getSingleton();
	Overlay* overlay;
	
	// destroy if already exists
	if (overlay = mgr.getByName("DebugOverlay"))
		mgr.destroy(overlay);
		
	overlay = mgr.create("DebugOverlay");
	
	TexturePtr tex;
	for (int i = 0; i < pSet->shadow_count; ++i)
	{	
		TexturePtr tex = mSceneMgr->getShadowTexture(i);
		
		// Set up a debug panel to display the shadow
		if (MaterialManager::getSingleton().resourceExists("Ogre/DebugTexture" + toStr(i)))
			MaterialManager::getSingleton().remove("Ogre/DebugTexture" + toStr(i));
		MaterialPtr debugMat = MaterialManager::getSingleton().create(
			"Ogre/DebugTexture" + toStr(i), 
			ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
			
		debugMat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
		TextureUnitState *t = debugMat->getTechnique(0)->getPass(0)->createTextureUnitState(tex->getName());
		t->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);

		OverlayContainer* debugPanel;
		
		// destroy container if exists
		try
		{
			if (debugPanel = static_cast<OverlayContainer*>(mgr.getOverlayElement("Ogre/DebugTexPanel" + toStr(i))))
				mgr.destroyOverlayElement(debugPanel);
		}
		catch (Ogre::Exception&) {}
		
		debugPanel = (OverlayContainer*)
			(OverlayManager::getSingleton().createOverlayElement("Panel", "Ogre/DebugTexPanel" + StringConverter::toString(i)));
		debugPanel->_setPosition(0.8, i*0.31);  //aspect.. 0.25 0.24
		debugPanel->_setDimensions(0.2, 0.3);
		debugPanel->setMaterialName(debugMat->getName());
		debugPanel->show();
		overlay->add2D(debugPanel);
		overlay->show();
	}
	#endif
	
	UpdPSSMMaterials();


	//  rebuild static geom after materials change
	if (mStaticGeom)
	{
		mStaticGeom->destroy();
		mStaticGeom->build();
	}

	ti.update();	/// time
	float dt = ti.dt * 1000.f;
	LogO(String("::: Time Shadows: ") + toStr(dt) + " ms");
}


/// . . . . . . . . 
void App::UpdPSSMMaterials()
{
	if (pSet->shadow_type == Sh_None)  return;
	
	if (pSet->shadow_count == 1)  // 1 tex
	{
		float dist = pSet->shadow_dist;
		sh::Vector3* splits = new sh::Vector3(dist, 0,0);  //dist*2, dist*3);
		sh::Factory::getInstance().setSharedParameter("pssmSplitPoints", sh::makeProperty<sh::Vector3>(splits));
		return;
	}
	
	if (!mPSSMSetup.get())  return;
	
	//--  pssm params
	PSSMShadowCameraSetup* pssmSetup = static_cast<PSSMShadowCameraSetup*>(mPSSMSetup.get());
	const PSSMShadowCameraSetup::SplitPointList& sp = pssmSetup->getSplitPoints();
	const int last = sp.size()-1;

	sh::Vector3* splits = new sh::Vector3(
		sp[std::min(1,last)], sp[std::min(2,last)], sp[std::min(3,last)] );

	sh::Factory::getInstance().setSharedParameter("pssmSplitPoints", sh::makeProperty<sh::Vector3>(splits));
}


//  . . . . . . . . 
void App::materialCreated(sh::MaterialInstance* m, const std::string& configuration, unsigned short lodIndex)
{

	Ogre::Technique* t = static_cast<sh::OgreMaterial*>(m->getMaterial())->getOgreTechniqueForConfiguration (configuration, lodIndex);

	if (pSet->shadow_type <= Sh_Simple)
	{
		t->setShadowCasterMaterial("");
		return;
	}

	// this is just here to set the correct shadow caster
	if (m->hasProperty("transparent") && m->hasProperty("cull_hardware") &&
		sh::retrieveValue<sh::StringValue>(m->getProperty("cull_hardware"), 0).get() == "none")
	{
		// Crash !?
		///assert(!MaterialManager::getSingleton().getByName("PSSM/shadow_caster_nocull").isNull ());
		//t->setShadowCasterMaterial("PSSM/shadow_caster_nocull");
	}

	bool noalpha = !m->hasProperty("transparent") || !sh::retrieveValue<sh::BooleanValue>(m->getProperty("transparent"), 0).get();
	bool instancing = m->hasProperty("instancing") && sh::retrieveValue<sh::BooleanValue>(m->getProperty("instancing"), 0).get();
	/// TODO: in settings and gui chk..
	//instancing = false;
	//noalpha = true;

	std::string vertex = "PSSM/shadow_caster";
	if (instancing)  vertex += "_instancing";
	vertex += "_vs";

	std::string fragment = "PSSM/shadow_caster";

	if (!noalpha)  fragment += "_alpha";
	fragment += "_ps";

	t->getPass(0)->setShadowCasterVertexProgram(vertex);
	t->getPass(0)->setShadowCasterFragmentProgram(fragment);
}
