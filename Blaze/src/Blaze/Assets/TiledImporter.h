#pragma once
#include "Blaze/Core/Core.h"
#include <string>

namespace Blaze
{
	// Imports Tiled maps (.tmj / .json / orthogonal .tmx with CSV tiles) into a Blaze .map.
	class BLAZE_API TiledImporter
	{
	public:
		// Copies tileset images into textureDir (or beside outMapPath), writes a BlazeMap file.
		// Returns false on failure; optional outError receives a short message.
		static bool ImportToMapFile(
			const std::string& tiledPath,
			const std::string& outMapPath,
			const std::string& textureDir,
			std::string* outError = nullptr);

		static bool IsTiledMapFile(const std::string& path);
	};
}
