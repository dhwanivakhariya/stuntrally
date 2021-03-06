#include "pch.h"
/// Big portions of this file are borrowed and adapted from Performous under GPL (http://performous.org)


#include "pathmanager.h"
#include <boost/filesystem.hpp>
#include <string>
#include <fstream>
#include <list>
#include <cassert>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

// Should come from CMake
#ifndef SHARED_DATA_DIR
#define SHARED_DATA_DIR "data"
#endif
// Optionally comes from CMake
#ifndef OGRE_PLUGIN_DIR
#define OGRE_PLUGIN_DIR ""
#endif


// TODO: Create a PORTABLE_INSTALL flag that allows disabling the usage of system dirs


// Define this useful alias for the overlong namespace name
namespace fs = boost::filesystem;

namespace {
	fs::path execname();
}


//  static vars
std::string PATHMANAGER::ogre_plugin, PATHMANAGER::home_dir,
	PATHMANAGER::user_config, PATHMANAGER::game_config,
	PATHMANAGER::user_data, PATHMANAGER::game_data,
	PATHMANAGER::cache_dir;


void PATHMANAGER::Init(std::ostream & info_output, std::ostream & error_output, bool log_paths)
{
	typedef std::vector<fs::path> Paths;

	// Set Ogre plugins dir
	{
		ogre_plugin = "";
		char *plugindir = getenv("OGRE_PLUGIN_DIR");
		if (plugindir) {
			ogre_plugin = plugindir;
		#ifndef _WIN32
		} else if (fs::exists(fs::path(OGRE_PLUGIN_DIR) / "RenderSystem_GL.so")) {
			ogre_plugin = OGRE_PLUGIN_DIR;
		#endif
		} else {
			#ifdef _WIN32
			ogre_plugin = ".";
			#else
			Paths dirs;
			#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(_M_X64)
			dirs.push_back("/usr/local/lib64");
			dirs.push_back("/usr/lib64");
			#else
			dirs.push_back("/usr/local/lib32");
			dirs.push_back("/usr/lib32");
			#endif
			dirs.push_back("/usr/local/lib");
			dirs.push_back("/usr/lib");
			// Loop through the paths and pick the first one that contain a plugin
			for (Paths::const_iterator p = dirs.begin(); p != dirs.end(); ++p) {
				if (fs::exists(*p / "OGRE/RenderSystem_GL.so")) {
					ogre_plugin = (*p / "OGRE").string();
					break;
				} else if (fs::exists(*p / "ogre/RenderSystem_GL.so")) {
					ogre_plugin = (*p / "ogre").string();
					break;
				}
			}
			#endif
		}
	}

	fs::path stuntrally = "stuntrally";
	// Figure out the user's home directory
	{
		home_dir = "";
		#ifndef _WIN32 // POSIX
			char *homedir = getenv("HOME");
			if (homedir == NULL)
			{
				home_dir = "/home/";
				homedir = getenv("USER");
				if (homedir == NULL) {
					homedir = getenv("USERNAME");
					if (homedir == NULL) {
						error_output << "Could not find user's home directory!" << std::endl;
						home_dir = "/tmp/";
					}
				}
			}
		#else // Windows
			char *homedir = getenv("USERPROFILE");
			if (homedir == NULL) homedir = "data"; // WIN 9x/Me
		#endif
		home_dir += homedir;
	}

	// Find user's config dir
	#ifndef _WIN32 // POSIX
	{
		char const* conf = getenv("XDG_CONFIG_HOME");
		if (conf) user_config = (fs::path(conf) / stuntrally).string();
		else user_config = (fs::path(home_dir) / ".config" / stuntrally).string();
	}
	#else // Windows
	{
		// Open AppData directory
		std::string str;
		ITEMIDLIST* pidl;
		char AppDir[MAX_PATH];
		HRESULT hRes = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE , &pidl);
		if (hRes == NOERROR)
		{
			SHGetPathFromIDList(pidl, AppDir);
			int i;
			for (i = 0; AppDir[i] != '\0'; i++) {
				if (AppDir[i] == '\\') str += '/';
				else str += AppDir[i];
			}
			user_config = (fs::path(str) / stuntrally).string();
		}
	}
	#endif
	// Create user's config dir
	CreateDir(user_config, error_output);

	// Find user's data dir (for additional data)
	#ifdef _WIN32
	user_data = user_config;  // APPDATA/stuntrally
	#else
	{
		char const* xdg_data_home = getenv("XDG_DATA_HOME");
		user_data = (xdg_data_home ? xdg_data_home / stuntrally
					: fs::path(home_dir) / ".local/share/games" / stuntrally).string();
	}
	#endif

	// Create user's data dir and its children
	///--------------------------------------------------
	CreateDir(user_data, error_output);
	CreateDir(Records(), error_output);
	CreateDir(Ghosts(), error_output);
	
	CreateDir(Replays(), error_output);
	CreateDir(Screenshots(), error_output);
	CreateDir(TracksUser(), error_output);  // user tracks

	CreateDir(DataUser(), error_output);  // user data


	// Find game data dir and defaults config dir
	char *datadir = getenv("STUNTRALLY_DATA_ROOT");
	if (datadir)
		game_data = std::string(datadir);
	else
	{	fs::path shareDir = SHARED_DATA_DIR;
		Paths dirs;

		// Adding users data dir
		// TODO: Disabled for now until this is handled properly
		//dirs.push_back(user_data_dir);

		// Adding relative path for running from sources
		dirs.push_back(execname().parent_path().parent_path() / "data");
		dirs.push_back(execname().parent_path().parent_path());
		dirs.push_back(execname().parent_path() / "data");
		dirs.push_back(execname().parent_path());
		// Adding relative path from installed executable
		dirs.push_back(execname().parent_path().parent_path() / shareDir);
		#ifndef _WIN32
		// Adding XDG_DATA_DIRS
		{
			char const* xdg_data_dirs = getenv("XDG_DATA_DIRS");
			std::istringstream iss(xdg_data_dirs ? xdg_data_dirs : "/usr/local/share/:/usr/share/");
			for (std::string p; std::getline(iss, p, ':'); dirs.push_back(p / stuntrally)) {}
		}
		#endif
		// TODO: Adding path from config file

		//  Loop through the paths and pick the first one that contain some data
		for (Paths::const_iterator p = dirs.begin(); p != dirs.end(); ++p)
		{	//  Data dir
			if (fs::exists(*p / "hud"))
				game_data = p->string();
			//  Config dir
			if (fs::exists(*p / "config"))
				game_config = (*p / "config").string();
			//  Check if both are found
			if (!game_data.empty() && !game_config.empty())  break;
		}
	}


	//  Subdirs for each sim_mode
	///--------------------------------------------------
	std::list <std::string> li;
	PATHMANAGER::DirList(PATHMANAGER::CarSim(), li);
	for (std::list <std::string>::iterator i = li.begin(); i != li.end(); ++i)
	{
		CreateDir(Records()+"/"+*i, error_output);
		CreateDir(Ghosts()+"/"+*i, error_output);
	}

	// Find cache dir
	#ifdef _WIN32
	cache_dir = user_config + "/cache";  // APPDATA/stuntrally/cache
	#else
	char const* xdg_cache_home = getenv("XDG_CACHE_HOME");
	cache_dir = (xdg_cache_home ? xdg_cache_home / stuntrally
				: fs::path(home_dir) / ".cache" / stuntrally).string();
	#endif
	// Create cache dir
	CreateDir(CacheDir(), error_output);
	CreateDir(ShaderDir(), error_output);

	// Print diagnostic info
	if (log_paths)
	{
		std::stringstream out;
		out << "--- Directories: ---" << std::endl;
		out << "Ogre plugin:  " << ogre_plugin << std::endl;
		out << "Data:         " << Data() << std::endl;
		//out << "Default cfg:  " << GetGameConfigDir() << std::endl;
		//out << "Home:         " << home_dir << std::endl;
		out << "User cfg,log: " << UserConfigDir() << std::endl;
		out << "User data:    " << user_data << std::endl;
		out << "Cache:        " << CacheDir() << std::endl;
		info_output << out.str();
	}
}

bool PATHMANAGER::FileExists(const std::string & filename)
{
	return fs::exists(filename);
}

bool PATHMANAGER::CreateDir(const std::string& path, std::ostream & error_output)
{
	try	{	fs::create_directories(path);	}
	catch (...)
	{
		error_output << "Could not create directory " << path << std::endl;
		return false;
	}
	return true;
}


// TODO: implement with boost::filesystem
bool PATHMANAGER::DirList(std::string dirpath, std::list <std::string> & dirlist, std::string extension)
{
//------Folder listing code for POSIX
#ifndef _WIN32
	DIR *dp;
	struct dirent *ep;
	dp = opendir(dirpath.c_str());
	if (dp != NULL)
	{
		while (ep = readdir(dp))
		{
			//puts (ep->d_name);
			std::string newname = ep->d_name;
			if (newname[0] != '.')
			{
				dirlist.push_back(newname);
			}
		}
		(void) closedir(dp);
	}
	else
		return false;
#else
//------Folder listing for WIN32
	HANDLE          hList;
	TCHAR           szDir[MAX_PATH+1];
	WIN32_FIND_DATA FileData;

	// Get the proper directory path
	sprintf(szDir, "%s\\*", dirpath.c_str());

	// Get the first file
	hList = FindFirstFile(szDir, &FileData);
	if (hList == INVALID_HANDLE_VALUE)
	{ 
		//no files found.  that's OK
	}
	else
	{
		// Traverse through the directory structure
		while (FindNextFile(hList, &FileData))
		{
			// Check the object is a directory or not
			if (FileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			{} else
			{
				if (FileData.cFileName[0] != '.')
				{
					dirlist.push_back(FileData.cFileName);
				}
			}
		}
	}

	FindClose(hList);
#endif
//------End
	
	//remove non-matcthing extensions
	if (!extension.empty())
	{
		std::list <std::list <std::string>::iterator> todel;
		for (std::list <std::string>::iterator i = dirlist.begin(); i != dirlist.end(); ++i)
		{
			if (i->find(extension) != i->length()-extension.length())
				todel.push_back(i);
		}
		
		for (std::list <std::list <std::string>::iterator>::iterator i = todel.begin(); i != todel.end(); ++i)
			dirlist.erase(*i);
	}
	
	dirlist.sort();
	return true;
}

namespace {
	/// Get the current executable name with path. Returns empty path if the name
	/// cannot be found. May return absolute or relative paths.
	#if defined(_WIN32)
	#include <windows.h>
	fs::path execname() {
		char buf[1024];
		DWORD ret = GetModuleFileName(NULL, buf, sizeof(buf));
		if (ret == 0 || ret == sizeof(buf)) return fs::path();
		return buf;
	}
	#elif defined(__APPLE__)
	#include <mach-o/dyld.h>
	fs::path execname() {
		char buf[1024];
		uint32_t size = sizeof(buf);
		int ret = _NSGetExecutablePath(buf, &size);
		if (ret != 0) return fs::path();
		return buf;
	}
	#elif defined(sun) || defined(__sun)
	#include <stdlib.h>
	fs::path execname() {
		return getexecname();
	}
	#elif defined(__FreeBSD__)
	#include <sys/sysctl.h>
	fs::path execname() {
		int mib[4];
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PATHNAME;
		mib[3] = -1;
		char buf[1024];
		size_t maxchars = sizeof(buf) - 1;
		size_t size = maxchars;
		sysctl(mib, 4, buf, &size, NULL, 0);
		if (size == 0 || size >= maxchars) return fs::path();
		buf[size] = '\0';
		return buf;
	}
	#elif defined(__linux__)
	#include <unistd.h>
	fs::path execname() {
		char buf[1024];
		ssize_t maxchars = sizeof(buf) - 1;
		ssize_t size = readlink("/proc/self/exe", buf, sizeof(buf));
		if (size <= 0 || size >= maxchars) return fs::path();
		buf[size] = '\0';
		return buf;
	}
	#else
	fs::path execname() {
		return fs::path();
	}
	#endif
}
