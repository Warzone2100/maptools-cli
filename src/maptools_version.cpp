// Warzone 2100 MapTools
/*
	This file is part of Warzone 2100.
	Copyright (C) 2022  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "maptools_version.h"
#include <sstream>
#include <wzmaplib/map_version.h>
#include <nlohmann/json.hpp>
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
#include <zip.h>
#endif
#include <png.h>

#define stringify__(s) #s
#define stringify_(s) stringify__(s)

std::string generateMapToolsVersionInfo()
{
	std::stringstream versionInfo;
	versionInfo << "maptools " stringify_(MAPTOOLS_CLI_VERSION_MAJOR) "." stringify_(MAPTOOLS_CLI_VERSION_MINOR) "." stringify_(MAPTOOLS_CLI_VERSION_REV);
	versionInfo << " wzmaplib/" << WzMap::wzmaplib_version_string();
	versionInfo << " nlohmann-json/" << stringify_(NLOHMANN_JSON_VERSION_MAJOR) "." stringify_(NLOHMANN_JSON_VERSION_MINOR) "." stringify_(NLOHMANN_JSON_VERSION_PATCH);
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
	const char* verStr = zip_libzip_version();
	if (verStr)
	{
		versionInfo << " libzip/" << verStr;
	}
#endif
	versionInfo << " libpng/" << PNG_LIBPNG_VER_STRING;
	return versionInfo.str();
}

