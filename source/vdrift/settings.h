#pragma once
#include "configfile.h"


#define SET_VER  2203  // 2.2


enum eGraphType  {
	Gh_Fps=0,
	Gh_CarAccelG, Gh_BulletHit,
	Gh_Sound,
	Gh_TireSlips, Gh_Suspension,
	Gh_TireEdit,
	Gh_TorqueCurve, Gh_Engine,
	Gh_Clutch, Gh_Diffs, //todo: tire friction circles,
	Gh_ALL  };  // total count
const static std::string csGraphNames[Gh_ALL] = {
	"Fps graphics perf.",
	"Car Accel G's", "Car Hit chassis",
	"Sound volume & pan, wave",
	"Tires slip| & slide-", "Suspension pos & vel",
	"Tire Edit (Pacejka coeffs)*",
	"Torque Curve, gears", "Engine torque & power",
	"Clutch, Rpm, Gear", "Differentials"
	};

enum eShadowType  {  Sh_None=0, Sh_Depth, Sh_Soft  };

class SETTINGS
{
public:
//------------------------------------------
	int version;  // file version

	//  show
	bool show_fps,
		show_gauges, show_digits,
		trackmap, mini_zoomed, mini_rotated, mini_terrain, mini_border,
		check_beam, check_arrow,
		show_times, show_opponents, opplist_sort,
		show_cam, cam_tilt,
		car_dbgbars, car_dbgtxt, car_dbgsurf,
		car_tirevis, show_graphs;

	float size_gauges, size_minimap, size_minipos, size_arrow, zoom_minimap;
	int gauges_type, gauges_layout;
	bool cam_loop_chng;  int cam_in_loop;
	
	eGraphType graphs_type;
	int car_dbgtxtclr, car_dbgtxtcnt;
	//  gui
	bool tracks_sortup, cars_sortup, champ_info;
	int tracks_view, tracks_sort, cars_sort,
		tut_type, champ_type, chall_type, car_ed_tab;

	//  graphics
	int preset;  // last set, info only
	int anisotropy, tex_size, ter_mtr, ter_tripl;  bool bFog;
	float view_distance, terdetail, terdist, road_dist;
	float shadow_dist;  int shadow_size, lightmap_size, shadow_count, shadow_type; //eShadowType
	bool use_imposters, imposters_only;
	int refl_skip, refl_faces, refl_size;  float refl_dist;
	int refl_mode;  // 0 static, 1 single, 2 full
	bool water_reflect, water_refract;  int water_rttsize;
	bool particles, trails;  float grass, trees_dist, grass_dist;
	float particles_len, trails_len;
	bool boost_fov;

	std::string shader_mode;

	//---------------  car setup
	bool abs[2], tcs[2],  // [2] = 0 gravel 1 asphalt
		autoshift, autorear, rear_inv, show_mph;
	float sss_effect[2], sss_velfactor[2];
	//  steering range multipliers
	float steer_range[2],  // gravel/asphalt
		steer_sim[2];  // simulation modes  0 easy 1 normal
	std::vector<int> cam_view;  //[4]

	//---------------  game config
	class GameSet
	{
	public:
		std::string track;  bool track_user, trackreverse;
		std::vector<std::string> car;  //[4]
		std::vector<float> car_hue, car_sat, car_val, car_gloss, car_refl;  //[6] also for ghosts

		int local_players, num_laps;  // split
		//  game setup
		std::string sim_mode;
		bool collis_veget, collis_cars, collis_roadw, dyn_objects;
		int boost_type, flip_type, damage_type, rewind_type;  float boost_power;
		float trees, damage_dec;
		
		bool rpl_rec;
		//  champ
		int champ_num, chall_num;  // -1 none
		bool champ_rev;
		
		float pre_time;  int start_order;

		GameSet();
	}  game,  // current game, changed only on new game start
		gui;  // gui only config
	//---------------
	
	//  misc
	bool isMain;  int inMenu;  // last menu id
	bool dev_keys, dev_no_prvs;  // dev
	std::string language;
	bool split_vertically;
	
	//  startup, other
	bool autostart, escquit, startInMain;
	bool bltDebug, bltLines, bltProfilerTxt, profilerTxt;
	bool loadingbackground, ogre_dialog;
	bool mouse_capture;

	//  sound
	float vol_master, vol_engine, vol_tires, vol_susp, vol_env,
		vol_fl_splash,vol_fl_cont, vol_car_crash,vol_car_scrap;
	
	//  sim freq (1/interval timestep)
	float game_fq, blt_fq,  perf_speed;
	int blt_iter, dyn_iter,  multi_thr, thread_sleep;
	
	//  effects
	bool all_effects, bloom, blur, hdr;
	float bloom_int, bloom_orig, blur_int;  // intensity
	float dof_focus, dof_far;
	bool softparticles, ssao, godrays, dof, filmgrain;
	//  hdr
	float hdrBloomint, hdrBloomorig;
	float hdrParam1, hdrParam2, hdrParam3;
	float hdrAdaptationScale;
	float vignRadius, vignDarkness;

	//  screen
	int windowx, windowy, fsaa;
	bool fullscreen, vsync;
	std::string buffer, rendersystem;
	
	//  replay
	bool rpl_rec, rpl_ghost, rpl_bestonly;
	bool rpl_ghostother, rpl_trackghost;
	bool rpl_ghostpar, rpl_ghostrewind, rpl_listghosts;
	int rpl_listview, rpl_numViews;
	
	//  network
	std::string nickname, netGameName;
	std::string master_server_address;
	int master_server_port, local_port;

	// not in gui
	bool boostFromExhaust;  int net_local_plr;
	
//------------------------------------------
	SETTINGS();

	template <typename T>
	bool Param(CONFIGFILE & conf, bool write, std::string pname, T & value)
	{
		if (write)
		{	conf.SetParam(pname, value);
			return true;
		}else
			return conf.GetParam(pname, value);
	}
	void Serialize(bool write, CONFIGFILE & config);
	void Load(std::string sfile), Save(std::string sfile);
};
