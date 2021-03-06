#include "pch.h"
#include "track.h"

#include "configfile.h"
#include "reseatable_reference.h"
#include "tracksurface.h"
#include "objectloader.h"
#include <functional>
#include <algorithm>
#include "../ogre/common/Def_Str.h"
#include "game.h"  // for tires map

#include <list>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;


TRACK::TRACK(ostream & info, ostream & error) 
	:pGame(0),
	info_output(info), error_output(error),
	texture_size("large"),
	vertical_tracking_skyboxes(false),
	loaded(false),
	cull(false),
	sDefaultTire("gravel")
{
}

TRACK::~TRACK()
{
	Clear();
}

bool TRACK::Load(
	const string & trackpath,
	const string & effects_texturepath,
	//SCENENODE & rootnode,
	bool reverse,
	int anisotropy,
	const string & texsize)
{
	Clear();

	info_output << "Loading track from path: " << trackpath << endl;

	//load parameters
	if (!LoadParameters(trackpath))
		return false;
	
	//load roads
	if (!LoadRoads(trackpath, reverse))
	{
		error_output << "Error during road loading; continuing with an unsmoothed track" << endl;
		ClearRoads();
	}

	//load the lap sequence
	if (!LoadLapSequence(trackpath, reverse))
		return false;

	if (!CreateRacingLines(/*&rootnode,* effects_texturepath, texsize*/))
		return false;

	//load objects
	if (!LoadObjects(trackpath, /*rootnode,*/ anisotropy))
	{
		info_output << "Track with no objects." << endl;
		//return false;
	}

	info_output << "Track loaded: " << model_library.size() << " models, " << texture_library.size() << " textures" << endl;

	return true;
}

bool TRACK::LoadLapSequence(const string & trackpath, bool reverse)
{
	string parampath = trackpath + "/track.txt";
	CONFIGFILE trackconfig;
	if (!trackconfig.Load(parampath))
	{
		error_output << "Can't find track configfile: " << parampath << endl;
		return false;
	}
	
	trackconfig.GetParam("cull faces", cull);

	int lapmarkers = 0;
	if (trackconfig.GetParam("lap sequences", lapmarkers))
	{
		for (int l = 0; l < lapmarkers; l++)
		{
			float lapraw[3];
			stringstream lapname;
			lapname << "lap sequence " << l;
			trackconfig.GetParam(lapname.str(), lapraw);
			int roadid = lapraw[0];
			int patchid = lapraw[1];

			//info_output << "Looking for lap sequence: " << roadid << ", " << patchid << endl;

			int curroad = 0;
			for (list <ROADSTRIP>::iterator i = roads.begin(); i != roads.end(); ++i)
			{
				if (curroad == roadid)
				{
					int curpatch = 0;
					for (list <ROADPATCH>::const_iterator p = i->GetPatchList().begin(); p != i->GetPatchList().end(); ++p)
					{
						if (curpatch == patchid)
						{
							lapsequence.push_back(&p->GetPatch());
							//info_output << "Lap sequence found: " << roadid << ", " << patchid << "= " << &p->GetPatch() << endl;
						}
						curpatch++;
					}
				}
				curroad++;
			}
		}
	}

	// calculate distance from starting line for each patch to account for those tracks
	// where starting line is not on the 1st patch of the road
	// note this only updates the road with lap sequence 0 on it
	if (!lapsequence.empty())
	{
		BEZIER* start_patch = const_cast <BEZIER *> (lapsequence[0]);
		start_patch->dist_from_start = 0.0;
		BEZIER* curr_patch = start_patch->next_patch;
		float total_dist = start_patch->length;
		int count = 0;
		while ( curr_patch && curr_patch != start_patch)
		{
			count++;
			curr_patch->dist_from_start = total_dist;
			total_dist += curr_patch->length;
			curr_patch = curr_patch->next_patch;
		}
	}

	if (lapmarkers == 0)
		info_output << "No lap sequence found; lap timing will not be possible" << endl;
	else
		info_output << "Track timing sectors: " << lapmarkers << endl;

	return true;
}

bool TRACK::DeferredLoad(
	const string & trackpath,
	bool reverse,
	int anisotropy,
	const string & texsize,
	bool dynamicshadowsenabled,
	bool doagressivecombining)
{
	Clear();

	texture_size = texsize;
	info_output << "Loading track from path: " << trackpath << endl;

	//load parameters
	if (!LoadParameters(trackpath))
		return false;

	//load roads
	if (!LoadRoads(trackpath, reverse))
	{
		error_output << "Error during road loading; continuing with an unsmoothed track" << endl;
		ClearRoads();
	}

	//load the lap sequence
	if (!LoadLapSequence(trackpath, reverse))
		return false;

	if (!CreateRacingLines(/*&rootnode, effects_texturepath, texsize*/))
		return false;

	//load objects
	if (!BeginObjectLoad(trackpath, /*rootnode,*/ anisotropy, dynamicshadowsenabled, doagressivecombining))
		return false;

	return true;
}

bool TRACK::ContinueDeferredLoad()
{
	if (Loaded())
		return true;

	pair <bool,bool> loadstatus = ContinueObjectLoad();
	if (loadstatus.first)
		return false;
	if (!loadstatus.second)
	{
		loaded = true;
	}
	return true;
}

int TRACK::DeferredLoadTotalObjects()
{
	assert(objload.get());
	return objload->GetNumObjects();
}

void TRACK::Clear()
{
	objects.clear();
	model_library.clear();
	/**/ogre_meshes.clear();///
	texture_library.clear();
	ClearRoads();
	lapsequence.clear();
	start_positions.clear();
	//racingline_node = NULL;
	loaded = false;
}

bool TRACK::CreateRacingLines(
	//SCENENODE * parentnode, 
	/*const string & texturepath,
	const string & texsize*/)
{
	/*assert(parentnode);
	if (!racingline_node)
	{
		racingline_node = &parentnode->AddNode();
	}*/
	
	/*if (!racingline_texture.Loaded())
	{
		TEXTUREINFO tex; 
		tex.SetName(texturepath + "/racingline.png");
		if (!racingline_texture.Load(tex, error_output, texsize))
			return false;
	}*/
	
	K1999 k1999data;
	int n = 0;
	for (list <ROADSTRIP>::iterator i = roads.begin(); i != roads.end(); ++i,++n)
	{
		if (k1999data.LoadData(&(*i)))
		{
			k1999data.CalcRaceLine();
			k1999data.UpdateRoadStrip(&(*i));
		}
		//else error_output << "Couldn't create racing line for roadstrip " << n << endl;
		
		//i->CreateRacingLine(racingline_node, racingline_texture, error_output);
	}
	
	return true;
}

bool TRACK::LoadParameters(const string & trackpath)
{
	string parampath = trackpath + "/track.txt";
	CONFIGFILE param;
	if (!param.Load(parampath))
	{
		error_output << "Can't find track configfile: " << parampath << endl;
		return false;
	}

	vertical_tracking_skyboxes = false; //default to false
	param.GetParam("vertical tracking skyboxes", vertical_tracking_skyboxes);
	//cout << vertical_tracking_skyboxes << endl;

	int sp_num = 0;
	stringstream sp_name;
	sp_name << "start position " << sp_num;
	float f3[3];
	float f1;
	while (param.GetParam(sp_name.str(), f3))
	{
		MATHVECTOR<float,3> pos(f3[2], f3[0], f3[1]);

		sp_name.str("");
		sp_name << "start orientation-xyz " << sp_num;
		if (!param.GetParam(sp_name.str(), f3))
		{
			error_output << "No matching orientation xyz for start position " << sp_num << endl;
			return false;
		}
		sp_name.str("");
		sp_name << "start orientation-w " << sp_num;
		if (!param.GetParam(sp_name.str(), f1))
		{
			error_output << "No matching orientation w for start position " << sp_num << endl;
			return false;
		}

		QUATERNION<float> orient(f3[2], f3[0], f3[1], f1);
		//QUATERNION<float> orient(f3[0], f3[1], f3[2], f1);

		//due to historical reasons the initial orientation places the car faces the wrong way
		QUATERNION<float> fixer; 
		fixer.Rotate(PI_d, 0, 0, 1);
		orient = fixer * orient;

		start_positions.push_back(pair <MATHVECTOR<float,3>, QUATERNION<float> >
				(pos, orient));

		sp_num++;
		sp_name.str("");
		sp_name << "start position " << sp_num;
	}

	return true;
}


bool TRACK::BeginObjectLoad(
	const string & trackpath,
	//SCENENODE & sceneroot,
	int anisotropy,
	bool dynamicshadowsenabled,
	bool doagressivecombining)
{
	objload.reset(new OBJECTLOADER(trackpath, /*sceneroot,*/ anisotropy, dynamicshadowsenabled,
		info_output, error_output, cull, doagressivecombining));

	if (!objload->BeginObjectLoad())
		return false;

	return true;
}

pair <bool,bool> TRACK::ContinueObjectLoad()
{
	assert(objload.get());
	return objload->ContinueObjectLoad(this, model_library, texture_library,
		objects, vertical_tracking_skyboxes, texture_size);
}

bool TRACK::LoadObjects(const string & trackpath, /*SCENENODE & sceneroot,*/ int anisotropy)
{
	BeginObjectLoad(trackpath, /*sceneroot,*/ anisotropy, false, false);
	pair <bool,bool> loadstatus = ContinueObjectLoad();
	while (!loadstatus.first && loadstatus.second)
	{
		loadstatus = ContinueObjectLoad();
	}
	return !loadstatus.first;
}

void TRACK::Reverse()
{
	//move timing sector 0 back 1 patch so we'll still drive over it when going in reverse around the track
	if (!lapsequence.empty())
	{
		int counts = 0;

		for (list <ROADSTRIP>::iterator i = roads.begin(); i != roads.end(); ++i)
		{
			optional <const BEZIER *> newstartline = i->FindBezierAtOffset(lapsequence[0],-1);
			if (newstartline)
			{
				lapsequence[0] = newstartline.get();
				counts++;
			}
		}

		assert(counts == 1); //do a sanity check, because I don't trust the FindBezierAtOffset function
	}

	//reverse the timing sectors
	if (lapsequence.size() > 1)
	{
		//reverse the lap sequence, but keep the first bezier where it is (remember, the track is a loop)
		//so, for example, now instead of 1 2 3 4 we should have 1 4 3 2
		vector <const BEZIER *>::iterator secondbezier = lapsequence.begin();
		++secondbezier;
		assert(secondbezier != lapsequence.end());
		reverse(secondbezier, lapsequence.end());
	}


	//flip start positions
	for (vector <pair <MATHVECTOR<float,3>, QUATERNION<float> > >::iterator i = start_positions.begin();
		i != start_positions.end(); ++i)
	{
		i->second.Rotate(PI_d, 0,0,1);
		i->second[0] = -i->second[0];
		i->second[1] = -i->second[1];
		//i->second[2] = -i->second[2];
		//i->second[3] = -i->second[3];
	}

	//reverse start positions
	reverse(start_positions.begin(), start_positions.end());

	//reverse roads
	for_each(roads.begin(), roads.end(), mem_fun_ref(&ROADSTRIP::Reverse));
}

bool TRACK::LoadRoads(const string & trackpath, bool reverse)
{
	ClearRoads();

	ifstream trackfile;
	trackfile.open((trackpath + "/roads.trk").c_str());
	if (!trackfile)
	{
		//error_output << "Error opening roads file: " << trackpath + "/roads.trk" << endl;
		//return false;
	}

	int numroads=0;

	trackfile >> numroads;

	for (int i = 0; i < numroads && trackfile; i++)
	{
		roads.push_back(ROADSTRIP());
		roads.back().ReadFrom(trackfile, error_output);
	}

	if (reverse)
	{
		Reverse();
		direction = DIRECTION_REVERSE;
	}
	else
		direction = DIRECTION_FORWARD;

	return true;
}

bool TRACK::CastRay(
	const MATHVECTOR<float,3> & origin,
	const MATHVECTOR<float,3> & direction,
	float seglen, MATHVECTOR<float,3> & outtri,
	const BEZIER * & colpatch,
	MATHVECTOR<float,3> & normal) const
{
	bool col = false;
	for (list <ROADSTRIP>::const_iterator i = roads.begin(); i != roads.end(); ++i)
	{
		MATHVECTOR<float,3> coltri, colnorm;
		const BEZIER * colbez = NULL;
		if (i->Collide(origin, direction, seglen, coltri, colbez, colnorm))
		{
			if (!col || (coltri-origin).Magnitude() < (outtri-origin).Magnitude())
			{
				outtri = coltri;
				normal = colnorm;
				colpatch = colbez;
			}

			col = true;
		}
	}

	return col;
}

optional <const BEZIER *> ROADSTRIP::FindBezierAtOffset(const BEZIER * bezier, int offset) const
{
	list <ROADPATCH>::const_iterator it = patches.end(); //this iterator will hold the found ROADPATCH

	//search for the roadpatch containing the bezier and store an iterator to it in "it"
	for (list <ROADPATCH>::const_iterator i = patches.begin(); i != patches.end(); ++i)
	{
		if (&i->GetPatch() == bezier)
		{
			it = i;
			break;
		}
	}

	if (it == patches.end())
		return optional <const BEZIER *>(); //return nothing
	else
	{
		//now do the offset
		int curoffset = offset;
		while (curoffset != 0)
		{
			if (curoffset < 0)
			{
				//why is this so difficult?  all i'm trying to do is make the iterator loop around
				list <ROADPATCH>::const_reverse_iterator rit(it);
				if (rit == patches.rend())
					rit = patches.rbegin();
				rit++;
				if (rit == patches.rend())
					rit = patches.rbegin();
				it = rit.base();
				if (it == patches.end())
					it = patches.begin();

				curoffset++;
			}
			else if (curoffset > 0)
			{
				it++;
				if (it == patches.end())
					it = patches.begin();

				curoffset--;
			}
		}

		assert(it != patches.end());
		return optional <const BEZIER *>(&it->GetPatch());
	}
}

pair <MATHVECTOR<float,3>, QUATERNION<float> > TRACK::GetStart(unsigned int index)
{
	assert(!start_positions.empty());
	unsigned int laststart = 0;  // force auto gen  // start_positions.size()-1;
	
	if (index > laststart || start_positions.empty())
	{
		pair <MATHVECTOR<float,3>, QUATERNION<float> > sp;
		if (!start_positions.empty())
			sp = start_positions[laststart];
		else
			sp = make_pair(MATHVECTOR<float,3>(0,0,0), QUATERNION<float>(0,0,0,1));
			
		MATHVECTOR<float,3> backward(-6,0,0);  // par dist back
		backward = backward * (index-laststart);
		sp.second.RotateVector(backward);
		sp.first = sp.first + backward;
		return sp;
	}
	else
		return start_positions[index];
}
