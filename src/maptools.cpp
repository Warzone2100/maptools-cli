// Warzone 2100 MapTools
/*
	This file is part of Warzone 2100.
	Copyright (C) 2021  Warzone 2100 Project

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

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#endif
#include <CLI11/CLI11.hpp>
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
#include <wzmaplib/map.h>
#include <wzmaplib/map_preview.h>
#include <wzmaplib/map_package.h>
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
#include <ZipIOProvider.h>
#endif
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include "pngsave.h"
#include "maptools_version.h"

class MapToolDebugLogger : public WzMap::LoggingProtocol
{
public:
	MapToolDebugLogger(bool verbose)
	: verbose(verbose)
	{ }
	virtual ~MapToolDebugLogger() { }
	virtual void printLog(WzMap::LoggingProtocol::LogLevel level, const char *function, int line, const char *str) override
	{
		std::ostream* pOutputStream = &(std::cout);
		if (level == WzMap::LoggingProtocol::LogLevel::Error)
		{
			pOutputStream = &(std::cerr);
		}
		std::string levelStr;
		switch (level)
		{
			case WzMap::LoggingProtocol::LogLevel::Info_Verbose:
			case WzMap::LoggingProtocol::LogLevel::Info:
				if (!verbose) { return; }
				levelStr = "INFO";
				break;
			case WzMap::LoggingProtocol::LogLevel::Warning:
				levelStr = "WARNING";
				break;
			case WzMap::LoggingProtocol::LogLevel::Error:
				levelStr = "ERROR";
				break;
		}
		(*pOutputStream) << levelStr << ": [" << function << ":" << line << "] " << str << std::endl;
	}
private:
	bool verbose = false;
};

static optional<WzMap::MapPreviewColor> convertHexColorToPreviewColor(const std::string& input)
{
	if (input.empty())
	{
		return nullopt;
	}

	size_t startingIdx = 0;
	if (input.front() == '#')
	{
		startingIdx = 1;
	}

	if ((input.size() - 1) % 2 != 0)
	{
		return nullopt;
	}

	// split into 2-character hex components
	std::vector<uint8_t> hexComponents;
	for (size_t i = startingIdx; i + 1 < input.size(); i += 2)
	{
		try {
			int componentInt = std::stoi(input.substr(i, 2), 0, 16);
			hexComponents.push_back(static_cast<uint8_t>(componentInt));
		}
		catch (const std::invalid_argument& e) {
			return nullopt;
		}
		catch (const std::out_of_range& e) {
			return nullopt;
		}
	}

	if (hexComponents.size() < 3 || hexComponents.size() > 4)
	{
		return nullopt;
	}

	uint8_t alphaComponent = 255;
	if (hexComponents.size() > 3)
	{
		alphaComponent = hexComponents[3];
	}
	return WzMap::MapPreviewColor{hexComponents[0], hexComponents[1], hexComponents[2], alphaComponent};
}

// Defining operator<<() for enum classes to override CLI11's enum streaming
namespace WzMap {
inline std::ostream &operator<<(std::ostream &os, const MapType& mapType) {
	switch(mapType) {
	case WzMap::MapType::CAMPAIGN:
		os << "Campaign";
		break;
	case WzMap::MapType::SAVEGAME:
		os << "Savegame";
		break;
	case WzMap::MapType::SKIRMISH:
		os << "Skirmish";
		break;
	}
	return os;
}
inline std::ostream &operator<<(std::ostream &os, const OutputFormat& outputFormat) {
	switch(outputFormat) {
	case WzMap::OutputFormat::VER1_BINARY_OLD:
		os << "Binary .BJO (flaME-compatible / old)";
		break;
	case WzMap::OutputFormat::VER2:
		os << "JSONv1 (WZ 3.4+)";
		break;
	case WzMap::OutputFormat::VER3:
		os << "JSONv2 (WZ 4.1+)";
		break;
	}
	return os;
}
inline std::ostream &operator<<(std::ostream &os, const Map::LoadedFormat& mapFormat) {
	switch(mapFormat) {
	case WzMap::Map::LoadedFormat::MIXED:
		os << "Mixed Formats";
		break;
	case WzMap::Map::LoadedFormat::BINARY_OLD:
		os << "Binary .BJO (old)";
		break;
	case WzMap::Map::LoadedFormat::JSON_v1:
		os << "JSONv1 (WZ 3.4+)";
		break;
	case WzMap::Map::LoadedFormat::SCRIPT_GENERATED:
		os << "Script-Generated (WZ 4.0+)";
		break;
	case WzMap::Map::LoadedFormat::JSON_v2:
		os << "JSONv2 (WZ 4.1+)";
		break;
	}
	return os;
}
inline std::ostream &operator<<(std::ostream &os, const LevelFormat& levelFormat) {
	switch(levelFormat) {
	case WzMap::LevelFormat::LEV:
		os << "LEV (flaME-compatible / old)";
		break;
	case WzMap::LevelFormat::JSON:
		os << "JSON level file (WZ 4.3+)";
		break;
	}
	return os;
}
bool lexical_cast(const std::string &input, MapPreviewColor &output) {
	auto converted = ::convertHexColorToPreviewColor(input);
	if (!converted.has_value())
	{
		return false;
	}
	output = converted.value();
	return true;
}
bool set_draw_options_from_strval(const std::string& input, MapPreviewColorScheme::DrawOptions& output, bool val = true)
{
	if (input.compare("terrain") == 0)
	{
		output.drawTerrain = val;
	}
	else if (input.compare("structures") == 0)
	{
		output.drawStructures = val;
	}
	else if (input.compare("oil") == 0)
	{
		output.drawOil = val;
	}
	else
	{
		return false;
	}

	return true;
}
bool lexical_cast(const std::string &input, MapPreviewColorScheme::DrawOptions &output)
{
	bool hasResetOutput = false;
	if (input.compare("all") == 0)
	{
		output.set(true);
		return true;
	}
	for (const auto &var : CLI::detail::split(input, ','))
	{
		if (var.empty()) { continue; }
		if (!hasResetOutput)
		{
			output.set(false);
			hasResetOutput = true;
		}
		if (!set_draw_options_from_strval(var, output, true))
		{
			return false;
		}
	}
	return true;
}
} // namespace WzMap


static bool convertMapPackage(const std::string& mapPackageContentsPath, const std::string& outputPath, WzMap::LevelFormat levelFormat, WzMap::OutputFormat outputFormat, uint32_t mapSeed, bool copyAdditionalFiles, bool verbose, bool exportUncompressed, bool fixedLastMod, optional<std::string> override_map_name = nullopt, std::shared_ptr<WzMap::IOProvider> mapIO = std::shared_ptr<WzMap::IOProvider>(new WzMap::StdIOProvider()))
{
	auto logger = std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(verbose));

	auto wzMapPackage = WzMap::MapPackage::loadPackage(mapPackageContentsPath, logger, mapIO);
	if (!wzMapPackage)
	{
		std::cerr << "Failed to load map archive package from: " << mapPackageContentsPath << std::endl;
		return false;
	}

	auto wzMap = wzMapPackage->loadMap(mapSeed, logger);
	if (!wzMap)
	{
		// Failed to load map
		std::cerr << "Failed to load map from map archive path: " << mapPackageContentsPath << std::endl;
		return false;
	}

	std::string outputBasePath;
	std::shared_ptr<WzMap::IOProvider> exportIO;
	if (exportUncompressed)
	{
		exportIO = std::make_shared<WzMap::StdIOProvider>();
		outputBasePath = outputPath;
		if (!outputBasePath.empty())
		{
			if (!exportIO->makeDirectory(outputBasePath))
			{
				std::cerr << "Failed to create / verify destination directory: " << outputBasePath << std::endl;
				return false;
			}
		}
	}
	else
	{
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
		exportIO = WzMapZipIO::createZipArchiveFS(outputPath.c_str(), fixedLastMod);
		if (!exportIO)
		{
			std::cerr << "Failed to open map archive file for output: " << outputPath << std::endl;
			return false;
		}
		outputBasePath.clear();
#else
		std::cerr << "maptools was not compiled with map archive (.wz) support - you must pass --output-uncompressed" << std::endl;
		return false;
#endif // !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
	}

	if (override_map_name.has_value())
	{
		WzMap::LevelDetails modifiedLevelDetails = wzMapPackage->levelDetails();
		modifiedLevelDetails.name = override_map_name.value();
		wzMapPackage->updateLevelDetails(modifiedLevelDetails);
	}

	if (!wzMapPackage->exportMapPackageFiles(outputBasePath, levelFormat, outputFormat, nullopt, copyAdditionalFiles, logger, exportIO))
	{
		// Failed to export map package
		std::cerr << "Failed to export map package to: " << outputPath << std::endl;
		return false;
	}

	auto inputMapFormat = wzMap->loadedMapFormat();

	std::cout << "Converted map package:" << std::endl
			<< "\t - from format [";
	if (inputMapFormat.has_value())
	{
		std::cout << inputMapFormat.value();
	}
	else
	{
		std::cout << "unknown";
	}
	std::cout << "] -> [" << outputFormat << "]" << std::endl;
	std::cout << "\t - with: " << levelFormat << std::endl;
	std::cout << "\t - saved to: " << outputPath << std::endl;

	return true;
}

#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
static bool convertMapPackage_FromArchive(const std::string& mapArchive, const std::string& outputPath, WzMap::LevelFormat levelFormat, WzMap::OutputFormat outputFormat, uint32_t mapSeed, bool copyAdditionalFiles, bool verbose, bool outputUncompressed, bool fixedLastMod, optional<std::string> override_map_name)
{
	auto zipArchive = WzMapZipIO::openZipArchiveFS(mapArchive.c_str());
	if (!zipArchive)
	{
		std::cerr << "Failed to open map archive file: " << mapArchive << std::endl;
		return false;
	}

	return convertMapPackage("", outputPath, levelFormat, outputFormat, mapSeed, copyAdditionalFiles, verbose, outputUncompressed, fixedLastMod, override_map_name, zipArchive);
}
#endif // !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)

static bool convertMap(WzMap::MapType mapType, uint32_t mapMaxPlayers, const std::string& inputMapDirectory, const std::string& outputMapDirectory, WzMap::OutputFormat outputFormat, uint32_t mapSeed, bool verbose)
{
	auto wzMap = WzMap::Map::loadFromPath(inputMapDirectory, mapType, mapMaxPlayers, mapSeed, std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(verbose)));
	if (!wzMap)
	{
		// Failed to load map
		std::cerr << "Failed to load map: " << inputMapDirectory << std::endl;
		return false;
	}

	if (!wzMap->exportMapToPath(*(wzMap.get()), outputMapDirectory, mapType, mapMaxPlayers, outputFormat, std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(verbose))))
	{
		// Failed to export map
		std::cerr << "Failed to export map to: " << outputMapDirectory << std::endl;
		return false;
	}

	auto inputMapFormat = wzMap->loadedMapFormat();

	std::cout << "Converted map:\n"
			<< "\t - from format [";
	if (inputMapFormat.has_value())
	{
		std::cout << inputMapFormat.value();
	}
	else
	{
		std::cout << "unknown";
	}
	std::cout << "] -> [" << outputFormat << "]\n";
	std::cout << "\t - saved to: " << outputMapDirectory << std::endl;

	return true;
}

static optional<MAP_TILESET> guessMapTileset(WzMap::Map& wzMap)
{
	auto pTerrainTypes = wzMap.mapTerrainTypes();
	if (!pTerrainTypes)
	{
		return nullopt;
	}
	auto& terrainTypes = pTerrainTypes->terrainTypes;
	if (terrainTypes.size() >= 3)
	{
		if (terrainTypes[0] == 1 && terrainTypes[1] == 0 && terrainTypes[2] == 2)
		{
			return MAP_TILESET::ARIZONA;
		}
		else if (terrainTypes[0] == 2 && terrainTypes[1] == 2 && terrainTypes[2] == 2)
		{
			return MAP_TILESET::URBAN;
		}
		else if (terrainTypes[0] == 0 && terrainTypes[1] == 0 && terrainTypes[2] == 2)
		{
			return MAP_TILESET::ROCKIES;
		}
		else
		{
			std::cerr << "Unknown terrain types signature: " << terrainTypes[0] << terrainTypes[1] << terrainTypes[2] << "; defaulting to Arizona tilset." << std::endl;
			return MAP_TILESET::ARIZONA;
		}
	}
	else
	{
		std::cerr << "Unknown terrain types; defaulting to Arizona tilset." << std::endl;
		return MAP_TILESET::ARIZONA;
	}
}

// Maroon
// (this should not conflict with other standard player colors, and should be fairly easy to distinguish from terrain tile colors on all tilesets)
constexpr WzMap::MapPreviewColor ScavsColorDefault = { 128, 0, 0, 255 };

class MapToolsPreviewSimplePlayerColorProvider : public WzMap::MapPlayerColorProvider
{
public:
	MapToolsPreviewSimplePlayerColorProvider(WzMap::MapPreviewColor scavsColor)
	: scavsColor(scavsColor)
	{ }
	~MapToolsPreviewSimplePlayerColorProvider() { }

	// -1 = scavs
	virtual WzMap::MapPreviewColor getPlayerColor(int8_t mapPlayer) override
	{
		if (mapPlayer == PLAYER_SCAVENGERS)
		{
			return scavsColor;
		}
		return {0, 255, 2, 255}; // default: bright green
	}
private:
	WzMap::MapPreviewColor scavsColor;
};

class MapToolsPreviewVariedPlayerColorProvider : public WzMap::MapPlayerColorProvider
{
public:
	MapToolsPreviewVariedPlayerColorProvider(WzMap::MapPreviewColor scavsColor)
	: scavsColor(scavsColor)
	{ }
	~MapToolsPreviewVariedPlayerColorProvider() { }

	// -1 = scavs
	virtual WzMap::MapPreviewColor getPlayerColor(int8_t mapPlayer) override
	{
		if (mapPlayer == PLAYER_SCAVENGERS)
		{
			// Maroon
			return scavsColor;
		}
		if (mapPlayer >= maxClanColours)
		{
			// out of bounds
			return {0, 0, 0, 255};
		}
		return clanColours[mapPlayer];
	}
private:
	WzMap::MapPreviewColor scavsColor;
	static constexpr size_t maxClanColours = 16;
	WzMap::MapPreviewColor clanColours[maxClanColours] =
	{
		// NOTE: These do not exactly match the default *order* in WZ (which is randomized when starting a new skirmish game anyway),
		// but they do match the colors used
		// [r,g,b,a]
		{0, 255, 0, 255},		// green  Player 0
		{255, 192, 40, 255},          // orange Player 1
//		{255, 255, 255, 255},	// grey   Player 2
		{55, 55, 55, 255},			// black  Player 3	// NOTE: To match WZ, this is actually set to a dark grey color in map previews to be more visible
		{255, 0, 0, 255},		// red    Player 4
		{20, 20, 255, 255},		// blue   Player 5
		{255, 0, 192, 255},           // pink   Player 6
		{0, 255, 255, 255},		// cyan   Player 7
		{255, 255, 0, 255},           // yellow Player 8
		{144, 0, 255, 255},           // purple Player 9
		{255, 255, 255, 255},	// grey   Player 2
		{200, 255, 255, 255},         // white  Player A (Should be brighter than grey, but grey is already maximum.)
		{128, 128, 255, 255},         // bright blue Player B
		{128, 255, 128, 255},         // neon green  Player C
		{128, 0, 0, 255},             // infrared    Player D
		{64, 0, 128, 255},            // ultraviolet Player E
		{128, 128, 0, 255},           // brown       Player F
	};
};

enum class MapToolsPreviewColorProvider
{
	Simple,
	WZPlayerColors
};

static std::unique_ptr<WzMap::MapPreviewImage> generateMapPreview_FromMapObject_Impl(WzMap::Map& map, MapToolsPreviewColorProvider playerColorProvider, WzMap::MapPreviewColor scavsColor, const WzMap::MapPreviewColorScheme::DrawOptions& drawOptions, const WzMap::LevelDetails &levelDetails)
{
	WzMap::MapPreviewColorScheme previewColorScheme;
	previewColorScheme.hqColor = {255, 0, 255, 255};
	previewColorScheme.oilResourceColor = {255, 255, 0, 255};
	previewColorScheme.oilBarrelColor = {128, 192, 0, 255};
	switch (playerColorProvider)
	{
		case MapToolsPreviewColorProvider::Simple:
			previewColorScheme.playerColorProvider = std::unique_ptr<WzMap::MapPlayerColorProvider>(new MapToolsPreviewSimplePlayerColorProvider(scavsColor));
			break;
		case MapToolsPreviewColorProvider::WZPlayerColors:
			previewColorScheme.playerColorProvider = std::unique_ptr<WzMap::MapPlayerColorProvider>(new MapToolsPreviewVariedPlayerColorProvider(scavsColor));
			break;
	}
	switch (levelDetails.tileset)
	{
	case MAP_TILESET::ARIZONA:
		previewColorScheme.tilesetColors = WzMap::TilesetColorScheme::TilesetArizona();
		break;
	case MAP_TILESET::URBAN:
		previewColorScheme.tilesetColors = WzMap::TilesetColorScheme::TilesetUrban();
		break;
	case MAP_TILESET::ROCKIES:
		previewColorScheme.tilesetColors = WzMap::TilesetColorScheme::TilesetRockies();
		break;
	}
	previewColorScheme.drawOptions = drawOptions;

	return WzMap::generate2DMapPreview(map, previewColorScheme, WzMap::MapStatsConfiguration(levelDetails.type));
}

static bool generateMapPreviewPNG_FromMapObject(WzMap::Map& map, const std::string& outputPNGPath, MapToolsPreviewColorProvider playerColorProvider, WzMap::MapPreviewColor scavsColor, const WzMap::MapPreviewColorScheme::DrawOptions& drawOptions, const WzMap::LevelDetails &levelDetails)
{
	auto previewResult = generateMapPreview_FromMapObject_Impl(map, playerColorProvider, scavsColor, drawOptions, levelDetails);
	if (!previewResult)
	{
		std::cerr << "Failed to generate map preview" << std::endl;
		return false;
	}

	if (!savePng(outputPNGPath.c_str(), previewResult->imageData.data(), static_cast<int>(previewResult->width), static_cast<int>(previewResult->height)))
	{
		std::cerr << "Failed to save preview PNG" << std::endl;
		return false;
	}

	std::cout << "Generated map preview:\n"
			<< "\t - saved to: " << outputPNGPath << std::endl;

	return true;
}

static bool generateMapPreviewPNG_FromPackageContents(const std::string& mapPackageContentsPath, const std::string& outputPNGPath, MapToolsPreviewColorProvider playerColorProvider, WzMap::MapPreviewColor scavsColor, const WzMap::MapPreviewColorScheme::DrawOptions& drawOptions, uint32_t mapSeed, bool verbose, std::shared_ptr<WzMap::IOProvider> mapIO = std::shared_ptr<WzMap::IOProvider>(new WzMap::StdIOProvider()))
{
	auto logger = std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(verbose));

	auto wzMapPackage = WzMap::MapPackage::loadPackage(mapPackageContentsPath, logger, mapIO);
	if (!wzMapPackage)
	{
		std::cerr << "Failed to load map archive package from: " << mapPackageContentsPath << std::endl;
		return false;
	}

	auto wzMap = wzMapPackage->loadMap(mapSeed, logger);
	if (!wzMap)
	{
		// Failed to load map
		std::cerr << "Failed to load map from map archive path: " << mapPackageContentsPath << std::endl;
		return false;
	}

	return generateMapPreviewPNG_FromMapObject(*(wzMap.get()), outputPNGPath, playerColorProvider, scavsColor, drawOptions, wzMapPackage->levelDetails());
}

#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
static bool generateMapPreviewPNG_FromArchive(const std::string& mapArchive, const std::string& outputPNGPath, MapToolsPreviewColorProvider playerColorProvider, WzMap::MapPreviewColor scavsColor, const WzMap::MapPreviewColorScheme::DrawOptions& drawOptions, uint32_t mapSeed, bool verbose)
{
	auto zipArchive = WzMapZipIO::openZipArchiveFS(mapArchive.c_str());
	if (!zipArchive)
	{
		std::cerr << "Failed to open map archive file: " << mapArchive << std::endl;
		return false;
	}

	return generateMapPreviewPNG_FromPackageContents("", outputPNGPath, playerColorProvider, scavsColor, drawOptions, mapSeed, verbose, zipArchive);
}
#endif // !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)

static bool generateMapPreviewPNG_FromMapDirectory(WzMap::MapType mapType, uint32_t mapMaxPlayers, const std::string& inputMapDirectory, const std::string& outputPNGPath, MapToolsPreviewColorProvider playerColorProvider, WzMap::MapPreviewColor scavsColor, const WzMap::MapPreviewColorScheme::DrawOptions& drawOptions, uint32_t mapSeed, bool verbose)
{
	auto wzMap = WzMap::Map::loadFromPath(inputMapDirectory, mapType, mapMaxPlayers, mapSeed, std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(verbose)));
	if (!wzMap)
	{
		// Failed to load map
		std::cerr << "Failed to load map: " << inputMapDirectory << std::endl;
		return false;
	}

	WzMap::LevelDetails synthesizedLevelDetails;
	synthesizedLevelDetails.name = "";
	synthesizedLevelDetails.type = mapType;
	synthesizedLevelDetails.players = mapMaxPlayers;
	optional<MAP_TILESET> mapTilesetResult = guessMapTileset(*wzMap.get());
	if (!mapTilesetResult.has_value())
	{
		// Failed to guess the map tilset - presumably an error loading the map
		std::cerr << "Failed to guess map tilset" << std::endl;
		return false;
	}
	synthesizedLevelDetails.tileset = mapTilesetResult.value();
	synthesizedLevelDetails.mapFolderPath = "";

	return generateMapPreviewPNG_FromMapObject(*(wzMap.get()), outputPNGPath, playerColorProvider, scavsColor, drawOptions, synthesizedLevelDetails);
}

namespace nlohmann {
	template<>
	struct adl_serializer<WzMap::MapStats::PerPlayerCounts::MinMax> {
		static void to_json(ordered_json& j, const WzMap::MapStats::PerPlayerCounts::MinMax& p) {
			j = nlohmann::ordered_json::object();
			j["min"] = p.min;
			j["max"] = p.max;
		}

		static void from_json(const ordered_json& j, WzMap::MapStats::PerPlayerCounts::MinMax& p) {
			if (j.is_object())
			{
				p.min = j["min"].get<uint32_t>();
				p.max = j["max"].get<uint32_t>();
			}
		}
	};
}

static nlohmann::ordered_json generateMapInfoJSON_FromMapStats(const WzMap::LevelDetails details, const WzMap::MapStats& stats, std::shared_ptr<MapToolDebugLogger> logger)
{
	nlohmann::ordered_json output = nlohmann::ordered_json::object();

	// Level Details
	output["name"] = details.name;
	output["type"] = WzMap::to_string(details.type);
	output["players"] = details.players;
	output["tileset"] = WzMap::to_string(details.tileset);
	if (!details.author.empty())
	{
		nlohmann::ordered_json authorinfo = nlohmann::ordered_json::object();
		authorinfo["name"] = details.author;
		output["author"] = authorinfo;
	}
	else
	{
//		debug(pCustomLogger, LOG_WARNING, "LevelDetails is missing a valid author");
	}
	if (!details.additionalAuthors.empty())
	{
		nlohmann::ordered_json otherauthorsinfo = nlohmann::ordered_json::array();
		for (const auto& author : details.additionalAuthors)
		{
			nlohmann::ordered_json authorinfo = nlohmann::ordered_json::object();
			authorinfo["name"] = author;
			otherauthorsinfo.push_back(authorinfo);
		}
		output["additionalAuthors"] = otherauthorsinfo;
	}
	if (!details.license.empty())
	{
		output["license"] = details.license;
	}
	else
	{
//		debug(pCustomLogger, LOG_WARNING, "LevelDetails is missing a valid license");
	}
	if (!details.createdDate.empty())
	{
		output["created"] = details.createdDate;
	}
	else
	{
//		debug(pCustomLogger, LOG_WARNING, "LevelDetails is missing a created-date");
	}
	if (details.generator.has_value() && !details.generator.value().empty())
	{
		output["generator"] = details.generator.value();
	}

	// Map Stats
	auto mapsize = nlohmann::ordered_json::object();
	mapsize["w"] = stats.mapWidth;
	mapsize["h"] = stats.mapHeight;
	output["mapsize"] = std::move(mapsize);
	auto scavengerCounts = nlohmann::ordered_json::object();
	scavengerCounts["units"] = stats.scavengerUnits;
	scavengerCounts["structures"] = stats.scavengerStructs;
	scavengerCounts["factories"] = stats.scavengerFactories;
	scavengerCounts["resourceExtractors"] = stats.scavengerResourceExtractors;
	output["scavenger"] = std::move(scavengerCounts);
	output["oilWells"] = stats.oilWellsTotal;
	auto perPlayerCounts = nlohmann::ordered_json::object();
	perPlayerCounts["units"] = stats.perPlayerCounts.unitsPerPlayer;
	perPlayerCounts["structures"] = stats.perPlayerCounts.structuresPerPlayer;
	perPlayerCounts["resourceExtractors"] = stats.perPlayerCounts.resourceExtractorsPerPlayer;
	perPlayerCounts["powerGenerators"] = stats.perPlayerCounts.powerGeneratorsPerPlayer;
	perPlayerCounts["regFactories"] = stats.perPlayerCounts.regFactoriesPerPlayer;
	perPlayerCounts["vtolFactories"] = stats.perPlayerCounts.vtolFactoriesPerPlayer;
	perPlayerCounts["cyborgFactories"] = stats.perPlayerCounts.cyborgFactoriesPerPlayer;
	perPlayerCounts["researchCenters"] = stats.perPlayerCounts.researchCentersPerPlayer;
	perPlayerCounts["defenseStructures"] = stats.perPlayerCounts.defenseStructuresPerPlayer;
	output["player"] = std::move(perPlayerCounts);
	auto startEquality = nlohmann::ordered_json::object();
	startEquality["units"] = stats.playerBalance.units;
	startEquality["structures"] = stats.playerBalance.structures;
	startEquality["resourceExtractors"] = stats.playerBalance.resourceExtractors;
	startEquality["powerGenerators"] = stats.playerBalance.powerGenerators;
	startEquality["factories"] = stats.playerBalance.factories;
	startEquality["regFactories"] = stats.playerBalance.regFactories;
	startEquality["vtolFactories"] = stats.playerBalance.vtolFactories;
	startEquality["cyborgFactories"] = stats.playerBalance.cyborgFactories;
	startEquality["researchCenters"] = stats.playerBalance.researchCenters;
	startEquality["defenseStructures"] = stats.playerBalance.defenseStructures;
	auto balance = nlohmann::ordered_json::object();
	balance["startEquality"] = std::move(startEquality);
	output["balance"] = std::move(balance);
	auto playerHQPositions = nlohmann::ordered_json::array();
	for (uint8_t playerIdx = 0; playerIdx < details.players; ++playerIdx)
	{
		auto it = stats.playerHQPositions.find(playerIdx);
		auto hqPos = nlohmann::ordered_json::object();
		if (it != stats.playerHQPositions.end() && !it->second.empty())
		{
			hqPos["x"] = it->second.back().first;
			hqPos["y"] = it->second.back().second;
		}
		playerHQPositions.push_back(hqPos);
	}
	output["hq"] = playerHQPositions;

	return output;
}

inline std::string loadedFormatToString(optional<WzMap::Map::LoadedFormat> mapFormat)
{
	if (!mapFormat.has_value())
	{
		return "unknown";
	}
	switch(mapFormat.value())
	{
	case WzMap::Map::LoadedFormat::MIXED:
		return "mixed";
	case WzMap::Map::LoadedFormat::BINARY_OLD:
		return "binary";
	case WzMap::Map::LoadedFormat::JSON_v1:
		return "jsonv1";
	case WzMap::Map::LoadedFormat::SCRIPT_GENERATED:
		return "script";
	case WzMap::Map::LoadedFormat::JSON_v2:
		return "jsonv2";
	}
	return ""; // silence warning
}

inline std::string levelFormatToString(optional<WzMap::LevelFormat> levelFormat)
{
	if (!levelFormat.has_value())
	{
		return "";
	}
	switch(levelFormat.value())
	{
		case WzMap::LevelFormat::LEV:
			return "lev";
		case WzMap::LevelFormat::JSON:
			return "json";
	}
	return ""; // silence warning
}

static nlohmann::ordered_json generateMapInfoJSON_FromPackage(WzMap::MapPackage& mapPackage, const WzMap::MapStats& stats, std::shared_ptr<MapToolDebugLogger> logger)
{
	nlohmann::ordered_json output = generateMapInfoJSON_FromMapStats(mapPackage.levelDetails(),stats, logger);

	// Whether the map package is a "map mod"
	output["mapMod"] = static_cast<bool>(mapPackage.packageType() == WzMap::MapPackage::MapPackageType::Map_Mod);
	// Modification types (for map mods)
	nlohmann::ordered_json modTypes = nlohmann::ordered_json::object();
	bool anyModTypes = mapPackage.modTypesEnumerate([&modTypes](WzMap::MapPackage::ModTypes type) {
		modTypes[WzMap::MapPackage::to_string(type)] = true;
	});
	if (anyModTypes)
	{
		output["modTypes"] = modTypes;
	}
	// The loaded level details format
	auto levelFormat = mapPackage.loadedLevelDetailsFormat();
	if (levelFormat.has_value())
	{
		output["levelFormat"] = levelFormatToString(levelFormat);
	}
	else
	{
		std::cerr << "Loaded level details format is missing ??" << std::endl;
	}
	// The loaded map format
	auto pMap = mapPackage.loadMap(0);
	if (pMap)
	{
		auto loadedMapFormat = pMap->loadedMapFormat();
		output["mapFormat"] = loadedFormatToString(loadedMapFormat);
	}
	else
	{
		std::cerr << "Failed to load map from archive package ??" << std::endl;
	}
	// Whether the map package is a new "flat" map package
	output["flatMapPackage"] = mapPackage.isFlatMapPackage();

	return output;
}

static optional<nlohmann::ordered_json> generateMapInfoJSON_FromPackageContents(const std::string& mapPackageContentsPath, uint32_t mapSeed, std::shared_ptr<MapToolDebugLogger> logger, std::shared_ptr<WzMap::IOProvider> mapIO = std::shared_ptr<WzMap::IOProvider>(new WzMap::StdIOProvider()))
{
	auto wzMapPackage = WzMap::MapPackage::loadPackage(mapPackageContentsPath, logger, mapIO);
	if (!wzMapPackage)
	{
		std::cerr << "Failed to load map archive package from: " << mapPackageContentsPath << std::endl;
		return nullopt;
	}

	auto mapStatsResult = wzMapPackage->calculateMapStats(mapSeed);
	if (!mapStatsResult.has_value())
	{
		std::cerr << "Failed to calculate map info / stats from: " << mapPackageContentsPath << std::endl;
		return nullopt;
	}

	return generateMapInfoJSON_FromPackage(*(wzMapPackage.get()), mapStatsResult.value(), logger);
}

#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
static optional<nlohmann::ordered_json> generateMapInfoJSON_FromArchive(const std::string& mapArchive, uint32_t mapSeed, std::shared_ptr<MapToolDebugLogger> logger)
{
	auto zipArchive = WzMapZipIO::openZipArchiveFS(mapArchive.c_str());
	if (!zipArchive)
	{
		std::cerr << "Failed to open map archive file: " << mapArchive << std::endl;
		return nullopt;
	}

	return generateMapInfoJSON_FromPackageContents("", mapSeed, logger, zipArchive);
}
#endif // !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)

// specify string->value mappings
static const std::map<std::string, WzMap::MapType> maptype_map{{"skirmish", WzMap::MapType::SKIRMISH}, {"campaign", WzMap::MapType::CAMPAIGN}};
static const std::map<std::string, WzMap::LevelFormat> levelformat_map{{"latest", WzMap::LatestLevelFormat}, {"json", WzMap::LevelFormat::JSON}, {"lev", WzMap::LevelFormat::LEV}};
static const std::map<std::string, WzMap::OutputFormat> outputformat_map{{"latest", WzMap::LatestOutputFormat}, {"jsonv2", WzMap::OutputFormat::VER3}, {"json", WzMap::OutputFormat::VER2}, {"bjo", WzMap::OutputFormat::VER1_BINARY_OLD}};
static const std::map<std::string, MapToolsPreviewColorProvider> previewcolors_map{{"simple", MapToolsPreviewColorProvider::Simple}, {"wz", MapToolsPreviewColorProvider::WZPlayerColors}};

static bool strEndsWith(const std::string& str, const std::string& suffix)
{
	return (str.size() >= suffix.size()) && (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

/// Check for a specified file extension
class FileExtensionValidator : public CLI::Validator {
  public:
	FileExtensionValidator(std::string fileExtension) {
		std::stringstream out;
		out << "FILE(*" << fileExtension << ")";
		description(out.str());

		if (!fileExtension.empty())
		{
			if (fileExtension.front() != '.')
			{
				fileExtension = "." + fileExtension;
			}
		}

		func_ = [fileExtension](std::string &filename) {
			if (!strEndsWith(filename, fileExtension))
			{
				return "Filename does not end in extension: " + fileExtension;
			}
			return std::string();
		};
	}
};

class AsHexColorValue : public CLI::Validator {
  public:
	explicit AsHexColorValue() {
		description("RGB hexadecimal color code");

		// transform function
		func_ = [](std::string &input) -> std::string {

			CLI::detail::rtrim(input);
			if (input.empty()) {
				throw CLI::ValidationError("Input is empty");
			}

			auto convertedValue = convertHexColorToPreviewColor(input);
			if (!convertedValue.has_value())
			{
				throw CLI::ValidationError("Invalid RGB hex color code format");
			}

			// Return empty error string (success)
			return std::string{};
		};
	}
};

class WzMapToolsAppInstance : public CLI::App
{
protected:
	explicit WzMapToolsAppInstance()
		: CLI::App("WZ2100 Map Tools")
	{ }
public:
	static std::shared_ptr<WzMapToolsAppInstance> makeWzMapToolsAppInstance()
	{
		class make_shared_enabler: public WzMapToolsAppInstance {};
		auto app = std::make_shared<make_shared_enabler>();

	#if defined(MAPTOOLS_CLI_VERSION_MAJOR) && defined(MAPTOOLS_CLI_VERSION_MINOR) && defined(MAPTOOLS_CLI_VERSION_REV)
		app->set_version_flag("--version", []() -> std::string {
			return generateMapToolsVersionInfo();
		});
	#else
		#error Missing maptools version defines
	#endif

		// Initialize rand()
		srand((unsigned int)time(NULL));
		(void)rand();
		app->mapSeed = rand();

		std::stringstream footerInfo;
		footerInfo << "License: GPL-2.0-or-later" << std::endl;
		footerInfo << "Source: https://github.com/Warzone2100/maptools-cli" << std::endl;
		app->footer(footerInfo.str());

		app->add_flag("-v,--verbose", app->verbose, "Verbose output");

		WzMapToolsAppInstance::addSubCommand_Package(app);
		WzMapToolsAppInstance::addSubCommand_Map(app);

		return app;
	}
	int getRetVal() const { return retVal; }
private:
	static void addSubCommand_Package(const std::shared_ptr<WzMapToolsAppInstance>& app);
	static void addSubCommand_Map(const std::shared_ptr<WzMapToolsAppInstance>& app);
private:
	int retVal = 0;
	bool verbose = false;

	std::string inputPath;
	std::string outputPath;
	uint32_t mapSeed;
	WzMap::OutputFormat outputMapFormat = WzMap::LatestOutputFormat;

	// package variables
	WzMap::LevelFormat outputLevelFormat = WzMap::LevelFormat::JSON;
	bool sub_convert_copyadditionalfiles = false;
	bool sub_convert_fixed_last_mod = false;
	bool sub_convert_uncompressed = false;
	std::string override_map_name;

	MapToolsPreviewColorProvider preview_PlayerColorProvider = MapToolsPreviewColorProvider::Simple;
	WzMap::MapPreviewColor preview_scavsColor = ScavsColorDefault;
	WzMap::MapPreviewColorScheme::DrawOptions preview_drawOptions;

	// map commands variables
	WzMap::MapType mapType = WzMap::MapType::SKIRMISH;
	uint32_t mapMaxPlayers = 0;
};

void WzMapToolsAppInstance::addSubCommand_Package(const std::shared_ptr<WzMapToolsAppInstance>& app)
{
	std::weak_ptr<WzMapToolsAppInstance> weakAppInstance = std::weak_ptr<WzMapToolsAppInstance>(app);

	CLI::App* sub_package = app->add_subcommand("package", "Manipulating a map package");
	sub_package->fallthrough();

	std::string inputOptionDescription;
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
	inputOptionDescription = "Input map package (.wz package, or extracted package folder)";
#else
	inputOptionDescription = "Input map package (extracted package folder)";
#endif

	auto inputPathIsFile = [](const std::string& path) -> bool {
		if (path.empty())
		{
			return false;
		}
		return CLI::ExistingFile(path).empty();
	};

	// [CONVERTING MAP PACKAGE]
	CLI::App* sub_convert = sub_package->add_subcommand("convert", "Convert a map from one format to another");
	sub_convert->fallthrough();
	sub_convert->add_option("-l,--levelformat", app->outputLevelFormat, "Output level info format")
		->transform(CLI::CheckedTransformer(levelformat_map, CLI::ignore_case).description("value in {\n\t\tlev -> LEV (flaME-compatible / old),\n\t\tjson -> JSON level file (WZ 4.3+),\n\t\tlatest -> " + CLI::detail::to_string(WzMap::LatestLevelFormat) + "}"))
		->default_val("latest");
	sub_convert->add_option("-f,--format", app->outputMapFormat, "Output map format")
		->required()
		->transform(CLI::CheckedTransformer(outputformat_map, CLI::ignore_case).description("value in {\n\t\tbjo -> Binary .BJO (flaME-compatible / old),\n\t\tjson -> JSONv1 (WZ 3.4+),\n\t\tjsonv2 -> JSONv2 (WZ 4.1+),\n\t\tlatest -> " + CLI::detail::to_string(WzMap::LatestOutputFormat) + "}"));
	sub_convert->add_option("-i,--input,input", app->inputPath, inputOptionDescription)
		->required()
		->check(CLI::ExistingPath);
	sub_convert->add_option("-o,--output,output", app->outputPath, "Output path")
		->required()
		->check(CLI::NonexistentPath);
	sub_convert->add_flag("--preserve-mods", app->sub_convert_copyadditionalfiles, "Copy other files from the original map package (i.e. the extra files / modifications in a map-mod)");
	sub_convert->add_flag("--fixed-lastmod", app->sub_convert_fixed_last_mod, "Fixed last modification date (if outputting to a .wz archive)");
	sub_convert->add_flag("--output-uncompressed", app->sub_convert_uncompressed, "Output uncompressed to a folder (not in a .wz file)");
	sub_convert->add_option("--set-name", app->override_map_name, "Set / override the map name when converting");
	sub_convert->add_option("--map-seed", app->mapSeed, "Specify the script-generated map seed");
	sub_convert->callback([weakAppInstance, inputPathIsFile]() {
		auto app = weakAppInstance.lock();
		if (!app)
		{
			std::cerr << "ERROR: Invalid instance" << std::endl;
			return;
		}
		optional<std::string> override_map_name_opt = nullopt;
		if (!app->override_map_name.empty())
		{
			override_map_name_opt = app->override_map_name;
		}
		if (inputPathIsFile(app->inputPath))
		{
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
			if (!convertMapPackage_FromArchive(app->inputPath, app->outputPath, app->outputLevelFormat, app->outputMapFormat, app->mapSeed, app->sub_convert_copyadditionalfiles, app->verbose, app->sub_convert_uncompressed, app->sub_convert_fixed_last_mod, override_map_name_opt))
			{
				app->retVal = 1;
			}
#else
			std::cerr << "ERROR: maptools was compiled without support for .wz archives, and cannot open: " << app->inputPath << std::endl;
			retVal = 1;
#endif
		}
		else
		{
			if (!convertMapPackage(app->inputPath, app->outputPath, app->outputLevelFormat, app->outputMapFormat, app->mapSeed, app->sub_convert_copyadditionalfiles, app->verbose, app->sub_convert_uncompressed, app->sub_convert_fixed_last_mod, override_map_name_opt))
			{
				app->retVal = 1;
			}
		}
	});

	// [GENERATING MAP PREVIEW PNG]
	CLI::App* sub_preview = sub_package->add_subcommand("genpreview", "Generate a map preview PNG");
	sub_preview->fallthrough();
	sub_preview->add_option("-i,--input,input", app->inputPath, inputOptionDescription)
		->required()
		->check(CLI::ExistingPath);
	sub_preview->add_option("-o,--output,output", app->outputPath, "Output PNG filename (+ path)")
		->required()
		->check(FileExtensionValidator(".png"));
	sub_preview->add_option("-c,--playercolors", app->preview_PlayerColorProvider, "Player colors")
		->transform(CLI::CheckedTransformer(previewcolors_map, CLI::ignore_case).description("value in {\n\t\tsimple -> use one color for scavs, one color for players,\n\t\twz -> use WZ colors for players (distinct)\n\t}"))
		->default_val("simple");
	sub_preview->add_option("--scavcolor", app->preview_scavsColor, "Specify the scavengers hex color")
		->check(AsHexColorValue());
	sub_preview->add_option("--layers", app->preview_drawOptions, "Specify layers to draw\n\t\teither \"all\" or a comma-separated list of any of:\n\t\t\"terrain\",\"structures\",\"oil\"")
		->default_val("all");
	sub_preview->add_option("--map-seed", app->mapSeed, "Specify the script-generated map seed");
	sub_preview->callback([weakAppInstance, inputPathIsFile]() {
		auto app = weakAppInstance.lock();
		if (!app)
		{
			std::cerr << "ERROR: Invalid instance" << std::endl;
			return;
		}
		if (inputPathIsFile(app->inputPath))
		{
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
			if (!generateMapPreviewPNG_FromArchive(app->inputPath, app->outputPath, app->preview_PlayerColorProvider, app->preview_scavsColor, app->preview_drawOptions, app->mapSeed, app->verbose))
			{
				app->retVal = 1;
			}
#else
			std::cerr << "ERROR: maptools was compiled without support for .wz archives, and cannot open: " << app->inputPath << std::endl;
			app->retVal = 1;
#endif
		}
		else
		{
			if (!generateMapPreviewPNG_FromPackageContents(app->inputPath, app->outputPath, app->preview_PlayerColorProvider, app->preview_scavsColor, app->preview_drawOptions, app->mapSeed, app->verbose))
			{
				app->retVal = 1;
			}
		}
	});

	// [EXTRACTING INFORMATION FROM A MAP PACKAGE]
	CLI::App* sub_info = sub_package->add_subcommand("info", "Extract info / stats from a map package");
	sub_info->fallthrough();
	sub_info->add_option("-i,--input,input", app->inputPath, inputOptionDescription)
		->required()
		->check(CLI::ExistingPath);
	sub_info->add_option("-o,--output", app->outputPath, "Output filename (+ path)")
		->check(FileExtensionValidator(".json"));
	sub_info->add_option("--map-seed", app->mapSeed, "Specify the script-generated map seed");
	sub_info->callback([weakAppInstance, inputPathIsFile]() {
		auto app = weakAppInstance.lock();
		if (!app)
		{
			std::cerr << "ERROR: Invalid instance" << std::endl;
			return;
		}
		optional<nlohmann::ordered_json> mapInfoJSON;
		std::shared_ptr<MapToolDebugLogger> logger;
		if (!app->outputPath.empty())
		{
			logger = std::make_shared<MapToolDebugLogger>(new MapToolDebugLogger(app->verbose));
		}
		if (inputPathIsFile(app->inputPath))
		{
#if !defined(WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT)
			mapInfoJSON = generateMapInfoJSON_FromArchive(app->inputPath, app->mapSeed, logger);
#else
			std::cerr << "ERROR: maptools was compiled without support for .wz archives, and cannot open: " << app->inputPath << std::endl;
			app->retVal = 1;
#endif
		}
		else
		{
			mapInfoJSON = generateMapInfoJSON_FromPackageContents(app->inputPath, app->mapSeed, logger);
		}

		if (!mapInfoJSON.has_value())
		{
			app->retVal = 1;
			return;
		}

		std::string jsonStr = mapInfoJSON.value().dump(4, ' ', false, nlohmann::ordered_json::error_handler_t::ignore);

		if (!app->outputPath.empty())
		{
			WzMap::StdIOProvider stdOutput;
			if (!stdOutput.writeFullFile(app->outputPath, jsonStr.c_str(), static_cast<uint32_t>(jsonStr.size())))
			{
				std::cerr << "Failed to output JSON to: " << app->outputPath << std::endl;
				app->retVal = 1;
				return;
			}
			std::cout << "Wrote output JSON to: " << app->outputPath << std::endl;
		}
		else
		{
			std::cout << jsonStr << std::endl;
		}
	});
}

void WzMapToolsAppInstance::addSubCommand_Map(const std::shared_ptr<WzMapToolsAppInstance>& app)
{
	std::weak_ptr<WzMapToolsAppInstance> weakAppInstance = std::weak_ptr<WzMapToolsAppInstance>(app);

	CLI::App* sub_map = app->add_subcommand("map", "Manipulating a map folder");
	sub_map->fallthrough();

	// [CONVERTING MAP FORMAT]
	CLI::App* sub_convert = sub_map->add_subcommand("convert", "Convert a map from one format to another");
	sub_convert->fallthrough();
	sub_convert->add_option("-t,--maptype", app->mapType, "Map type")
		->transform(CLI::CheckedTransformer(maptype_map, CLI::ignore_case))
		->default_val(WzMap::MapType::SKIRMISH);
	sub_convert->add_option("-p,--maxplayers", app->mapMaxPlayers, "Map max players")
		->required()
		->check(CLI::Range(1, 10));
	sub_convert->add_option("-f,--format", app->outputMapFormat, "Output map format")
		->required()
		->transform(CLI::CheckedTransformer(outputformat_map, CLI::ignore_case).description("value in {\n\t\tbjo -> Binary .BJO (flaME-compatible / old),\n\t\tjson -> JSONv1 (WZ 3.4+),\n\t\tjsonv2 -> JSONv2 (WZ 4.1+),\n\t\tlatest -> " + CLI::detail::to_string(WzMap::LatestOutputFormat) + "}"));
	sub_convert->add_option("-i,--input,inputmapdir", app->inputPath, "Input map directory")
		->required()
		->check(CLI::ExistingDirectory);
	sub_convert->add_option("-o,--output,outputmapdir", app->outputPath, "Output map directory")
		->required()
		->check(CLI::ExistingDirectory);
	sub_convert->add_option("--map-seed", app->mapSeed, "Specify the script-generated map seed");
	sub_convert->callback([weakAppInstance]() {
		auto app = weakAppInstance.lock();
		if (!app)
		{
			std::cerr << "ERROR: Invalid instance" << std::endl;
			return;
		}
		if (!convertMap(app->mapType, app->mapMaxPlayers, app->inputPath, app->outputPath, app->outputMapFormat, app->mapSeed, app->verbose))
		{
			app->retVal = 1;
		}
	});

	// [GENERATING MAP PREVIEW PNG]
	CLI::App* sub_preview = sub_map->add_subcommand("genpreview", "Generate a map preview PNG");
	sub_preview->fallthrough();
	sub_preview->add_option("-t,--maptype", app->mapType, "Map type")
		->transform(CLI::CheckedTransformer(maptype_map, CLI::ignore_case))
		->default_val(WzMap::MapType::SKIRMISH);
	sub_preview->add_option("-p,--maxplayers", app->mapMaxPlayers, "Map max players")
		->required()
		->check(CLI::Range(1, 10));
	sub_preview->add_option("-i,--input,inputmapdir", app->inputPath, "Input map directory")
		->required()
		->check(CLI::ExistingDirectory);
	sub_preview->add_option("-o,--output,output", app->outputPath, "Output PNG filename (+ path)")
		->required()
		->check(FileExtensionValidator(".png"));
	sub_preview->add_option("-c,--playercolors", app->preview_PlayerColorProvider, "Player colors")
		->transform(CLI::CheckedTransformer(previewcolors_map, CLI::ignore_case).description("value in {\n\t\tsimple -> use one color for scavs, one color for players,\n\t\twz -> use WZ colors for players (distinct)\n\t}"))
		->default_val("simple");
	sub_preview->add_option("--scavcolor", app->preview_scavsColor, "Specify the scavengers hex color")
		->check(AsHexColorValue());
	sub_preview->add_option("--layers", app->preview_drawOptions, "Specify layers to draw\n\t\teither \"all\" or a comma-separated list of any of:\n\t\t\"terrain\",\"structures\",\"oil\"")
		->default_val("all");
	sub_preview->add_option("--map-seed", app->mapSeed, "Specify the script-generated map seed");
	sub_preview->callback([weakAppInstance]() {
		auto app = weakAppInstance.lock();
		if (!app)
		{
			std::cerr << "ERROR: Invalid instance" << std::endl;
			return;
		}
		if (!generateMapPreviewPNG_FromMapDirectory(app->mapType, app->mapMaxPlayers, app->inputPath, app->outputPath, app->preview_PlayerColorProvider, app->preview_scavsColor, app->preview_drawOptions, app->mapSeed, app->verbose))
		{
			app->retVal = 1;
		}
	});
}

int main(int argc, char **argv)
{
	std::shared_ptr<WzMapToolsAppInstance> app = WzMapToolsAppInstance::makeWzMapToolsAppInstance();
	CLI11_PARSE((*app), argc, argv);
	return app->getRetVal();
}
