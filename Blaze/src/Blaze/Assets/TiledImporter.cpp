#include "TiledImporter.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Renderer/GameObject.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace Blaze
{
	namespace
	{
		constexpr unsigned FLIPPED_HORIZONTALLY = 0x80000000u;
		constexpr unsigned FLIPPED_VERTICALLY = 0x40000000u;
		constexpr unsigned FLIPPED_DIAGONALLY = 0x20000000u;
		constexpr unsigned TILE_ID_MASK = ~(FLIPPED_HORIZONTALLY | FLIPPED_VERTICALLY | FLIPPED_DIAGONALLY);

		struct JsonValue
		{
			enum class Type { Null, Bool, Number, String, Array, Object };
			Type type = Type::Null;
			bool b = false;
			double n = 0.0;
			std::string s;
			std::vector<JsonValue> a;
			std::unordered_map<std::string, JsonValue> o;

			bool IsObject() const { return type == Type::Object; }
			bool IsArray() const { return type == Type::Array; }
			bool Has(const std::string& key) const { return type == Type::Object && o.find(key) != o.end(); }
			const JsonValue& At(const std::string& key) const
			{
				static const JsonValue nullVal;
				auto it = o.find(key);
				return it == o.end() ? nullVal : it->second;
			}
			double Number(double fallback = 0.0) const
			{
				if (type == Type::Number) return n;
				if (type == Type::Bool) return b ? 1.0 : 0.0;
				if (type == Type::String)
				{
					try { return std::stod(s); }
					catch (...) { return fallback; }
				}
				return fallback;
			}
			int Int(int fallback = 0) const { return (int)std::lround(Number((double)fallback)); }
			bool Bool(bool fallback = false) const
			{
				if (type == Type::Bool) return b;
				if (type == Type::Number) return n != 0.0;
				return fallback;
			}
			std::string String(const std::string& fallback = {}) const
			{
				if (type == Type::String) return s;
				return fallback;
			}
		};

		struct ParseError : std::runtime_error
		{
			using std::runtime_error::runtime_error;
		};

		class JsonParser
		{
		public:
			explicit JsonParser(const std::string& text) : m_Text(text) {}

			JsonValue Parse()
			{
				SkipWs();
				JsonValue value = ParseValue();
				SkipWs();
				return value;
			}

		private:
			const std::string& m_Text;
			size_t m_Pos = 0;

			char Peek() const { return m_Pos < m_Text.size() ? m_Text[m_Pos] : '\0'; }
			char Get() { return m_Pos < m_Text.size() ? m_Text[m_Pos++] : '\0'; }

			void SkipWs()
			{
				while (m_Pos < m_Text.size() && std::isspace((unsigned char)m_Text[m_Pos]))
					++m_Pos;
			}

			JsonValue ParseValue()
			{
				SkipWs();
				const char c = Peek();
				if (c == '{') return ParseObject();
				if (c == '[') return ParseArray();
				if (c == '"') return ParseStringValue();
				if (c == 't' || c == 'f') return ParseBool();
				if (c == 'n') return ParseNull();
				if (c == '-' || std::isdigit((unsigned char)c)) return ParseNumber();
				throw ParseError("Unexpected JSON token");
			}

			JsonValue ParseObject()
			{
				Get(); // {
				JsonValue value;
				value.type = JsonValue::Type::Object;
				SkipWs();
				if (Peek() == '}')
				{
					Get();
					return value;
				}
				while (true)
				{
					SkipWs();
					if (Peek() != '"')
						throw ParseError("Expected object key");
					std::string key = ParseString();
					SkipWs();
					if (Get() != ':')
						throw ParseError("Expected ':'");
					value.o.emplace(std::move(key), ParseValue());
					SkipWs();
					const char sep = Get();
					if (sep == '}')
						break;
					if (sep != ',')
						throw ParseError("Expected ',' or '}'");
				}
				return value;
			}

			JsonValue ParseArray()
			{
				Get(); // [
				JsonValue value;
				value.type = JsonValue::Type::Array;
				SkipWs();
				if (Peek() == ']')
				{
					Get();
					return value;
				}
				while (true)
				{
					value.a.push_back(ParseValue());
					SkipWs();
					const char sep = Get();
					if (sep == ']')
						break;
					if (sep != ',')
						throw ParseError("Expected ',' or ']'");
				}
				return value;
			}

			std::string ParseString()
			{
				if (Get() != '"')
					throw ParseError("Expected string");
				std::string out;
				while (true)
				{
					char c = Get();
					if (c == '\0')
						throw ParseError("Unterminated string");
					if (c == '"')
						break;
					if (c == '\\')
					{
						char e = Get();
						switch (e)
						{
						case '"': case '\\': case '/': out.push_back(e); break;
						case 'b': out.push_back('\b'); break;
						case 'f': out.push_back('\f'); break;
						case 'n': out.push_back('\n'); break;
						case 'r': out.push_back('\r'); break;
						case 't': out.push_back('\t'); break;
						case 'u':
						{
							unsigned code = 0;
							for (int i = 0; i < 4; ++i)
							{
								char h = Get();
								code <<= 4;
								if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
								else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
								else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
								else throw ParseError("Bad unicode escape");
							}
							if (code < 128)
								out.push_back((char)code);
							else
								out.push_back('?');
							break;
						}
						default:
							out.push_back(e);
							break;
						}
					}
					else
						out.push_back(c);
				}
				return out;
			}

			JsonValue ParseStringValue()
			{
				JsonValue value;
				value.type = JsonValue::Type::String;
				value.s = ParseString();
				return value;
			}

			JsonValue ParseNumber()
			{
				const size_t start = m_Pos;
				if (Peek() == '-') Get();
				while (std::isdigit((unsigned char)Peek())) Get();
				if (Peek() == '.')
				{
					Get();
					while (std::isdigit((unsigned char)Peek())) Get();
				}
				if (Peek() == 'e' || Peek() == 'E')
				{
					Get();
					if (Peek() == '+' || Peek() == '-') Get();
					while (std::isdigit((unsigned char)Peek())) Get();
				}
				JsonValue value;
				value.type = JsonValue::Type::Number;
				value.n = std::stod(m_Text.substr(start, m_Pos - start));
				return value;
			}

			JsonValue ParseBool()
			{
				JsonValue value;
				value.type = JsonValue::Type::Bool;
				if (m_Text.compare(m_Pos, 4, "true") == 0)
				{
					m_Pos += 4;
					value.b = true;
					return value;
				}
				if (m_Text.compare(m_Pos, 5, "false") == 0)
				{
					m_Pos += 5;
					value.b = false;
					return value;
				}
				throw ParseError("Invalid bool");
			}

			JsonValue ParseNull()
			{
				if (m_Text.compare(m_Pos, 4, "null") != 0)
					throw ParseError("Invalid null");
				m_Pos += 4;
				return {};
			}
		};

		struct TilesetInfo
		{
			int firstGid = 1;
			std::string name;
			std::string imagePath;
			int tileWidth = 32;
			int tileHeight = 32;
			int columns = 1;
			int tileCount = 1;
			int imageWidth = 0;
			int imageHeight = 0;
			std::vector<uint8_t> solid; // local tile id
		};

		struct TileLayerInfo
		{
			std::string name;
			bool visible = true;
			bool collision = false;
			float opacity = 1.0f;
			std::vector<unsigned> gids; // raw including flip bits, Tiled order (top-left)
			int width = 0;
			int height = 0;
		};

		struct ObjectInfo
		{
			std::string name;
			std::string type;
			float x = 0.0f;
			float y = 0.0f;
			float width = 0.0f;
			float height = 0.0f;
		};

		struct ParsedMap
		{
			int width = 0;
			int height = 0;
			int tileWidth = 32;
			int tileHeight = 32;
			std::vector<TilesetInfo> tilesets;
			std::vector<TileLayerInfo> layers;
			std::vector<ObjectInfo> objects;
		};

		std::string ReadFileText(const fs::path& path)
		{
			std::ifstream in(path, std::ios::binary);
			if (!in)
				return {};
			std::ostringstream ss;
			ss << in.rdbuf();
			return ss.str();
		}

		std::string ToLower(std::string s)
		{
			for (char& c : s)
				c = (char)std::tolower((unsigned char)c);
			return s;
		}

		bool NameSuggestsCollision(const std::string& name)
		{
			const std::string lower = ToLower(name);
			return lower.find("collision") != std::string::npos
				|| lower.find("solid") != std::string::npos
				|| lower == "col"
				|| lower == "colliders";
		}

		bool PropertyBool(const JsonValue& props, const std::string& key, bool fallback = false)
		{
			if (!props.IsArray())
				return fallback;
			for (const JsonValue& p : props.a)
			{
				if (!p.IsObject())
					continue;
				if (ToLower(p.At("name").String()) != ToLower(key))
					continue;
				const JsonValue& v = p.At("value");
				if (v.type == JsonValue::Type::Bool)
					return v.Bool();
				if (v.type == JsonValue::Type::Number)
					return v.Number() != 0.0;
				if (v.type == JsonValue::Type::String)
				{
					const std::string s = ToLower(v.String());
					return s == "true" || s == "1" || s == "yes";
				}
			}
			return fallback;
		}

		TilesetInfo ParseTilesetObject(const JsonValue& ts, const fs::path& mapDir)
		{
			TilesetInfo info;
			info.firstGid = ts.At("firstgid").Int(1);
			info.name = ts.At("name").String("Tileset");
			info.tileWidth = std::max(ts.At("tilewidth").Int(32), 1);
			info.tileHeight = std::max(ts.At("tileheight").Int(32), 1);
			info.columns = std::max(ts.At("columns").Int(0), 0);
			info.tileCount = std::max(ts.At("tilecount").Int(0), 0);
			info.imageWidth = ts.At("imagewidth").Int(0);
			info.imageHeight = ts.At("imageheight").Int(0);

			std::string image = ts.At("image").String();
			if (image.empty() && ts.Has("source"))
			{
				const fs::path sourcePath = mapDir / ts.At("source").String();
				const std::string text = ReadFileText(sourcePath);
				if (!text.empty())
				{
					try
					{
						JsonParser parser(text);
						JsonValue root = parser.Parse();
						if (image.empty())
							image = root.At("image").String();
						if (info.columns <= 0)
							info.columns = root.At("columns").Int(0);
						if (info.tileCount <= 0)
							info.tileCount = root.At("tilecount").Int(0);
						if (info.imageWidth <= 0)
							info.imageWidth = root.At("imagewidth").Int(0);
						if (info.imageHeight <= 0)
							info.imageHeight = root.At("imageheight").Int(0);
						info.tileWidth = std::max(root.At("tilewidth").Int(info.tileWidth), 1);
						info.tileHeight = std::max(root.At("tileheight").Int(info.tileHeight), 1);
						if (info.name == "Tileset")
							info.name = root.At("name").String(info.name);

						const JsonValue& tiles = root.At("tiles");
						if (tiles.IsArray())
						{
							const int count = std::max(info.tileCount, 1);
							info.solid.assign((size_t)count, 0);
							for (const JsonValue& tile : tiles.a)
							{
								const int id = tile.At("id").Int(-1);
								if (id < 0 || id >= count)
									continue;
								bool solid = PropertyBool(tile.At("properties"), "solid", false)
									|| PropertyBool(tile.At("properties"), "collision", false);
								const std::string type = ToLower(tile.At("type").String());
								if (type == "solid" || type == "collision" || type == "wall")
									solid = true;
								info.solid[(size_t)id] = solid ? 1 : 0;
							}
						}
					}
					catch (...)
					{
					}
					if (!image.empty() && !fs::path(image).is_absolute())
						info.imagePath = (sourcePath.parent_path() / image).generic_string();
				}
			}

			if (!image.empty())
			{
				fs::path imgPath = image;
				if (!imgPath.is_absolute())
					imgPath = mapDir / imgPath;
				info.imagePath = imgPath.generic_string();
			}

			if (info.columns <= 0 && info.imageWidth > 0)
				info.columns = std::max(info.imageWidth / info.tileWidth, 1);
			if (info.columns <= 0)
				info.columns = 1;
			if (info.tileCount <= 0 && info.imageHeight > 0)
				info.tileCount = info.columns * std::max(info.imageHeight / info.tileHeight, 1);
			if (info.tileCount <= 0)
				info.tileCount = info.columns;

			if (info.solid.empty())
				info.solid.assign((size_t)info.tileCount, 0);

			const JsonValue& tiles = ts.At("tiles");
			if (tiles.IsArray())
			{
				if ((int)info.solid.size() < info.tileCount)
					info.solid.resize((size_t)info.tileCount, 0);
				for (const JsonValue& tile : tiles.a)
				{
					const int id = tile.At("id").Int(-1);
					if (id < 0 || id >= (int)info.solid.size())
						continue;
					bool solid = PropertyBool(tile.At("properties"), "solid", false)
						|| PropertyBool(tile.At("properties"), "collision", false);
					const std::string type = ToLower(tile.At("type").String());
					if (type == "solid" || type == "collision" || type == "wall")
						solid = true;
					info.solid[(size_t)id] = solid ? 1 : 0;
				}
			}

			return info;
		}

		void AppendObjectsFromLayer(const JsonValue& layer, std::vector<ObjectInfo>& out)
		{
			const JsonValue& objects = layer.At("objects");
			if (!objects.IsArray())
				return;
			for (const JsonValue& obj : objects.a)
			{
				ObjectInfo info;
				info.name = obj.At("name").String("Object");
				info.type = obj.At("type").String();
				info.x = (float)obj.At("x").Number();
				info.y = (float)obj.At("y").Number();
				info.width = (float)obj.At("width").Number();
				info.height = (float)obj.At("height").Number();
				out.push_back(std::move(info));
			}
		}

		bool ParseTiledJson(const fs::path& path, ParsedMap& out, std::string& error)
		{
			const std::string text = ReadFileText(path);
			if (text.empty())
			{
				error = "Could not read file";
				return false;
			}

			JsonValue root;
			try
			{
				JsonParser parser(text);
				root = parser.Parse();
			}
			catch (const std::exception& ex)
			{
				error = std::string("JSON parse failed: ") + ex.what();
				return false;
			}

			if (!root.IsObject())
			{
				error = "Root is not a JSON object";
				return false;
			}

			const std::string orientation = ToLower(root.At("orientation").String("orthogonal"));
			if (orientation != "orthogonal")
			{
				error = "Only orthogonal Tiled maps are supported";
				return false;
			}

			out.width = root.At("width").Int(0);
			out.height = root.At("height").Int(0);
			out.tileWidth = std::max(root.At("tilewidth").Int(32), 1);
			out.tileHeight = std::max(root.At("tileheight").Int(32), 1);
			if (out.width <= 0 || out.height <= 0)
			{
				error = "Invalid map size";
				return false;
			}

			const fs::path mapDir = path.parent_path();
			const JsonValue& tilesets = root.At("tilesets");
			if (tilesets.IsArray())
			{
				for (const JsonValue& ts : tilesets.a)
					out.tilesets.push_back(ParseTilesetObject(ts, mapDir));
			}
			std::sort(out.tilesets.begin(), out.tilesets.end(),
				[](const TilesetInfo& a, const TilesetInfo& b) { return a.firstGid < b.firstGid; });

			const JsonValue& layers = root.At("layers");
			if (!layers.IsArray())
			{
				error = "Map has no layers";
				return false;
			}

			for (const JsonValue& layer : layers.a)
			{
				const std::string type = ToLower(layer.At("type").String());
				if (type == "objectgroup")
				{
					AppendObjectsFromLayer(layer, out.objects);
					continue;
				}
				if (type == "group")
				{
					const JsonValue& children = layer.At("layers");
					if (children.IsArray())
					{
						for (const JsonValue& child : children.a)
						{
							if (ToLower(child.At("type").String()) == "objectgroup")
								AppendObjectsFromLayer(child, out.objects);
							else if (ToLower(child.At("type").String()) == "tilelayer")
							{
								TileLayerInfo info;
								info.name = child.At("name").String("Layer");
								info.visible = child.At("visible").Bool(true);
								info.opacity = (float)child.At("opacity").Number(1.0);
								info.width = child.At("width").Int(out.width);
								info.height = child.At("height").Int(out.height);
								info.collision = NameSuggestsCollision(info.name)
									|| PropertyBool(child.At("properties"), "collision", false)
									|| PropertyBool(child.At("properties"), "solid", false);
								const JsonValue& data = child.At("data");
								if (data.IsArray())
								{
									info.gids.reserve(data.a.size());
									for (const JsonValue& v : data.a)
										info.gids.push_back((unsigned)v.Number());
								}
								if ((int)info.gids.size() >= info.width * info.height)
									out.layers.push_back(std::move(info));
							}
						}
					}
					continue;
				}
				if (type != "tilelayer")
					continue;

				TileLayerInfo info;
				info.name = layer.At("name").String("Layer");
				info.visible = layer.At("visible").Bool(true);
				info.opacity = (float)layer.At("opacity").Number(1.0);
				info.width = layer.At("width").Int(out.width);
				info.height = layer.At("height").Int(out.height);
				info.collision = NameSuggestsCollision(info.name)
					|| PropertyBool(layer.At("properties"), "collision", false)
					|| PropertyBool(layer.At("properties"), "solid", false);
				const JsonValue& data = layer.At("data");
				if (!data.IsArray())
					continue;
				info.gids.reserve(data.a.size());
				for (const JsonValue& v : data.a)
					info.gids.push_back((unsigned)v.Number());
				if ((int)info.gids.size() < info.width * info.height)
					continue;
				out.layers.push_back(std::move(info));
			}

			if (out.layers.empty())
			{
				error = "No tile layers found";
				return false;
			}
			if (out.tilesets.empty())
			{
				error = "No tilesets found";
				return false;
			}
			return true;
		}

		std::string XmlAttr(const std::string& tag, const std::string& key)
		{
			const std::string needle = key + "=\"";
			const size_t pos = tag.find(needle);
			if (pos == std::string::npos)
				return {};
			const size_t start = pos + needle.size();
			const size_t end = tag.find('"', start);
			if (end == std::string::npos)
				return {};
			return tag.substr(start, end - start);
		}

		bool ParseTiledTmx(const fs::path& path, ParsedMap& out, std::string& error)
		{
			const std::string text = ReadFileText(path);
			if (text.empty())
			{
				error = "Could not read file";
				return false;
			}

			const size_t mapPos = text.find("<map");
			if (mapPos == std::string::npos)
			{
				error = "Not a TMX map";
				return false;
			}
			const size_t mapEnd = text.find('>', mapPos);
			const std::string mapTag = text.substr(mapPos, mapEnd - mapPos + 1);
			if (ToLower(XmlAttr(mapTag, "orientation")) != "orthogonal" && !XmlAttr(mapTag, "orientation").empty())
			{
				error = "Only orthogonal Tiled maps are supported";
				return false;
			}

			out.width = std::stoi(XmlAttr(mapTag, "width").empty() ? "0" : XmlAttr(mapTag, "width"));
			out.height = std::stoi(XmlAttr(mapTag, "height").empty() ? "0" : XmlAttr(mapTag, "height"));
			out.tileWidth = std::max(std::stoi(XmlAttr(mapTag, "tilewidth").empty() ? "32" : XmlAttr(mapTag, "tilewidth")), 1);
			out.tileHeight = std::max(std::stoi(XmlAttr(mapTag, "tileheight").empty() ? "32" : XmlAttr(mapTag, "tileheight")), 1);
			if (out.width <= 0 || out.height <= 0)
			{
				error = "Invalid map size";
				return false;
			}

			const fs::path mapDir = path.parent_path();
			size_t search = 0;
			while (true)
			{
				const size_t tsPos = text.find("<tileset", search);
				if (tsPos == std::string::npos)
					break;
				const size_t tsTagEnd = text.find('>', tsPos);
				if (tsTagEnd == std::string::npos)
					break;
				const std::string tsTag = text.substr(tsPos, tsTagEnd - tsPos + 1);
				TilesetInfo info;
				info.firstGid = std::stoi(XmlAttr(tsTag, "firstgid").empty() ? "1" : XmlAttr(tsTag, "firstgid"));
				info.name = XmlAttr(tsTag, "name");
				if (info.name.empty())
					info.name = "Tileset";
				info.tileWidth = std::max(std::stoi(XmlAttr(tsTag, "tilewidth").empty() ? std::to_string(out.tileWidth) : XmlAttr(tsTag, "tilewidth")), 1);
				info.tileHeight = std::max(std::stoi(XmlAttr(tsTag, "tileheight").empty() ? std::to_string(out.tileHeight) : XmlAttr(tsTag, "tileheight")), 1);
				info.columns = std::max(std::stoi(XmlAttr(tsTag, "columns").empty() ? "0" : XmlAttr(tsTag, "columns")), 0);
				info.tileCount = std::max(std::stoi(XmlAttr(tsTag, "tilecount").empty() ? "0" : XmlAttr(tsTag, "tilecount")), 0);

				const std::string source = XmlAttr(tsTag, "source");
				size_t blockEnd = tsTagEnd;
				if (tsTag.find("/>") == std::string::npos)
				{
					blockEnd = text.find("</tileset>", tsTagEnd);
					if (blockEnd == std::string::npos)
						blockEnd = tsTagEnd;
				}
				const std::string block = text.substr(tsPos, blockEnd - tsPos);
				const size_t imgPos = block.find("<image");
				if (imgPos != std::string::npos)
				{
					const size_t imgEnd = block.find('>', imgPos);
					const std::string imgTag = block.substr(imgPos, imgEnd - imgPos + 1);
					std::string image = XmlAttr(imgTag, "source");
					info.imageWidth = std::stoi(XmlAttr(imgTag, "width").empty() ? "0" : XmlAttr(imgTag, "width"));
					info.imageHeight = std::stoi(XmlAttr(imgTag, "height").empty() ? "0" : XmlAttr(imgTag, "height"));
					if (!image.empty())
					{
						fs::path imgPath = image;
						if (!imgPath.is_absolute())
							imgPath = mapDir / imgPath;
						info.imagePath = imgPath.generic_string();
					}
				}
				else if (!source.empty())
				{
					// External TSX not fully parsed — try sibling JSON/TSX image guess later.
					fs::path srcPath = mapDir / source;
					const std::string extText = ReadFileText(srcPath);
					const size_t img2 = extText.find("<image");
					if (img2 != std::string::npos)
					{
						const size_t imgEnd = extText.find('>', img2);
						const std::string imgTag = extText.substr(img2, imgEnd - img2 + 1);
						std::string image = XmlAttr(imgTag, "source");
						if (!image.empty())
						{
							fs::path imgPath = image;
							if (!imgPath.is_absolute())
								imgPath = srcPath.parent_path() / imgPath;
							info.imagePath = imgPath.generic_string();
						}
						info.imageWidth = std::stoi(XmlAttr(imgTag, "width").empty() ? "0" : XmlAttr(imgTag, "width"));
						info.imageHeight = std::stoi(XmlAttr(imgTag, "height").empty() ? "0" : XmlAttr(imgTag, "height"));
						if (info.columns <= 0)
							info.columns = std::stoi(XmlAttr(extText.substr(0, extText.find('>')), "columns").empty() ? "0" : "0");
					}
				}

				if (info.columns <= 0 && info.imageWidth > 0)
					info.columns = std::max(info.imageWidth / info.tileWidth, 1);
				if (info.columns <= 0)
					info.columns = 1;
				if (info.tileCount <= 0 && info.imageHeight > 0)
					info.tileCount = info.columns * std::max(info.imageHeight / info.tileHeight, 1);
				if (info.tileCount <= 0)
					info.tileCount = info.columns;
				info.solid.assign((size_t)info.tileCount, 0);
				out.tilesets.push_back(std::move(info));
				search = blockEnd + 1;
			}

			search = 0;
			while (true)
			{
				const size_t layerPos = text.find("<layer", search);
				if (layerPos == std::string::npos)
					break;
				const size_t layerTagEnd = text.find('>', layerPos);
				if (layerTagEnd == std::string::npos)
					break;
				const std::string layerTag = text.substr(layerPos, layerTagEnd - layerPos + 1);
				TileLayerInfo info;
				info.name = XmlAttr(layerTag, "name");
				if (info.name.empty())
					info.name = "Layer";
				info.visible = XmlAttr(layerTag, "visible") != "0";
				info.opacity = XmlAttr(layerTag, "opacity").empty() ? 1.0f : std::stof(XmlAttr(layerTag, "opacity"));
				info.width = std::stoi(XmlAttr(layerTag, "width").empty() ? std::to_string(out.width) : XmlAttr(layerTag, "width"));
				info.height = std::stoi(XmlAttr(layerTag, "height").empty() ? std::to_string(out.height) : XmlAttr(layerTag, "height"));
				info.collision = NameSuggestsCollision(info.name);

				const size_t dataPos = text.find("<data", layerTagEnd);
				const size_t layerClose = text.find("</layer>", layerTagEnd);
				if (dataPos == std::string::npos || layerClose == std::string::npos || dataPos > layerClose)
				{
					search = layerTagEnd + 1;
					continue;
				}
				const size_t dataTagEnd = text.find('>', dataPos);
				const std::string dataTag = text.substr(dataPos, dataTagEnd - dataPos + 1);
				const std::string encoding = ToLower(XmlAttr(dataTag, "encoding"));
				if (encoding != "csv" && !encoding.empty())
				{
					error = "TMX layer '" + info.name + "' uses unsupported encoding (use CSV or JSON export)";
					return false;
				}
				const size_t dataClose = text.find("</data>", dataTagEnd);
				std::string csv = text.substr(dataTagEnd + 1, dataClose - dataTagEnd - 1);
				std::replace(csv.begin(), csv.end(), '\n', ',');
				std::replace(csv.begin(), csv.end(), '\r', ',');
				std::stringstream ss(csv);
				std::string token;
				while (std::getline(ss, token, ','))
				{
					size_t start = 0;
					while (start < token.size() && std::isspace((unsigned char)token[start])) ++start;
					size_t end = token.size();
					while (end > start && std::isspace((unsigned char)token[end - 1])) --end;
					if (end <= start)
						continue;
					info.gids.push_back((unsigned)std::stoul(token.substr(start, end - start)));
				}
				if ((int)info.gids.size() >= info.width * info.height)
					out.layers.push_back(std::move(info));
				search = layerClose + 1;
			}

			if (out.layers.empty())
			{
				error = "No CSV tile layers found (export JSON or use CSV encoding)";
				return false;
			}
			if (out.tilesets.empty())
			{
				error = "No tilesets found";
				return false;
			}
			return true;
		}

		const TilesetInfo* FindTileset(const std::vector<TilesetInfo>& tilesets, unsigned gid)
		{
			gid &= TILE_ID_MASK;
			if (gid == 0)
				return nullptr;
			const TilesetInfo* best = nullptr;
			for (const TilesetInfo& ts : tilesets)
			{
				if ((int)gid >= ts.firstGid)
					best = &ts;
			}
			return best;
		}

		int LocalTileId(const TilesetInfo& ts, unsigned gid)
		{
			gid &= TILE_ID_MASK;
			if (gid == 0)
				return -1;
			const int local = (int)gid - ts.firstGid;
			if (local < 0 || local >= ts.tileCount)
				return -1;
			return local;
		}

		std::string CopyTextureUnique(const fs::path& source, const fs::path& destDir)
		{
			std::error_code ec;
			fs::create_directories(destDir, ec);
			fs::path destination = destDir / source.filename();
			if (fs::exists(destination))
			{
				const fs::path stem = source.stem();
				const fs::path ext = source.extension();
				int suffix = 1;
				do
				{
					destination = destDir / (stem.string() + "_" + std::to_string(suffix) + ext.string());
					++suffix;
				} while (fs::exists(destination));
			}
			fs::copy_file(source, destination, fs::copy_options::none, ec);
			if (ec)
				return {};
			return destination.generic_string();
		}

		std::string MakeRelativeToProject(const std::string& absPath)
		{
			std::error_code ec;
			const fs::path source = fs::absolute(absPath, ec);
			if (!Project::HasProject())
				return source.generic_string();
			const fs::path projectDir = fs::absolute(Project::GetDirectory(), ec);
			fs::path rel = fs::relative(source, projectDir, ec);
			if (!ec && !rel.empty() && rel.generic_string().rfind("..", 0) != 0)
				return rel.generic_string();
			return source.generic_string();
		}

		bool WriteBlazeMap(const ParsedMap& map, const std::string& outMapPath, const std::string& textureDir, std::string& error)
		{
			if (map.tilesets.empty() || map.layers.empty())
			{
				error = "Nothing to import";
				return false;
			}

			// Prefer the tileset used most often across layers (usually the main sheet).
			std::unordered_map<const TilesetInfo*, int> usage;
			for (const TileLayerInfo& layer : map.layers)
			{
				for (unsigned gid : layer.gids)
				{
					if (const TilesetInfo* ts = FindTileset(map.tilesets, gid))
						usage[ts] += 1;
				}
			}
			const TilesetInfo* primary = &map.tilesets.front();
			int bestUsage = -1;
			for (const auto& pair : usage)
			{
				if (pair.second > bestUsage)
				{
					bestUsage = pair.second;
					primary = pair.first;
				}
			}

			if (primary->imagePath.empty() || !fs::exists(primary->imagePath))
			{
				error = "Tileset image missing: " + primary->imagePath;
				return false;
			}

			const std::string copied = CopyTextureUnique(primary->imagePath, textureDir);
			if (copied.empty())
			{
				error = "Failed to copy tileset texture";
				return false;
			}

			const int setCols = std::max(primary->columns, 1);
			const int setRows = std::max((primary->tileCount + setCols - 1) / setCols, 1);
			const float cell = (float)std::max(map.tileWidth, 1);

			std::error_code ec;
			fs::create_directories(fs::path(outMapPath).parent_path(), ec);
			std::ofstream out(outMapPath, std::ios::trunc);
			if (!out.is_open())
			{
				error = "Could not write map file";
				return false;
			}

			out << "BlazeMap 1\n";
			out << "object\n";
			out << "name " << fs::path(outMapPath).stem().string() << "\n";
			out << "type " << (int)ObjectType::Tilemap << "\n";
			out << "pos 0 0 0\n";
			out << "rot 0\n";
			out << "scale 1 1\n";
			out << "layer 0\n";
			out << "visible 1\n";
			out << "color 1 1 1 1\n";
			out << "texture " << MakeRelativeToProject(copied) << "\n";
			out << "tilemap " << map.width << " " << map.height << " " << cell << "\n";
			out << "tilesetgrid " << setCols << " " << setRows << "\n";
			out << "tilecollision 1\n";
			out << "tilesetsolid";
			for (int i = 0; i < primary->tileCount; ++i)
			{
				uint8_t flag = 0;
				if (i < (int)primary->solid.size())
					flag = primary->solid[(size_t)i];
				out << " " << (int)flag;
			}
			out << "\n";
			out << "tilelayers " << (int)map.layers.size() << "\n";

			for (const TileLayerInfo& layer : map.layers)
			{
				const bool collision = layer.collision;
				const bool visible = layer.visible && !NameSuggestsCollision(layer.name);
				out << "tilelayer " << (visible ? 1 : 0) << " " << (collision ? 1 : 0)
					<< " " << layer.opacity << " " << layer.name << "\n";
				out << "tiles";

				// Tiled: row 0 = top. Blaze: row 0 = bottom.
				for (int by = 0; by < map.height; ++by)
				{
					const int ty = map.height - 1 - by;
					for (int x = 0; x < map.width; ++x)
					{
						const int tiledIndex = ty * map.width + x;
						int local = -1;
						if (tiledIndex >= 0 && tiledIndex < (int)layer.gids.size())
						{
							const unsigned gid = layer.gids[(size_t)tiledIndex];
							if (const TilesetInfo* ts = FindTileset(map.tilesets, gid))
							{
								if (ts == primary)
									local = LocalTileId(*ts, gid);
								// GIDs from other tilesets are skipped (empty) on shared-sheet import.
							}
						}
						out << " " << local;
					}
				}
				out << "\n";
			}
			out << "tilelayeractive 0\n";
			out << "end\n";

			// Optional object markers as Empty objects (positions in pixel space, map centered later by user).
			const float mapW = (float)map.width * cell;
			const float mapH = (float)map.height * cell;
			for (const ObjectInfo& obj : map.objects)
			{
				// Tiled Y down from top-left → Blaze Y up, origin at map center.
				const float cx = obj.x + obj.width * 0.5f - mapW * 0.5f;
				const float cy = mapH * 0.5f - (obj.y + obj.height * 0.5f);
				out << "object\n";
				out << "name " << (obj.name.empty() ? "TiledObject" : obj.name) << "\n";
				out << "type " << (int)ObjectType::Empty << "\n";
				out << "pos " << cx << " " << cy << " 0\n";
				out << "rot 0\n";
				out << "scale 1 1\n";
				out << "layer 1\n";
				out << "visible 1\n";
				if (!obj.type.empty())
					out << "tags " << obj.type << "\n";
				out << "end\n";
			}

			return true;
		}
	}

	bool TiledImporter::IsTiledMapFile(const std::string& path)
	{
		const std::string ext = ToLower(fs::path(path).extension().string());
		return ext == ".tmj" || ext == ".tmx" || ext == ".json";
	}

	bool TiledImporter::ImportToMapFile(
		const std::string& tiledPath,
		const std::string& outMapPath,
		const std::string& textureDir,
		std::string* outError)
	{
		std::string error;
		ParsedMap map;
		const std::string ext = ToLower(fs::path(tiledPath).extension().string());
		bool ok = false;
		if (ext == ".tmx")
			ok = ParseTiledTmx(tiledPath, map, error);
		else
			ok = ParseTiledJson(tiledPath, map, error);

		if (!ok)
		{
			if (outError)
				*outError = error;
			BZ_INFO("Tiled import failed: {0}", error);
			return false;
		}

		fs::path texDir = textureDir;
		if (texDir.empty())
			texDir = fs::path(outMapPath).parent_path();

		if (!WriteBlazeMap(map, outMapPath, texDir.generic_string(), error))
		{
			if (outError)
				*outError = error;
			BZ_INFO("Tiled import failed: {0}", error);
			return false;
		}

		BZ_INFO("Imported Tiled map -> {0} ({1} layers)", outMapPath, (int)map.layers.size());
		return true;
	}
}
