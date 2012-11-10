#include "pch.h"
#include "../common/Defines.h"
#include "RenderConst.h"
#ifdef ROAD_EDITOR
	#include "../../editor/OgreApp.h"
#else
	#include "../OgreGame.h"
	#include "../../vdrift/settings.h"
	#include "../../vdrift/game.h"
	#include "../SplitScreen.h"
#endif
#include "../../vdrift/pathmanager.h"
#include "../../paged-geom/GrassLoader.h"
#include "../../paged-geom/BatchPage.h"
#include "../../paged-geom/WindBatchPage.h"
#include "../../paged-geom/ImpostorPage.h"
#include "../../paged-geom/TreeLoader2D.h"
#include "../../paged-geom/MersenneTwister.h"
#include "BltObjects.h"

#include <boost/filesystem.hpp>
#include <OgreTerrain.h>
#include <OgreInstanceManager.h>
using namespace Ogre;


//---------------------------------------------------------------------------------------------------------------
///  Trees  ^ ^ ^ ^
//---------------------------------------------------------------------------------------------------------------

Terrain* gTerrain = NULL;
//bool gbLookAround = false;

inline Real getTerrainHeight(const Real x, const Real z, void *userData)
{
	return gTerrain->getHeightAtWorldPosition(x, 0, z);
}


void App::CreateTrees()
{
	QTimer ti;  ti.update();  /// time
	gTerrain = terrain;
	
	//-------------------------------------- Grass --------------------------------------
	int imgRoadSize = 0;
	Image imgRoad;
	try{
		imgRoad.load(String("roadDensity.png"),"General");
	}catch(...)
	{
		imgRoad.load(String("grassDensity.png"),"General");
	}
	imgRoadSize = imgRoad.getWidth();  // square[]
		
	// remove old BinFolder's (paged geom temp resource groups)
	if (ResourceGroupManager::getSingleton().resourceGroupExists("BinFolder"))
	{
		StringVectorPtr locations = ResourceGroupManager::getSingleton().listResourceLocations("BinFolder");
		for (StringVector::const_iterator it=locations->begin(); it!=locations->end(); ++it)
		{
			ResourceGroupManager::getSingleton().removeResourceLocation( (*it), "BinFolder" );
		}
	}

	using namespace Forests;
	Real tws = sc->td.fTerWorldSize * 0.5f;
	TBounds tbnd(-tws, -tws, tws, tws);
	//  pos0 - original  pos - with offset
	Vector3 pos0 = Vector3::ZERO, pos = Vector3::ZERO;  Radian yaw;

	bool bWind = 1;	 /// WIND

	Real fGrass = pSet->grass * sc->densGrass * 3.0f;  // std::min(pSet->grass, 
	#ifdef ROAD_EDITOR
	Real fTrees = pSet->gui.trees * sc->densTrees;
	#else
	Real fTrees = pSet->game.trees * sc->densTrees;
	#endif
	
	if (fGrass > 0.f)
	{
		#ifndef ROAD_EDITOR
		grass = new PagedGeometry(mSplitMgr->mCameras.front(), sc->grPage);  //30
		#else
		grass = new PagedGeometry(mCamera, sc->grPage);  //30
		#endif
		
		// create dir if not exist
		boost::filesystem::create_directory(PATHMANAGER::GetCacheDir() + "/" + toStr(sc->sceneryId));
		grass->setTempDir(PATHMANAGER::GetCacheDir() + "/" + toStr(sc->sceneryId) + "/");
		
		grass->addDetailLevel<GrassPage>(sc->grDist * pSet->grass_dist);

		GrassLoader *grassLoader = new Forests::GrassLoader(grass);
		grassLoader->setRenderQueueGroup(RQG_BatchAlpha);
		grass->setPageLoader(grassLoader);
		grassLoader->setHeightFunction(&getTerrainHeight);

		//  Grass layers
		for (int i=0; i < sc->ciNumGrLay; ++i)
		{
			const SGrassLayer* gr = &sc->grLayersAll[i];
			if (gr->on)
			{
				GrassLayer *l = grassLoader->addLayer(gr->material);
				l->setMinimumSize(gr->minSx, gr->minSy);  l->setMaximumSize(gr->maxSx, gr->maxSy);
				l->setDensity(gr->dens * fGrass);
				l->setSwayDistribution(gr->swayDistr);
				l->setSwayLength(gr->swayLen);  l->setSwaySpeed(gr->swaySpeed);

				l->setAnimationEnabled(true);  //l->setLightingEnabled(true);
				l->setRenderTechnique(GRASSTECH_CROSSQUADS);  //GRASSTECH_SPRITE-
				l->setFadeTechnique(FADETECH_ALPHA);  //FADETECH_GROW-

				l->setColorMap(gr->colorMap);
				l->setDensityMap("grassDensity.png",CHANNEL_RED);  //todo: more..
				l->setMapBounds(tbnd);
			}
		}

		grass->setShadersEnabled(true);
	}
	ti.update();  /// time
	float dt = ti.dt * 1000.f;
	LogO(String("::: Time Grass: ") + toStr(dt) + " ms");


///  HW Instancing  ---- ---- ---- ---- ---- ---- ---- ----
	//InstanceManager* instMgr[2];
	//std::vector<MovableObject*> mEntities[2];

	//if (instMgr)
	//	mSceneMgr->destroyInstanceManager(mCurrentManager);

	static const char *c_meshNames[] =
	{
		//"treeAOR-13oakWBig.mesh",
		//"treeAW-14LLHuge7k.mesh",
		"treeAR-10oakMed.mesh"
	};
	NUM_INST_ROW = 3*12;

	for (int n = 0; n < 2; ++n)  // 2 submeshes
	{
		instMgr[n] = mSceneMgr->createInstanceManager(
			"InstanceMgr"+toStr(n), c_meshNames[0],
			ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME,
			InstanceManager::HWInstancingBasic,
			NUM_INST_ROW * NUM_INST_ROW, IM_USEALL, n);
		
		MTRand rnd((MTRand::uint32)1213);

		for (int i=0; i<NUM_INST_ROW; ++i)
		for (int j=0; j<NUM_INST_ROW; ++j)
		{
			//Create the instanced entity
			InstancedEntity* ent = instMgr[n]->createInstancedEntity(
				n == 0 ? "Examples/Instancing/HWBasic/Tree" : "Examples/Instancing/HWBasic/TreeA");
			mEntities[n].push_back( ent );

			//mMovedInstances.push_back( ent );
			ent->setOrientation(Quaternion(Radian(rnd.rand()*PI_d), Vector3::UNIT_Y));
			ent->setScale((0.3f + 1.2f*rnd.rand()) * Vector3::UNIT_SCALE * 1.4f);
			ent->setPosition( Ogre::Vector3(
				(i - NUM_INST_ROW * 0.5f) * 6.f, -10,
				(j - NUM_INST_ROW * 0.5f) * 6.f) );
				
		}
	}


	//---------------------------------------------- Trees ----------------------------------------------
	if (0)  ///
	if (fTrees > 0.f)
	{
		// fast: 100_ 80 j1T!,  400 400 good sav2f  200 220 both`-
		#ifndef ROAD_EDITOR
		trees = new PagedGeometry(mSplitMgr->mCameras.front(), sc->trPage);
		#else
		trees = new PagedGeometry(mCamera, sc->trPage);
		#endif
		
		// create dir if not exist
		boost::filesystem::create_directory(PATHMANAGER::GetCacheDir() + "/" + toStr(sc->sceneryId));
		trees->setTempDir(PATHMANAGER::GetCacheDir() + "/" + toStr(sc->sceneryId) + "/");

		if (!pSet->imposters_only)
		{
			if (bWind)
				 trees->addDetailLevel<WindBatchPage>(sc->trDist * pSet->trees_dist, 0);
			else trees->addDetailLevel<BatchPage>	 (sc->trDist * pSet->trees_dist, 0);
		}
		if (pSet->use_imposters)
			trees->addDetailLevel<ImpostorPage>(sc->trDistImp * pSet->trees_dist, 0);

		TreeLoader2D* treeLoader = new TreeLoader2D(trees, tbnd);
		trees->setPageLoader(treeLoader);
		treeLoader->setHeightFunction(getTerrainHeight/*Around /*,userdata*/);
		treeLoader->setMaximumScale(4);  //6
		//treeLoader->setMinimumScale(0.5);  // todo: rescale all meshes, range is spread to only 255 vals!
		tws = sc->td.fTerWorldSize;
		int r = imgRoadSize, cntr = 0, cntshp = 0, txy = sc->td.iVertsX*sc->td.iVertsY-1;

		//  set random seed  /// todo: seed in scene.xml and in editor gui...
		MTRand rnd((MTRand::uint32)1213);
		#define getTerPos()		(rnd.rand()-0.5) * sc->td.fTerWorldSize

		//  Tree Layers
		for (size_t l=0; l < sc->pgLayers.size(); ++l)
		{
			PagedLayer& pg = sc->pgLayersAll[sc->pgLayers[l]];

			Entity* ent = mSceneMgr->createEntity(pg.name);
			ent->setVisibilityFlags(RV_Vegetation);  ///vis+  disable in render targets
			if (pg.windFx > 0.f)  {
				trees->setCustomParam(ent->getName(), "windFactorX", pg.windFx);
				trees->setCustomParam(ent->getName(), "windFactorY", pg.windFy);  }

			///  collision object
			const BltCollision* col = objs.Find(pg.name);
			Vector3 ofs(0,0,0);  if (col)  ofs = col->offset;  // mesh offset

			//  num trees  ----------------------------------------------------------------
			int cnt = fTrees * 6000 * pg.dens;
			for (int i = 0; i < cnt; ++i)
			{
				#if 0  ///  for new objects - test shapes
					int ii = l*cnt+i;
					yaw = Degree((ii*30)%360);  // grid
					pos.z = -100 +(ii / 12) * 10;  pos.x = -100 +(ii % 12) * 10;
					Real scl = pg.minScale;
				#else
					yaw = Degree(rnd.rand(360.0));
					pos.x = getTerPos();  pos.z = getTerPos();
					Real scl = rnd.rand() * (pg.maxScale-pg.minScale) + pg.minScale;
				#endif
				pos0 = pos;  // store original place
				bool add = true;

				//  offset mesh  pos, rotY, scl
				Vector2 vo;  float yr = -yaw.valueRadians();
				float cyr = cos(yr), syr = sin(yr);
				vo.x = ofs.x * cyr - ofs.y * syr;  // ofs x,y for pos x,z
				vo.y = ofs.x * syr + ofs.y * cyr;
				pos.x += vo.x * scl;  pos.z += vo.y * scl;
				
				//  check if on road - uses grassDensity
				if (r > 0)  //  ----------------
				{
				int mx = (pos.x + 0.5*tws)/tws*r,
					my = (pos.z + 0.5*tws)/tws*r;

					int c = sc->trRdDist + pg.addTrRdDist;
					int d = c;  //std::max(c, 20);  //pg.addTrRdDistMax
					register int ii,jj, rr, rmin = 3000;  //d
					for (jj = -d; jj <= d; ++jj)
					for (ii = -d; ii <= d; ++ii)
					{
						float cr = imgRoad.getColourAt(
							std::max(0,std::min(r-1, mx+ii)),
							std::max(0,std::min(r-1, my+jj)), 0).r;
						
						rr = abs(ii)+abs(jj);
						//rr = sqrt(float(ii*ii+jj*jj));  // much slower
						if (cr < 0.75f)  //par-
							rmin = std::min(rmin, rr);
					}
					if (rmin <= c)
						add = false;

					//if (rmin >= d-2)  // max dist to road
					//	add = false;
				}
				if (!add)  continue;  //?faster

				//  check ter angle  ------------
				int mx = (pos.x + 0.5*tws)/tws*sc->td.iVertsX,
					my =(-pos.z + 0.5*tws)/tws*sc->td.iVertsY;
				int a = std::max(0, std::min(txy, my*sc->td.iVertsX+mx));
				if (sc->td.hfAngle[a] > pg.maxTerAng)
					add = false;

				if (!add)  continue;  //

				//  check ter height  ------------
				pos.y = terrain->getHeightAtWorldPosition(pos.x, 0, pos.z);
				if (pos.y < pg.minTerH || pos.y > pg.maxTerH)
					add = false;				
				
				if (!add)  continue;  //
				
				//  check if in fluids  ------------
				float fa = 0.f;  // depth
				for (int fi=0; fi < sc->fluids.size(); ++fi)
				{
					const FluidBox& fb = sc->fluids[fi];
					if (fb.pos.y - pos.y > 0.f)  // dont check when above fluid, ..or below its size-
					{
						const float sizex = fb.size.x*0.5f, sizez = fb.size.z*0.5f;
						//  check rect 2d - no rot !
						if (pos.x > fb.pos.x - sizex && pos.x < fb.pos.x + sizex &&
							pos.z > fb.pos.z - sizez && pos.z < fb.pos.z + sizez)
						{
							float f = fb.pos.y - pos.y;
							if (f > fa)  fa = f;
						}
					}
				}
				if (fa > pg.maxDepth)
					add = false;
				
				if (!add)  continue;

				treeLoader->addTree(ent, pos0, yaw, scl);
				cntr++;
					
				
				///  add to bullet world
				#ifndef ROAD_EDITOR  //  in Game
				if (pSet->game.collis_veget && col)
				for (int c=0; c < col->shapes.size(); ++c)  // all shapes
				{
					const BltShape* shp = &col->shapes[c];
					Vector3 pos = pos0;  // restore original place
					Vector3 ofs = shp->offset;
					//  offset shape  pos, rotY, scl
					Vector2 vo;  float yr = -yaw.valueRadians();
					float cyr = cos(yr), syr = sin(yr);
					vo.x = ofs.x * cyr - ofs.y * syr;
					vo.y = ofs.x * syr + ofs.y * cyr;
					pos.x += vo.x * scl;  pos.z += vo.y * scl;

					//  apply pos offset xyz, rotY, mul by scale
					pos.y = terrain->getHeightAtWorldPosition(pos.x, 0, pos.z);
					btVector3 pc(pos.x, -pos.z, pos.y + ofs.z * scl);  // center
					btTransform tr;  tr.setIdentity();  tr.setOrigin(pc);

					btCollisionShape* bshp = 0;
					if (shp->type == BLT_CapsZ)
						bshp = new btCapsuleShapeZ(shp->radius * scl, shp->height * scl);
					else
						bshp = new btSphereShape(shp->radius * scl);
					//shp->setUserPointer((void*)7777);  // mark as vegetation ..

					btCollisionObject* bco = new btCollisionObject();
					bco->setActivationState(DISABLE_SIMULATION);
					bco->setCollisionShape(bshp);	bco->setWorldTransform(tr);
					bco->setFriction(shp->friction);	bco->setRestitution(shp->restitution);
					bco->setCollisionFlags(bco->getCollisionFlags() |
						btCollisionObject::CF_STATIC_OBJECT /*| btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT/**/);
					pGame->collision.world->addCollisionObject(bco);
					pGame->collision.shapes.push_back(bshp);  cntshp++;
				}
				#endif
			}
		}
		LogO(String("***** Vegetation objects count: ") + toStr(cntr) + "  shapes: " + toStr(cntshp));
	}
	//imgRoadSize = 0;
	ti.update();  /// time
	dt = ti.dt * 1000.f;
	LogO(String("::: Time Trees: ") + toStr(dt) + " ms");
}
