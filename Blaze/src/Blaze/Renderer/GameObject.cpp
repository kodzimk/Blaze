#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include"Blaze/Renderer/ParticleSystem2D.h"
#include"Blaze/Navigation/NavAgentSystem.h"
#include"Blaze/Navigation/EnemyAISystem.h"
#include"Blaze/UI/UISystem.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"
#include <cctype>
#include <algorithm>
#include <cmath>

namespace Blaze {
	Blaze::GameObject::GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color),m_Name(name)
	{		
		if (vertices.size() >= 5)
		{
			m_LocalMin = glm::vec2(vertices[0], vertices[1]);
			m_LocalMax = m_LocalMin;
			for (size_t i = 0; i + 1 < vertices.size(); i += 5)
			{
				m_LocalMin.x = std::min(m_LocalMin.x, vertices[i]);
				m_LocalMin.y = std::min(m_LocalMin.y, vertices[i + 1]);
				m_LocalMax.x = std::max(m_LocalMax.x, vertices[i]);
				m_LocalMax.y = std::max(m_LocalMax.y, vertices[i + 1]);
			}
		}

		glGenBuffers(1, &m_EBO);
		m_VertexArray.Bind();

		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), vertices.size() * sizeof(float), 5);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

		m_VertexArray.AttribPointer(m_VertexBuffer, 3, 0);	
		m_VertexArray.AttribPointer(m_VertexBuffer, 2, 3);
		m_VertexBuffer.UnBind();

		m_VertexArray.UnBind();
	}

	GameObject::~GameObject()
	{
		DestroyTileLayerMeshes();
		DetachFromHierarchy();
		if (m_EBO)
		{
			glDeleteBuffers(1, &m_EBO);
			m_EBO = 0;
		}
	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		if (!m_Visible || !m_Active)
			return;
		if (m_Type == ObjectType::Camera || m_Type == ObjectType::Sound || m_Type == ObjectType::Light
			|| m_Type == ObjectType::Empty || m_Type == ObjectType::PostProcess)
			return;
		if (m_Type == ObjectType::Text)
		{
			if (!UISystem::IsReady())
				UISystem::Init();
			UISystem::DrawWorldText(m_Shader, prop, m_Matrix, m_VertexArray, m_Text, m_FontSize, m_Color, m_FontPath);
			return;
		}
		if (m_Type == ObjectType::Tilemap)
		{
			DrawTilemap(prop, m_Shader);
			return;
		}
		if ((m_Type == ObjectType::Circle || m_Type == ObjectType::Box) && !m_Texture.IsValid())
			return;

		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");
		m_Shader.SetUniform2fv(GetFlipbookUVScale(), "uv_scale");
		m_Shader.SetUniform2fv(GetFlipbookUVOffset(), "uv_offset");

		m_Texture.Bind();
		m_VertexArray.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_Texture.UnBind();
	}

	void GameObject::DestroyTileLayerMeshes()
	{
		for (TilemapLayerGpuMesh& mesh : m_TileLayerMeshes)
		{
			if (mesh.ebo)
				glDeleteBuffers(1, &mesh.ebo);
			if (mesh.vbo)
				glDeleteBuffers(1, &mesh.vbo);
			if (mesh.vao)
				glDeleteVertexArrays(1, &mesh.vao);
			mesh.vao = mesh.vbo = mesh.ebo = 0;
			mesh.indexCount = 0;
			mesh.dirty = true;
		}
		m_TileLayerMeshes.clear();
	}

	void GameObject::EnsureTileLayerMeshes()
	{
		EnsureTileLayers();
		const size_t count = m_TileLayers.size();
		if (m_TileLayerMeshes.size() == count)
			return;

		if (m_TileLayerMeshes.size() > count)
		{
			for (size_t i = count; i < m_TileLayerMeshes.size(); ++i)
			{
				TilemapLayerGpuMesh& mesh = m_TileLayerMeshes[i];
				if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
				if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
				if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
			}
			m_TileLayerMeshes.resize(count);
		}
		else
		{
			m_TileLayerMeshes.resize(count);
		}

		for (TilemapLayerGpuMesh& mesh : m_TileLayerMeshes)
			mesh.dirty = true;
	}

	void GameObject::MarkTilemapMeshesDirty()
	{
		EnsureTileLayerMeshes();
		for (TilemapLayerGpuMesh& mesh : m_TileLayerMeshes)
			mesh.dirty = true;
	}

	void GameObject::MarkTilemapLayerMeshDirty(int layerIndex)
	{
		EnsureTileLayerMeshes();
		if (layerIndex < 0 || layerIndex >= (int)m_TileLayerMeshes.size())
			return;
		m_TileLayerMeshes[(size_t)layerIndex].dirty = true;
	}

	void GameObject::RebuildTilemapLayerMesh(int layerIndex)
	{
		EnsureTileLayerMeshes();
		if (layerIndex < 0 || layerIndex >= (int)m_TileLayers.size())
			return;

		TilemapLayerGpuMesh& mesh = m_TileLayerMeshes[(size_t)layerIndex];
		const TilemapLayer& layer = m_TileLayers[(size_t)layerIndex];

		if (mesh.ebo) { glDeleteBuffers(1, &mesh.ebo); mesh.ebo = 0; }
		if (mesh.vbo) { glDeleteBuffers(1, &mesh.vbo); mesh.vbo = 0; }
		if (mesh.vao) { glDeleteVertexArrays(1, &mesh.vao); mesh.vao = 0; }
		mesh.indexCount = 0;

		if (m_TilemapColumns <= 0 || m_TilemapRows <= 0)
		{
			mesh.dirty = false;
			return;
		}

		const float cell = glm::max(m_TileCellSize, 1.0f);
		const float mapW = (float)m_TilemapColumns * cell;
		const float mapH = (float)m_TilemapRows * cell;
		const float half = cell * 0.5f;
		const glm::vec2 uvScale = GetTilesetUVScale();

		std::vector<float> verts;
		std::vector<unsigned int> indices;
		verts.reserve((size_t)m_TilemapColumns * (size_t)m_TilemapRows * 20);
		indices.reserve((size_t)m_TilemapColumns * (size_t)m_TilemapRows * 6);

		for (int row = 0; row < m_TilemapRows; ++row)
		{
			for (int col = 0; col < m_TilemapColumns; ++col)
			{
				const int idx = row * m_TilemapColumns + col;
				if (idx < 0 || idx >= (int)layer.tiles.size())
					continue;
				const int tileId = layer.tiles[idx];
				if (tileId < 0)
					continue;

				const float cx = ((float)col + 0.5f) * cell - mapW * 0.5f;
				const float cy = ((float)row + 0.5f) * cell - mapH * 0.5f;
				const glm::vec2 uvOff = GetTilesetUVOffset(tileId);

				// Same corner/UV order as the default sprite mesh.
				const float corners[4][2] = {
					{ cx + half, cy + half }, // top right
					{ cx + half, cy - half }, // bottom right
					{ cx - half, cy - half }, // bottom left
					{ cx - half, cy + half }, // top left
				};
				const float uvs[4][2] = {
					{ 1.0f, 1.0f },
					{ 1.0f, 0.0f },
					{ 0.0f, 0.0f },
					{ 0.0f, 1.0f },
				};

				const unsigned int base = (unsigned int)(verts.size() / 5);
				for (int i = 0; i < 4; ++i)
				{
					verts.push_back(corners[i][0]);
					verts.push_back(corners[i][1]);
					verts.push_back(0.0f);
					verts.push_back(uvs[i][0] * uvScale.x + uvOff.x);
					verts.push_back(uvs[i][1] * uvScale.y + uvOff.y);
				}
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 3);
				indices.push_back(base + 1);
				indices.push_back(base + 2);
				indices.push_back(base + 3);
			}
		}

		mesh.indexCount = (int)indices.size();
		mesh.dirty = false;
		if (mesh.indexCount == 0)
			return;

		glGenVertexArrays(1, &mesh.vao);
		glGenBuffers(1, &mesh.vbo);
		glGenBuffers(1, &mesh.ebo);

		glBindVertexArray(mesh.vao);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void GameObject::DrawTilemap(const CameraProp& prop, Shader& shader)
	{
		(void)prop;
		if (!m_Texture.IsValid() || m_TilemapColumns <= 0 || m_TilemapRows <= 0)
			return;
		EnsureTileLayerMeshes();

		// Baked tiles use absolute UVs; object_matrix is the tilemap transform only.
		shader.SetUniform2fv(glm::vec2(1.0f, 1.0f), "uv_scale");
		shader.SetUniform2fv(glm::vec2(0.0f, 0.0f), "uv_offset");
		shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");

		m_Texture.Bind();

		for (int li = 0; li < (int)m_TileLayers.size(); ++li)
		{
			const TilemapLayer& layer = m_TileLayers[(size_t)li];
			if (!layer.visible)
				continue;

			TilemapLayerGpuMesh& mesh = m_TileLayerMeshes[(size_t)li];
			if (mesh.dirty)
				RebuildTilemapLayerMesh(li);
			if (mesh.indexCount <= 0 || mesh.vao == 0)
				continue;

			glm::vec4 tint = m_Color;
			tint.a *= glm::clamp(layer.opacity, 0.0f, 1.0f);
			shader.SetUniform4fv(tint, "color");

			glBindVertexArray(mesh.vao);
			glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
		}

		glBindVertexArray(0);
		m_Texture.UnBind();
	}

	int GameObject::GetTilesetTileCount() const
	{
		return glm::max(m_TilesetColumns, 1) * glm::max(m_TilesetRows, 1);
	}

	glm::vec2 GameObject::GetTilesetUVScale() const
	{
		const float cols = (float)glm::max(m_TilesetColumns, 1);
		const float rows = (float)glm::max(m_TilesetRows, 1);
		// Negative V: mesh has V=0 at bottom, stb textures have V=0 at top (no flip on load).
		return glm::vec2(1.0f / cols, -1.0f / rows);
	}

	glm::vec2 GameObject::GetTilesetUVOffset(int tileId) const
	{
		const int cols = glm::max(m_TilesetColumns, 1);
		const int rows = glm::max(m_TilesetRows, 1);
		const int total = cols * rows;
		const int id = glm::clamp(tileId, 0, total - 1);
		const int col = id % cols;
		const int rowFromTop = id / cols;
		// Matches ImGui tileset picker: row 0 = top row of the sheet.
		return glm::vec2(
			(float)col / (float)cols,
			(float)(rowFromTop + 1) / (float)rows);
	}

	void GameObject::InitTilemapDefaults()
	{
		DestroyTileLayerMeshes();
		m_TileCellSize = 32.0f;
		m_TilesetColumns = 1;
		m_TilesetRows = 1;
		m_TileCollisionEnabled = true;
		m_TilesetSolid.assign(1, 1); // default single tile is solid
		m_TileLayers.clear();
		m_ActiveTileLayer = 0;
		m_TilemapColumns = 0;
		m_TilemapRows = 0;
		AddTileLayer("Layer 0");
		SetTilemapSize(16, 12);
	}

	void GameObject::EnsureTileLayers()
	{
		if (!m_TileLayers.empty())
			return;
		TilemapLayer layer;
		layer.name = "Layer 0";
		const size_t expected = (size_t)glm::max(m_TilemapColumns, 0) * (size_t)glm::max(m_TilemapRows, 0);
		layer.tiles.assign(expected, -1);
		m_TileLayers.push_back(std::move(layer));
		m_ActiveTileLayer = 0;
		EnsureTileLayerMeshes();
		MarkTilemapMeshesDirty();
	}

	std::vector<int>& GameObject::ActiveTiles()
	{
		EnsureTileLayers();
		m_ActiveTileLayer = glm::clamp(m_ActiveTileLayer, 0, (int)m_TileLayers.size() - 1);
		return m_TileLayers[(size_t)m_ActiveTileLayer].tiles;
	}

	const std::vector<int>& GameObject::ActiveTiles() const
	{
		static const std::vector<int> empty;
		if (m_TileLayers.empty())
			return empty;
		const int index = glm::clamp(m_ActiveTileLayer, 0, (int)m_TileLayers.size() - 1);
		return m_TileLayers[(size_t)index].tiles;
	}

	void GameObject::SetActiveTileLayer(int index)
	{
		EnsureTileLayers();
		if (index < 0 || index >= (int)m_TileLayers.size())
			return;
		m_ActiveTileLayer = index;
	}

	int GameObject::AddTileLayer(const std::string& name)
	{
		TilemapLayer layer;
		layer.name = name.empty() ? "Layer" : name;
		const size_t expected = (size_t)glm::max(m_TilemapColumns, 0) * (size_t)glm::max(m_TilemapRows, 0);
		layer.tiles.assign(expected, -1);
		m_TileLayers.push_back(std::move(layer));
		m_ActiveTileLayer = (int)m_TileLayers.size() - 1;
		EnsureTileLayerMeshes();
		MarkTilemapLayerMeshDirty(m_ActiveTileLayer);
		return m_ActiveTileLayer;
	}

	bool GameObject::RemoveTileLayer(int index)
	{
		EnsureTileLayers();
		if ((int)m_TileLayers.size() <= 1)
			return false;
		if (index < 0 || index >= (int)m_TileLayers.size())
			return false;
		m_TileLayers.erase(m_TileLayers.begin() + index);
		if (m_ActiveTileLayer >= (int)m_TileLayers.size())
			m_ActiveTileLayer = (int)m_TileLayers.size() - 1;
		else if (m_ActiveTileLayer > index)
			--m_ActiveTileLayer;
		EnsureTileLayerMeshes();
		MarkTilemapMeshesDirty();
		return true;
	}

	void GameObject::ClearTileLayers()
	{
		m_TileLayers.clear();
		m_ActiveTileLayer = 0;
		DestroyTileLayerMeshes();
	}

	void GameObject::SetTileLayers(const std::vector<TilemapLayer>& layers, int activeIndex)
	{
		m_TileLayers = layers;
		if (m_TileLayers.empty())
			EnsureTileLayers();
		else
		{
			const size_t expected = (size_t)glm::max(m_TilemapColumns, 0) * (size_t)glm::max(m_TilemapRows, 0);
			for (TilemapLayer& layer : m_TileLayers)
			{
				if (layer.tiles.size() != expected)
				{
					std::vector<int> next(expected, -1);
					const size_t n = std::min(expected, layer.tiles.size());
					for (size_t i = 0; i < n; ++i)
						next[i] = layer.tiles[i];
					layer.tiles = std::move(next);
				}
			}
		}
		m_ActiveTileLayer = glm::clamp(activeIndex, 0, (int)m_TileLayers.size() - 1);
		EnsureTileLayerMeshes();
		MarkTilemapMeshesDirty();
	}

	const TilemapLayer* GameObject::GetTileLayer(int index) const
	{
		if (index < 0 || index >= (int)m_TileLayers.size())
			return nullptr;
		return &m_TileLayers[(size_t)index];
	}

	TilemapLayer* GameObject::GetTileLayerMutable(int index)
	{
		if (index < 0 || index >= (int)m_TileLayers.size())
			return nullptr;
		return &m_TileLayers[(size_t)index];
	}

	void GameObject::SetTileLayerName(int index, const std::string& name)
	{
		if (TilemapLayer* layer = GetTileLayerMutable(index))
			layer->name = name.empty() ? "Layer" : name;
	}

	void GameObject::SetTileLayerVisible(int index, bool visible)
	{
		if (TilemapLayer* layer = GetTileLayerMutable(index))
			layer->visible = visible;
	}

	void GameObject::SetTileLayerCollision(int index, bool collision)
	{
		if (TilemapLayer* layer = GetTileLayerMutable(index))
			layer->collision = collision;
	}

	void GameObject::SetTileLayerOpacity(int index, float opacity)
	{
		if (TilemapLayer* layer = GetTileLayerMutable(index))
			layer->opacity = glm::clamp(opacity, 0.0f, 1.0f);
	}

	void GameObject::SetTileLayerTiles(int index, const std::vector<int>& tiles)
	{
		TilemapLayer* layer = GetTileLayerMutable(index);
		if (!layer)
			return;
		const size_t expected = (size_t)glm::max(m_TilemapColumns, 0) * (size_t)glm::max(m_TilemapRows, 0);
		if (expected == 0)
			return;
		layer->tiles.assign(expected, -1);
		const size_t n = std::min(expected, tiles.size());
		for (size_t i = 0; i < n; ++i)
			layer->tiles[i] = tiles[i];
		MarkTilemapLayerMeshDirty(index);
	}

	const std::vector<int>& GameObject::GetTileLayerTiles(int index) const
	{
		static const std::vector<int> empty;
		const TilemapLayer* layer = GetTileLayer(index);
		return layer ? layer->tiles : empty;
	}

	const std::vector<int>& GameObject::GetTiles() const
	{
		return ActiveTiles();
	}

	void GameObject::RefreshTilemapBounds()
	{
		if (m_Type != ObjectType::Tilemap || m_TilemapColumns <= 0 || m_TilemapRows <= 0)
			return;
		const float cell = glm::max(m_TileCellSize, 1.0f);
		const float halfW = (float)m_TilemapColumns * cell * 0.5f;
		const float halfH = (float)m_TilemapRows * cell * 0.5f;
		m_LocalMin = glm::vec2(-halfW, -halfH);
		m_LocalMax = glm::vec2(halfW, halfH);
	}

	void GameObject::SetTilemapSize(int columns, int rows)
	{
		columns = glm::clamp(columns, 1, 512);
		rows = glm::clamp(rows, 1, 512);
		EnsureTileLayers();
		for (TilemapLayer& layer : m_TileLayers)
		{
			std::vector<int> next((size_t)columns * (size_t)rows, -1);
			for (int y = 0; y < std::min(rows, m_TilemapRows); ++y)
			{
				for (int x = 0; x < std::min(columns, m_TilemapColumns); ++x)
				{
					const int oldIdx = y * m_TilemapColumns + x;
					const int newIdx = y * columns + x;
					if (oldIdx >= 0 && oldIdx < (int)layer.tiles.size())
						next[newIdx] = layer.tiles[oldIdx];
				}
			}
			layer.tiles = std::move(next);
		}
		m_TilemapColumns = columns;
		m_TilemapRows = rows;
		RefreshTilemapBounds();
		MarkTilemapMeshesDirty();
	}

	void GameObject::SetTileCellSize(float size)
	{
		m_TileCellSize = glm::max(size, 1.0f);
		RefreshTilemapBounds();
		MarkTilemapMeshesDirty();
	}

	void GameObject::SetTilesetGrid(int columns, int rows)
	{
		m_TilesetColumns = glm::max(columns, 1);
		m_TilesetRows = glm::max(rows, 1);
		const int count = GetTilesetTileCount();
		std::vector<uint8_t> next((size_t)count, 0);
		const size_t n = std::min(next.size(), m_TilesetSolid.size());
		for (size_t i = 0; i < n; ++i)
			next[i] = m_TilesetSolid[i];
		m_TilesetSolid = std::move(next);
		MarkTilemapMeshesDirty();
	}

	void GameObject::SetTilesetSolid(int tileId, bool solid)
	{
		const int count = GetTilesetTileCount();
		if (tileId < 0 || tileId >= count)
			return;
		if ((int)m_TilesetSolid.size() != count)
			m_TilesetSolid.resize((size_t)count, 0);
		m_TilesetSolid[(size_t)tileId] = solid ? 1 : 0;
	}

	bool GameObject::IsTilesetSolid(int tileId) const
	{
		if (tileId < 0 || tileId >= (int)m_TilesetSolid.size())
			return false;
		return m_TilesetSolid[(size_t)tileId] != 0;
	}

	void GameObject::SetAllTilesetSolid(bool solid)
	{
		const int count = GetTilesetTileCount();
		m_TilesetSolid.assign((size_t)count, solid ? 1 : 0);
	}

	void GameObject::SetTilesetSolidFromList(const std::vector<uint8_t>& solids)
	{
		const int count = GetTilesetTileCount();
		m_TilesetSolid.assign((size_t)count, 0);
		const size_t n = std::min((size_t)count, solids.size());
		for (size_t i = 0; i < n; ++i)
			m_TilesetSolid[i] = solids[i] ? 1 : 0;
	}

	bool GameObject::IsPaintedTileSolid(int tileId) const
	{
		if (tileId < 0)
			return false;

		const int count = GetTilesetTileCount();
		if (count <= 0)
			return false;

		bool anyMarked = false;
		for (uint8_t flag : m_TilesetSolid)
		{
			if (flag)
			{
				anyMarked = true;
				break;
			}
		}

		// No solids marked yet => every painted tile collides (easy platformer default).
		if (!anyMarked)
			return true;

		if (tileId >= (int)m_TilesetSolid.size())
			return false;
		return m_TilesetSolid[(size_t)tileId] != 0;
	}

	bool GameObject::IsTileIdSolid(int tileId) const
	{
		if (!m_TileCollisionEnabled)
			return false;
		return IsPaintedTileSolid(tileId);
	}

	bool GameObject::IsSolidAt(int x, int y) const
	{
		if (!m_TileCollisionEnabled)
			return false;
		if (m_TileLayers.empty())
			return IsPaintedTileSolid(GetTile(x, y));

		for (const TilemapLayer& layer : m_TileLayers)
		{
			if (!layer.collision)
				continue;
			if (x < 0 || y < 0 || x >= m_TilemapColumns || y >= m_TilemapRows)
				continue;
			const int idx = y * m_TilemapColumns + x;
			if (idx < 0 || idx >= (int)layer.tiles.size())
				continue;
			if (IsPaintedTileSolid(layer.tiles[idx]))
				return true;
		}
		return false;
	}

	glm::vec2 GameObject::TileCenterLocal(int tileX, int tileY) const
	{
		const float cell = glm::max(m_TileCellSize, 1.0f);
		const float mapW = (float)m_TilemapColumns * cell;
		const float mapH = (float)m_TilemapRows * cell;
		return glm::vec2(
			((float)tileX + 0.5f) * cell - mapW * 0.5f,
			((float)tileY + 0.5f) * cell - mapH * 0.5f);
	}

	void GameObject::SetTile(int x, int y, int tileId)
	{
		SetTileOnLayer(m_ActiveTileLayer, x, y, tileId);
	}

	int GameObject::GetTile(int x, int y) const
	{
		return GetTileOnLayer(m_ActiveTileLayer, x, y);
	}

	void GameObject::SetTileOnLayer(int layerIndex, int x, int y, int tileId)
	{
		EnsureTileLayers();
		if (layerIndex < 0 || layerIndex >= (int)m_TileLayers.size())
			return;
		if (x < 0 || y < 0 || x >= m_TilemapColumns || y >= m_TilemapRows)
			return;
		const int idx = y * m_TilemapColumns + x;
		std::vector<int>& tiles = m_TileLayers[(size_t)layerIndex].tiles;
		if (idx < 0 || idx >= (int)tiles.size())
			return;
		if (tiles[idx] == tileId)
			return;
		tiles[idx] = tileId;
		MarkTilemapLayerMeshDirty(layerIndex);
	}

	int GameObject::GetTileOnLayer(int layerIndex, int x, int y) const
	{
		if (layerIndex < 0 || layerIndex >= (int)m_TileLayers.size())
			return -1;
		if (x < 0 || y < 0 || x >= m_TilemapColumns || y >= m_TilemapRows)
			return -1;
		const int idx = y * m_TilemapColumns + x;
		const std::vector<int>& tiles = m_TileLayers[(size_t)layerIndex].tiles;
		if (idx < 0 || idx >= (int)tiles.size())
			return -1;
		return tiles[idx];
	}

	void GameObject::FillTiles(int tileId)
	{
		for (int& t : ActiveTiles())
			t = tileId;
		MarkTilemapLayerMeshDirty(m_ActiveTileLayer);
	}

	void GameObject::ClearTiles()
	{
		FillTiles(-1);
	}

	void GameObject::SetTilesFromList(const std::vector<int>& tiles)
	{
		EnsureTileLayers();
		const size_t expected = (size_t)glm::max(m_TilemapColumns, 0) * (size_t)glm::max(m_TilemapRows, 0);
		if (expected == 0)
			return;
		std::vector<int>& dest = ActiveTiles();
		dest.assign(expected, -1);
		const size_t n = std::min(expected, tiles.size());
		for (size_t i = 0; i < n; ++i)
			dest[i] = tiles[i];
		MarkTilemapLayerMeshDirty(m_ActiveTileLayer);
	}

	bool GameObject::WorldToTile(const CameraProp& prop, const glm::vec2& worldPoint, int& outX, int& outY) const
	{
		if (m_Type != ObjectType::Tilemap || m_TilemapColumns <= 0 || m_TilemapRows <= 0)
			return false;

		const float sx = prop.projection[0][0];
		const float sy = prop.projection[1][1];
		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
			return false;

		const glm::vec3 worldPos = GetWorldPosition();
		const float worldRot = GetWorldRotation();
		const glm::vec2 worldScale = GetWorldScale();
		if (glm::abs(worldScale.x) < 1e-8f || glm::abs(worldScale.y) < 1e-8f)
			return false;

		// Inverse of GetWorldAABB mapping: world = pos + rotate(local * scale) * projectionScale
		glm::vec2 delta((worldPoint.x - worldPos.x) / sx, (worldPoint.y - worldPos.y) / sy);
		const float c = glm::cos(glm::radians(-worldRot));
		const float s = glm::sin(glm::radians(-worldRot));
		glm::vec2 local(c * delta.x - s * delta.y, s * delta.x + c * delta.y);
		local.x /= worldScale.x;
		local.y /= worldScale.y;

		const float cell = glm::max(m_TileCellSize, 1.0f);
		const float mapW = (float)m_TilemapColumns * cell;
		const float mapH = (float)m_TilemapRows * cell;
		const float fx = (local.x + mapW * 0.5f) / cell;
		const float fy = (local.y + mapH * 0.5f) / cell;
		const int tx = (int)std::floor(fx);
		const int ty = (int)std::floor(fy);
		if (tx < 0 || ty < 0 || tx >= m_TilemapColumns || ty >= m_TilemapRows)
			return false;

		outX = tx;
		outY = ty;
		return true;
	}

	glm::vec2 GameObject::TileCenterWorld(const CameraProp& prop, int tileX, int tileY) const
	{
		const float sx = prop.projection[0][0];
		const float sy = prop.projection[1][1];
		const float cell = glm::max(m_TileCellSize, 1.0f);
		const float mapW = (float)m_TilemapColumns * cell;
		const float mapH = (float)m_TilemapRows * cell;
		const float lx = ((float)tileX + 0.5f) * cell - mapW * 0.5f;
		const float ly = ((float)tileY + 0.5f) * cell - mapH * 0.5f;

		const glm::vec2 worldScale = GetWorldScale();
		const float worldRot = GetWorldRotation();
		glm::vec2 v(lx * worldScale.x, ly * worldScale.y);
		const float c = glm::cos(glm::radians(worldRot));
		const float s = glm::sin(glm::radians(worldRot));
		glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);
		const glm::vec3 worldPos = GetWorldPosition();
		return glm::vec2(worldPos.x + r.x * sx, worldPos.y + r.y * sy);
	}

	int GameObject::GetFlipbookTotalFrames() const
	{
		const int grid = glm::max(m_FlipColumns, 1) * glm::max(m_FlipRows, 1);
		if (m_FlipFrameCount > 0)
			return glm::clamp(m_FlipFrameCount, 1, grid);
		return grid;
	}

	void GameObject::GetFlipbookPlaybackRange(int& outStart, int& outEnd) const
	{
		const int total = GetFlipbookTotalFrames();
		const int last = glm::max(total - 1, 0);
		if (m_FlipActiveClip >= 0 && m_FlipActiveClip < (int)m_FlipClips.size())
		{
			outStart = glm::clamp(m_FlipClips[m_FlipActiveClip].startFrame, 0, last);
			outEnd = glm::clamp(m_FlipClips[m_FlipActiveClip].endFrame, outStart, last);
			return;
		}
		outStart = 0;
		outEnd = last;
	}

	float GameObject::GetFlipbookPlaybackFps() const
	{
		if (m_FlipActiveClip >= 0 && m_FlipActiveClip < (int)m_FlipClips.size())
		{
			const float clipFps = m_FlipClips[m_FlipActiveClip].fps;
			if (clipFps > 0.0f)
				return clipFps;
		}
		return m_FlipFps;
	}

	bool GameObject::GetFlipbookPlaybackLoop() const
	{
		if (m_FlipActiveClip >= 0 && m_FlipActiveClip < (int)m_FlipClips.size())
			return m_FlipClips[m_FlipActiveClip].loop;
		return m_FlipLoop;
	}

	const FlipbookClip* GameObject::GetFlipbookClip(int index) const
	{
		if (index < 0 || index >= (int)m_FlipClips.size())
			return nullptr;
		return &m_FlipClips[index];
	}

	const FlipbookClip* GameObject::GetFlipbookClip(const std::string& name) const
	{
		const int index = FindFlipbookClip(name);
		return index >= 0 ? &m_FlipClips[index] : nullptr;
	}

	std::string GameObject::GetActiveFlipbookClipName() const
	{
		if (m_FlipActiveClip < 0 || m_FlipActiveClip >= (int)m_FlipClips.size())
			return {};
		return m_FlipClips[m_FlipActiveClip].name;
	}

	int GameObject::FindFlipbookClip(const std::string& name) const
	{
		for (int i = 0; i < (int)m_FlipClips.size(); ++i)
		{
			if (m_FlipClips[i].name == name)
				return i;
		}
		return -1;
	}

	void GameObject::NormalizeFlipbookClip(FlipbookClip& clip) const
	{
		const int total = GetFlipbookTotalFrames();
		const int last = glm::max(total - 1, 0);
		clip.startFrame = glm::clamp(clip.startFrame, 0, last);
		clip.endFrame = glm::clamp(clip.endFrame, clip.startFrame, last);
		if (clip.name.empty())
			clip.name = "Clip";
	}

	std::string GameObject::MakeUniqueFlipbookClipName(const std::string& desired) const
	{
		std::string base = desired.empty() ? "Clip" : desired;
		if (FindFlipbookClip(base) < 0)
			return base;
		for (int i = 2; i < 10000; ++i)
		{
			const std::string candidate = base + "_" + std::to_string(i);
			if (FindFlipbookClip(candidate) < 0)
				return candidate;
		}
		return base + "_x";
	}

	void GameObject::ClampFlipbookFrame()
	{
		int start = 0;
		int end = 0;
		GetFlipbookPlaybackRange(start, end);
		if (m_FlipFrame < start)
			m_FlipFrame = start;
		if (m_FlipFrame > end)
			m_FlipFrame = end;
	}

	glm::vec2 GameObject::GetFlipbookUVScale() const
	{
		const float cols = (float)glm::max(m_FlipColumns, 1);
		const float rows = (float)glm::max(m_FlipRows, 1);
		return glm::vec2(1.0f / cols, 1.0f / rows);
	}

	glm::vec2 GameObject::GetFlipbookUVOffset() const
	{
		const int cols = glm::max(m_FlipColumns, 1);
		const int rows = glm::max(m_FlipRows, 1);
		const int frame = glm::clamp(m_FlipFrame, 0, GetFlipbookTotalFrames() - 1);
		const int col = frame % cols;
		const int rowFromTop = frame / cols;
		return glm::vec2(
			(float)col / (float)cols,
			1.0f - ((float)(rowFromTop + 1) / (float)rows));
	}

	void GameObject::UpdateAnimation(float dt)
	{
		m_FlipJustFinished = false;
		if (!m_FlipPlaying || dt <= 0.0f)
		{
			UpdateAnimStateMachine();
			return;
		}
		if (m_Type == ObjectType::Camera || m_Type == ObjectType::Sound || m_Type == ObjectType::Tilemap || m_Type == ObjectType::Text)
			return;

		int start = 0;
		int end = 0;
		GetFlipbookPlaybackRange(start, end);
		const int span = end - start + 1;
		if (span <= 1)
		{
			UpdateAnimStateMachine();
			return;
		}

		const float fps = glm::max(GetFlipbookPlaybackFps(), 0.01f);
		const bool loop = GetFlipbookPlaybackLoop();
		m_FlipTime += dt;
		const float frameDuration = 1.0f / fps;
		while (m_FlipTime >= frameDuration)
		{
			m_FlipTime -= frameDuration;
			++m_FlipFrame;
			if (m_FlipFrame > end)
			{
				if (loop)
					m_FlipFrame = start;
				else
				{
					m_FlipFrame = end;
					m_FlipPlaying = false;
					m_FlipTime = 0.0f;
					m_FlipJustFinished = true;
					break;
				}
			}
		}

		UpdateAnimStateMachine();
	}

	void GameObject::SetFlipbookColumns(int columns)
	{
		m_FlipColumns = glm::max(columns, 1);
		for (FlipbookClip& clip : m_FlipClips)
			NormalizeFlipbookClip(clip);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookRows(int rows)
	{
		m_FlipRows = glm::max(rows, 1);
		for (FlipbookClip& clip : m_FlipClips)
			NormalizeFlipbookClip(clip);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookFrameCount(int count)
	{
		m_FlipFrameCount = glm::max(count, 0);
		for (FlipbookClip& clip : m_FlipClips)
			NormalizeFlipbookClip(clip);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookFps(float fps)
	{
		m_FlipFps = glm::max(fps, 0.01f);
	}

	void GameObject::SetFlipbookPlaying(bool playing)
	{
		m_FlipPlaying = playing;
		if (playing)
			m_FlipTime = 0.0f;
	}

	void GameObject::SetFlipbookLoop(bool loop)
	{
		m_FlipLoop = loop;
	}

	void GameObject::SetFlipbookFrame(int frame)
	{
		m_FlipFrame = frame;
		ClampFlipbookFrame();
		m_FlipTime = 0.0f;
	}

	void GameObject::PlayFlipbook()
	{
		m_FlipPlaying = true;
		m_FlipTime = 0.0f;
	}

	bool GameObject::PlayFlipbook(const std::string& clipName)
	{
		if (!SetActiveFlipbookClip(clipName))
			return false;
		int start = 0;
		int end = 0;
		GetFlipbookPlaybackRange(start, end);
		(void)end;
		m_FlipFrame = start;
		m_FlipPlaying = true;
		m_FlipTime = 0.0f;
		return true;
	}

	void GameObject::StopFlipbook()
	{
		m_FlipPlaying = false;
		int start = 0;
		int end = 0;
		GetFlipbookPlaybackRange(start, end);
		(void)end;
		m_FlipFrame = start;
		m_FlipTime = 0.0f;
	}

	void GameObject::PauseFlipbook()
	{
		m_FlipPlaying = false;
	}

	int GameObject::AddFlipbookClip(const std::string& name, int startFrame, int endFrame, float fps, bool loop)
	{
		FlipbookClip clip;
		clip.name = MakeUniqueFlipbookClipName(name);
		clip.startFrame = startFrame;
		clip.endFrame = endFrame < startFrame ? startFrame : endFrame;
		clip.fps = fps;
		clip.loop = loop;
		NormalizeFlipbookClip(clip);
		m_FlipClips.push_back(clip);
		return (int)m_FlipClips.size() - 1;
	}

	bool GameObject::SetFlipbookClip(const std::string& name, int startFrame, int endFrame, float fps, bool loop)
	{
		const int index = FindFlipbookClip(name);
		if (index < 0)
			return false;
		FlipbookClip& clip = m_FlipClips[index];
		clip.startFrame = startFrame;
		clip.endFrame = endFrame < startFrame ? startFrame : endFrame;
		clip.fps = fps;
		clip.loop = loop;
		NormalizeFlipbookClip(clip);
		if (m_FlipActiveClip == index)
			ClampFlipbookFrame();
		return true;
	}

	bool GameObject::RemoveFlipbookClip(const std::string& name)
	{
		const int index = FindFlipbookClip(name);
		if (index < 0)
			return false;
		return RemoveFlipbookClipAt(index);
	}

	bool GameObject::RemoveFlipbookClipAt(int index)
	{
		if (index < 0 || index >= (int)m_FlipClips.size())
			return false;
		m_FlipClips.erase(m_FlipClips.begin() + index);
		if (m_FlipActiveClip == index)
			m_FlipActiveClip = -1;
		else if (m_FlipActiveClip > index)
			--m_FlipActiveClip;
		ClampFlipbookFrame();
		return true;
	}

	void GameObject::ClearFlipbookClips()
	{
		m_FlipClips.clear();
		m_FlipActiveClip = -1;
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookClips(const std::vector<FlipbookClip>& clips)
	{
		const std::string activeName = GetActiveFlipbookClipName();
		m_FlipClips = clips;
		for (size_t i = 0; i < m_FlipClips.size(); ++i)
		{
			if (m_FlipClips[i].name.empty())
				m_FlipClips[i].name = "Clip_" + std::to_string(i + 1);
			NormalizeFlipbookClip(m_FlipClips[i]);
		}
		m_FlipActiveClip = activeName.empty() ? -1 : FindFlipbookClip(activeName);
		ClampFlipbookFrame();
	}

	bool GameObject::SetActiveFlipbookClip(const std::string& name)
	{
		const int index = FindFlipbookClip(name);
		if (index < 0)
			return false;
		m_FlipActiveClip = index;
		ClampFlipbookFrame();
		return true;
	}

	void GameObject::ClearActiveFlipbookClip()
	{
		m_FlipActiveClip = -1;
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookAssetPath(const std::string& path, bool loadAsset)
	{
		m_FlipbookAssetPath = path;
		if (loadAsset && !m_FlipbookAssetPath.empty())
			LoadFlipbookAsset();
	}

	bool GameObject::ApplyFlipbookAsset(const FlipbookAsset& asset)
	{
		const std::string active = GetActiveFlipbookClipName();
		m_FlipColumns = asset.GetColumns();
		m_FlipRows = asset.GetRows();
		m_FlipFrameCount = asset.GetFrameCount();
		m_FlipFps = asset.GetFps();
		SetFlipbookClips(asset.GetClips());
		if (!asset.GetPath().empty())
			m_FlipbookAssetPath = asset.GetPath();
		if (!active.empty())
			SetActiveFlipbookClip(active);
		return true;
	}

	bool GameObject::LoadFlipbookAsset()
	{
		if (m_FlipbookAssetPath.empty())
			return false;
		FlipbookAsset asset;
		if (!asset.LoadFromFile(m_FlipbookAssetPath))
			return false;
		return ApplyFlipbookAsset(asset);
	}

	bool GameObject::SaveFlipbookAsset() const
	{
		if (m_FlipbookAssetPath.empty())
			return false;
		FlipbookAsset asset;
		asset.SetPath(m_FlipbookAssetPath);
		asset.SetColumns(m_FlipColumns);
		asset.SetRows(m_FlipRows);
		asset.SetFrameCount(m_FlipFrameCount);
		asset.SetFps(m_FlipFps);
		asset.SetClips(m_FlipClips);
		return asset.SaveToFile(m_FlipbookAssetPath);
	}

	bool GameObject::SaveFlipbookAssetAs(const std::string& path)
	{
		if (path.empty())
			return false;
		m_FlipbookAssetPath = path;
		return SaveFlipbookAsset();
	}

	void GameObject::AnimResetParams()
	{
		m_AnimBools.clear();
		m_AnimFloats.clear();
		m_AnimTriggers.clear();
		for (const AnimParam& param : m_AnimAsset.GetParams())
		{
			if (param.type == AnimParamType::Bool)
				m_AnimBools[param.name] = param.boolDefault;
			else if (param.type == AnimParamType::Float)
				m_AnimFloats[param.name] = param.floatDefault;
			else
				m_AnimTriggers[param.name] = false;
		}
	}

	bool GameObject::EnterAnimState(const std::string& stateName, bool forceRestart)
	{
		const int index = m_AnimAsset.FindState(stateName);
		if (index < 0)
			return false;
		const AnimState& state = m_AnimAsset.GetStates()[index];
		if (!forceRestart && m_AnimCurrentState == state.name)
			return true;

		m_AnimCurrentState = state.name;
		m_FlipJustFinished = false;
		if (!state.clipName.empty() && FindFlipbookClip(state.clipName) >= 0)
			PlayFlipbook(state.clipName);
		else if (!state.clipName.empty())
		{
			// Clip missing — still mark state so transitions can recover.
			StopFlipbook();
		}
		return true;
	}

	bool GameObject::TransitionConditionMet(const AnimTransition& transition) const
	{
		if (!transition.onFinished && !transition.hasBool && !transition.hasTrigger && !transition.hasFloat)
			return false;

		if (transition.onFinished && !m_FlipJustFinished)
			return false;

		if (transition.hasBool)
		{
			auto it = m_AnimBools.find(transition.boolName);
			const bool value = (it != m_AnimBools.end()) ? it->second : false;
			if (value != transition.boolEquals)
				return false;
		}

		if (transition.hasTrigger)
		{
			auto it = m_AnimTriggers.find(transition.triggerName);
			if (it == m_AnimTriggers.end() || !it->second)
				return false;
		}

		if (transition.hasFloat)
		{
			auto it = m_AnimFloats.find(transition.floatName);
			const float value = (it != m_AnimFloats.end()) ? it->second : 0.0f;
			switch (transition.floatCmp)
			{
			case AnimFloatCmp::Less:
				if (!(value < transition.floatValue)) return false;
				break;
			case AnimFloatCmp::GreaterEqual:
				if (!(value >= transition.floatValue)) return false;
				break;
			case AnimFloatCmp::LessEqual:
				if (!(value <= transition.floatValue)) return false;
				break;
			case AnimFloatCmp::Equal:
				if (glm::abs(value - transition.floatValue) > 1e-4f) return false;
				break;
			default:
				if (!(value > transition.floatValue)) return false;
				break;
			}
		}

		return true;
	}

	void GameObject::ClearConsumedTriggers(const AnimTransition& transition)
	{
		if (transition.hasTrigger)
			m_AnimTriggers[transition.triggerName] = false;
	}

	void GameObject::UpdateAnimStateMachine()
	{
		if (!m_AnimEnabled || m_AnimAssetPath.empty() || m_AnimAsset.GetStates().empty())
			return;

		if (m_AnimCurrentState.empty())
		{
			const std::string& def = m_AnimAsset.GetDefaultState();
			if (!def.empty())
				EnterAnimState(def, true);
			else
				EnterAnimState(m_AnimAsset.GetStates().front().name, true);
			return;
		}

		for (const AnimTransition& transition : m_AnimAsset.GetTransitions())
		{
			const bool fromMatch = (transition.from == "*") || (transition.from == m_AnimCurrentState);
			if (!fromMatch)
				continue;
			if (transition.to == m_AnimCurrentState && !transition.onFinished)
				continue;
			if (!TransitionConditionMet(transition))
				continue;

			ClearConsumedTriggers(transition);
			EnterAnimState(transition.to, true);
			break;
		}
	}

	void GameObject::SetAnimAssetPath(const std::string& path, bool loadAsset)
	{
		m_AnimAssetPath = path;
		if (path.empty())
		{
			m_AnimEnabled = false;
			m_AnimCurrentState.clear();
			m_AnimAsset = AnimStateMachineAsset{};
			AnimResetParams();
			return;
		}
		if (loadAsset)
			LoadAnimAsset();
	}

	bool GameObject::ApplyAnimAsset(const AnimStateMachineAsset& asset)
	{
		m_AnimAsset = asset;
		if (!m_AnimAssetPath.empty())
			m_AnimAsset.SetPath(m_AnimAssetPath);
		AnimResetParams();
		m_AnimEnabled = true;
		m_AnimCurrentState.clear();
		AnimRestart();
		return true;
	}

	bool GameObject::LoadAnimAsset()
	{
		if (m_AnimAssetPath.empty())
			return false;
		AnimStateMachineAsset asset;
		if (!asset.LoadFromFile(m_AnimAssetPath))
			return false;
		return ApplyAnimAsset(asset);
	}

	bool GameObject::SaveAnimAsset() const
	{
		if (m_AnimAssetPath.empty())
			return false;
		return m_AnimAsset.SaveToFile(m_AnimAssetPath);
	}

	bool GameObject::SaveAnimAssetAs(const std::string& path)
	{
		if (path.empty())
			return false;
		m_AnimAssetPath = path;
		m_AnimAsset.SetPath(path);
		return SaveAnimAsset();
	}

	void GameObject::SetAnimEnabled(bool enabled)
	{
		m_AnimEnabled = enabled;
		if (enabled)
			AnimRestart();
	}

	bool GameObject::AnimPlay(const std::string& stateName)
	{
		if (!m_AnimEnabled && !m_AnimAssetPath.empty())
			m_AnimEnabled = true;
		return EnterAnimState(stateName, true);
	}

	void GameObject::AnimSetBool(const std::string& name, bool value)
	{
		m_AnimBools[name] = value;
	}

	void GameObject::AnimSetTrigger(const std::string& name)
	{
		m_AnimTriggers[name] = true;
	}

	void GameObject::AnimSetFloat(const std::string& name, float value)
	{
		m_AnimFloats[name] = value;
	}

	bool GameObject::AnimGetBool(const std::string& name) const
	{
		auto it = m_AnimBools.find(name);
		return it != m_AnimBools.end() ? it->second : false;
	}

	float GameObject::AnimGetFloat(const std::string& name) const
	{
		auto it = m_AnimFloats.find(name);
		return it != m_AnimFloats.end() ? it->second : 0.0f;
	}

	void GameObject::AnimRestart()
	{
		if (!m_AnimEnabled || m_AnimAsset.GetStates().empty())
			return;
		AnimResetParams();
		const std::string& def = m_AnimAsset.GetDefaultState();
		if (!def.empty())
			EnterAnimState(def, true);
		else
			EnterAnimState(m_AnimAsset.GetStates().front().name, true);
	}

	glm::vec3 GameObject::GetWorldPosition() const
	{
		if (!m_Parent)
			return m_Pos;

		const glm::vec3 parentPos = m_Parent->GetWorldPosition();
		const float parentRot = m_Parent->GetWorldRotation();
		const glm::vec2 parentScale = m_Parent->GetWorldScale();

		glm::vec2 local(m_Pos.x * parentScale.x, m_Pos.y * parentScale.y);
		const float rad = glm::radians(parentRot);
		const float c = glm::cos(rad);
		const float s = glm::sin(rad);
		const glm::vec2 rotated(local.x * c - local.y * s, local.x * s + local.y * c);
		return glm::vec3(parentPos.x + rotated.x, parentPos.y + rotated.y, parentPos.z + m_Pos.z);
	}

	float GameObject::GetWorldRotation() const
	{
		return m_Parent ? (m_Parent->GetWorldRotation() + m_Rotation) : m_Rotation;
	}

	glm::vec2 GameObject::GetWorldScale() const
	{
		return m_Parent ? (m_Parent->GetWorldScale() * m_Scale) : m_Scale;
	}

	void GameObject::SetLocalPosition(const glm::vec3& pos)
	{
		m_Pos = pos;
		RebuildMatrix();
	}

	void GameObject::SetLocalRotation(float degrees)
	{
		m_Rotation = degrees;
		RebuildMatrix();
	}

	void GameObject::SetLocalScale(glm::vec2 scale)
	{
		m_Scale = scale;
		RebuildMatrix();
	}

	void GameObject::SetActive(bool active, bool recurseChildren)
	{
		m_Active = active;
		m_Visible = active;
		if (!recurseChildren)
			return;
		for (GameObject* child : m_Children)
		{
			if (child)
				child->SetActive(active, true);
		}
	}

	void GameObject::SetPosition(glm::vec3& pos)
	{
		if (!m_Parent)
		{
			m_Pos = pos;
			RebuildMatrix();
			return;
		}

		const glm::vec3 parentPos = m_Parent->GetWorldPosition();
		const float parentRot = m_Parent->GetWorldRotation();
		const glm::vec2 parentScale = m_Parent->GetWorldScale();

		glm::vec2 delta(pos.x - parentPos.x, pos.y - parentPos.y);
		const float rad = -glm::radians(parentRot);
		const float c = glm::cos(rad);
		const float s = glm::sin(rad);
		glm::vec2 unrot(delta.x * c - delta.y * s, delta.x * s + delta.y * c);
		if (glm::abs(parentScale.x) > 1e-6f) unrot.x /= parentScale.x;
		if (glm::abs(parentScale.y) > 1e-6f) unrot.y /= parentScale.y;
		m_Pos = glm::vec3(unrot.x, unrot.y, pos.z - parentPos.z);
		RebuildMatrix();
	}

	void GameObject::Translate(glm::vec3& translate)
	{
		glm::vec3 world = GetWorldPosition() + translate;
		SetPosition(world);
	}

	void GameObject::SetRotation(float degrees)
	{
		if (!m_Parent)
			m_Rotation = degrees;
		else
			m_Rotation = degrees - m_Parent->GetWorldRotation();
		RebuildMatrix();
	}

	void GameObject::Rotate(float degrees)
	{
		m_Rotation += degrees;
		RebuildMatrix();
	}

	void GameObject::SetScale(glm::vec2 scale)
	{
		if (!m_Parent)
		{
			m_Scale = scale;
		}
		else
		{
			glm::vec2 parentScale = m_Parent->GetWorldScale();
			m_Scale.x = (glm::abs(parentScale.x) > 1e-6f) ? (scale.x / parentScale.x) : scale.x;
			m_Scale.y = (glm::abs(parentScale.y) > 1e-6f) ? (scale.y / parentScale.y) : scale.y;
		}
		RebuildMatrix();
	}

	bool GameObject::WouldCreateParentCycle(GameObject* parent) const
	{
		for (GameObject* p = parent; p; p = p->m_Parent)
		{
			if (p == this)
				return true;
		}
		return false;
	}

	void GameObject::RemoveFromParentChildren()
	{
		if (!m_Parent)
			return;
		auto& siblings = m_Parent->m_Children;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
		m_Parent = nullptr;
	}

	void GameObject::DetachFromHierarchy()
	{
		std::vector<GameObject*> children = m_Children;
		for (GameObject* child : children)
		{
			if (child)
				child->SetParent(nullptr, true);
		}
		RemoveFromParentChildren();
	}

	void GameObject::ClearParent(bool keepWorld)
	{
		SetParent(nullptr, keepWorld);
	}

	bool GameObject::SetParent(GameObject* parent, bool keepWorld)
	{
		if (parent == this)
			return false;
		if (parent == m_Parent)
			return true;
		if (WouldCreateParentCycle(parent))
			return false;

		glm::vec3 worldPos = GetWorldPosition();
		float worldRot = GetWorldRotation();
		glm::vec2 worldScale = GetWorldScale();

		RemoveFromParentChildren();
		m_Parent = parent;
		if (m_Parent)
			m_Parent->m_Children.push_back(this);

		if (keepWorld)
		{
			SetRotation(worldRot);
			SetScale(worldScale);
			SetPosition(worldPos);
		}
		else
		{
			RebuildMatrix();
		}
		return true;
	}

	void GameObject::SetColor(const glm::vec4& color)
	{
		m_Color = color;
	}

	void GameObject::SetColor(float r, float g, float b, float a)
	{
		m_Color = glm::vec4(r, g, b, a);
	}

	void GameObject::SetType(ObjectType type)
	{
		m_Type = type;
		if (type == ObjectType::Sound)
		{
			m_HasCollision = false;
			m_DrivesView = false;
		}
		else if (type == ObjectType::Light)
		{
			m_HasCollision = false;
			m_DrivesView = false;
			if (m_LightRadius < 1.0f)
				m_LightRadius = 300.0f;
			if (m_LightIntensity < 0.0f)
				m_LightIntensity = 1.0f;
			m_LightEnabled = true;
			if (m_LightOuterAngle < m_LightInnerAngle)
				m_LightOuterAngle = m_LightInnerAngle;
			if (m_Color.r > 0.99f && m_Color.g > 0.99f && m_Color.b > 0.99f)
				m_Color = glm::vec4(1.0f, 0.92f, 0.75f, 1.0f);
			m_LocalMin = glm::vec2(-24.0f, -24.0f);
			m_LocalMax = glm::vec2(24.0f, 24.0f);
		}
		else if (type == ObjectType::PostProcess)
		{
			m_HasCollision = false;
			m_DrivesView = false;
			m_PostProcessEnabled = true;
			m_LocalMin = glm::vec2(-20.0f, -20.0f);
			m_LocalMax = glm::vec2(20.0f, 20.0f);
		}
		else if (type == ObjectType::Tilemap)
		{
			m_DrivesView = false;
			if (m_TilemapColumns <= 0 || m_TilemapRows <= 0)
				InitTilemapDefaults();
			else
				RefreshTilemapBounds();
		}
		else if (type == ObjectType::Text)
		{
			m_DrivesView = false;
			if (m_Text.empty())
				m_Text = "Text";
			if (m_FontSize < 1.0f)
				m_FontSize = 48.0f;
			RefreshTextBounds();
		}
	}

	void GameObject::SetText(const std::string& text)
	{
		m_Text = text;
		if (m_Type == ObjectType::Text)
			RefreshTextBounds();
	}

	void GameObject::SetFontSize(float size)
	{
		m_FontSize = glm::max(size, 1.0f);
		if (m_Type == ObjectType::Text)
			RefreshTextBounds();
	}

	void GameObject::SetFontPath(const std::string& path)
	{
		m_FontPath = path;
		if (m_Type == ObjectType::Text)
			RefreshTextBounds();
	}

	void GameObject::RefreshTextBounds()
	{
		if (m_Type != ObjectType::Text)
			return;
		if (!UISystem::IsReady())
			UISystem::Init();
		const float w = UISystem::MeasureTextWidth(m_Text.empty() ? " " : m_Text, m_FontSize, m_FontPath);
		const float h = m_FontSize;
		m_LocalMin = glm::vec2(-w * 0.5f, -h * 0.5f);
		m_LocalMax = glm::vec2(w * 0.5f, h * 0.5f);
	}

	void GameObject::SetZoom(float zoom)
	{
		m_Zoom = glm::max(zoom, 0.05f);
	}

	void GameObject::SetLightRadius(float radius)
	{
		m_LightRadius = glm::max(radius, 1.0f);
	}

	void GameObject::SetLightIntensity(float intensity)
	{
		m_LightIntensity = glm::max(intensity, 0.0f);
	}

	void GameObject::SetLightEnabled(bool enabled)
	{
		m_LightEnabled = enabled;
	}

	void GameObject::SetLightKind(LightKind kind)
	{
		m_LightKind = kind;
	}

	void GameObject::SetLightInnerAngle(float degrees)
	{
		m_LightInnerAngle = glm::clamp(degrees, 0.5f, 179.0f);
		if (m_LightOuterAngle < m_LightInnerAngle)
			m_LightOuterAngle = m_LightInnerAngle;
	}

	void GameObject::SetLightOuterAngle(float degrees)
	{
		m_LightOuterAngle = glm::clamp(degrees, 0.5f, 179.0f);
		if (m_LightOuterAngle < m_LightInnerAngle)
			m_LightInnerAngle = m_LightOuterAngle;
	}

	glm::vec2 GameObject::GetLightDirection() const
	{
		const float rad = glm::radians(GetWorldRotation());
		return glm::vec2(glm::cos(rad), glm::sin(rad));
	}

	void GameObject::SetPostProcessEnabled(bool enabled)
	{
		m_PostProcessEnabled = enabled;
	}

	void GameObject::SetPostProcessBrightness(float brightness)
	{
		m_PostProcessBrightness = brightness;
	}

	void GameObject::SetPostProcessContrast(float contrast)
	{
		m_PostProcessContrast = glm::max(contrast, 0.0f);
	}

	void GameObject::SetPostProcessSaturation(float saturation)
	{
		m_PostProcessSaturation = glm::max(saturation, 0.0f);
	}

	void GameObject::SetPostProcessVignette(float vignette)
	{
		m_PostProcessVignette = glm::clamp(vignette, 0.0f, 2.0f);
	}

	void GameObject::SetPostProcessTint(float r, float g, float b)
	{
		m_PostProcessTint = glm::max(glm::vec3(r, g, b), glm::vec3(0.0f));
	}

	void GameObject::SetPostProcessTint(const glm::vec3& tint)
	{
		m_PostProcessTint = glm::max(tint, glm::vec3(0.0f));
	}

	glm::mat4 GameObject::GetCameraViewMatrix() const
	{
		const float zoom = glm::max(m_Zoom, 1e-4f);
		const glm::vec3 worldPos = GetWorldPosition();
		const float worldRot = GetWorldRotation();
		glm::mat4 view(1.0f);
		view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
		view = glm::rotate(view, -glm::radians(worldRot), glm::vec3(0.0f, 0.0f, 1.0f));
		view = glm::translate(view, -worldPos);
		return view;
	}

	void GameObject::SetLayer(int layer)
	{
		m_Layer = layer;
	}

	static std::string TrimTag(const std::string& value)
	{
		size_t start = 0;
		while (start < value.size() && std::isspace((unsigned char)value[start]))
			++start;
		size_t end = value.size();
		while (end > start && std::isspace((unsigned char)value[end - 1]))
			--end;
		return value.substr(start, end - start);
	}

	void GameObject::SetTag(const std::string& tag)
	{
		m_Tags.clear();
		std::string trimmed = TrimTag(tag);
		if (!trimmed.empty())
			m_Tags.push_back(trimmed);
	}

	void GameObject::AddTag(const std::string& tag)
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty() || HasTag(trimmed))
			return;
		m_Tags.push_back(trimmed);
	}

	void GameObject::RemoveTag(const std::string& tag)
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
			return;
		m_Tags.erase(std::remove(m_Tags.begin(), m_Tags.end(), trimmed), m_Tags.end());
	}

	void GameObject::RemoveTagAt(size_t index)
	{
		if (index >= m_Tags.size())
			return;
		m_Tags.erase(m_Tags.begin() + (std::ptrdiff_t)index);
	}

	void GameObject::SetTagAt(size_t index, const std::string& tag)
	{
		if (index >= m_Tags.size())
			return;

		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
		{
			RemoveTagAt(index);
			return;
		}

		for (size_t i = 0; i < m_Tags.size(); ++i)
		{
			if (i != index && m_Tags[i] == trimmed)
				return;
		}

		m_Tags[index] = trimmed;
	}

	void GameObject::ClearTags()
	{
		m_Tags.clear();
	}

	void GameObject::SetTagsFromString(const std::string& tagsCsv)
	{
		m_Tags.clear();
		std::string token;
		for (size_t i = 0; i <= tagsCsv.size(); ++i)
		{
			char c = (i < tagsCsv.size()) ? tagsCsv[i] : ',';
			if (c == ',' || c == ';' || i == tagsCsv.size())
			{
				std::string trimmed = TrimTag(token);
				if (!trimmed.empty() && !HasTag(trimmed))
					m_Tags.push_back(trimmed);
				token.clear();
			}
			else
			{
				token.push_back(c);
			}
		}
	}

	bool GameObject::HasTag(const std::string& tag) const
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
			return false;
		for (const std::string& existing : m_Tags)
		{
			if (existing == trimmed)
				return true;
		}
		return false;
	}

	std::string GameObject::GetTag() const
	{
		return m_Tags.empty() ? std::string() : m_Tags.front();
	}

	std::string GameObject::GetTagsString() const
	{
		std::string out;
		for (size_t i = 0; i < m_Tags.size(); ++i)
		{
			if (i > 0)
				out += ", ";
			out += m_Tags[i];
		}
		return out;
	}

	void GameObject::AddCollision(CollisionType type)
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
			return;
		m_HasCollision = true;
		SetCollisionType(type);
	}

	void GameObject::RemoveCollision()
	{
		m_HasCollision = false;
	}

	void GameObject::SetCollisionType(CollisionType type)
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
			return;
		m_CollisionType = type;
		m_HasCollision = true;
		if (type == CollisionType::Circle)
			m_ColliderShape = ColliderShape::Circle;
		// Block / PhysicsRigidbody2D keep the current ColliderShape (Circle/Block/Capsule/Polygon).
	}

	void GameObject::SetColliderShape(ColliderShape shape)
	{
		m_ColliderShape = shape;
		if (shape == ColliderShape::Polygon && m_ColliderVertices.size() < 3)
			ResetColliderVerticesToBox();
		if (shape == ColliderShape::Capsule)
			SetCapsuleHeight(GetCapsuleHeight());
	}

	void GameObject::SetCollisionRadius(float radius)
	{
		m_CollisionRadius = glm::max(radius, 0.01f);
		if (m_ColliderShape == ColliderShape::Capsule)
			SetCapsuleHeight(GetCapsuleHeight());
	}

	void GameObject::SetCollisionSize(glm::vec2 size)
	{
		m_CollisionSize.x = glm::max(size.x, 0.01f);
		m_CollisionSize.y = glm::max(size.y, 0.01f);
		if (m_ColliderShape == ColliderShape::Capsule)
			SetCapsuleHeight(m_CollisionSize.y);
	}

	float GameObject::GetCapsuleHeight() const
	{
		return glm::max(m_CollisionSize.y, m_CollisionRadius * 2.0f);
	}

	void GameObject::SetCapsuleHeight(float height)
	{
		m_CollisionSize.y = glm::max(height, m_CollisionRadius * 2.0f);
	}

	void GameObject::ResetColliderVerticesToBox()
	{
		const glm::vec2 half = m_CollisionSize * 0.5f;
		m_ColliderVertices = {
			{ -half.x, -half.y },
			{  half.x, -half.y },
			{  half.x,  half.y },
			{ -half.x,  half.y },
		};
	}

	void GameObject::ClearColliderVertices()
	{
		m_ColliderVertices.clear();
	}

	void GameObject::SetColliderVertices(const std::vector<glm::vec2>& localVerts)
	{
		m_ColliderVertices.clear();
		const size_t n = std::min<size_t>(localVerts.size(), 8);
		for (size_t i = 0; i < n; ++i)
			m_ColliderVertices.push_back(localVerts[i]);
		if (m_ColliderVertices.size() >= 3)
			m_ColliderShape = ColliderShape::Polygon;
	}

	void GameObject::SetRigidbodyMass(float mass)
	{
		m_RigidbodyMass = glm::max(mass, 0.01f);
	}

	void GameObject::SetRigidbodyGravityScale(float scale)
	{
		m_RigidbodyGravityScale = scale;
	}

	void GameObject::SetRigidbodyKinematic(bool kinematic)
	{
		m_RigidbodyKinematic = kinematic;
	}

	void GameObject::SetIsTrigger(bool trigger)
	{
		m_IsTrigger = trigger;
	}

	void GameObject::SetCollisionLayer(int layer)
	{
		m_CollisionLayer = glm::clamp(layer, 0, 31);
	}

	void GameObject::SetCollisionMask(uint32_t mask)
	{
		m_CollisionMask = mask;
	}

	void GameObject::SetCollidesWithLayer(int layer, bool enabled)
	{
		layer = glm::clamp(layer, 0, 31);
		const uint32_t bit = 1u << layer;
		if (enabled)
			m_CollisionMask |= bit;
		else
			m_CollisionMask &= ~bit;
	}

	bool GameObject::CollidesWithLayer(int layer) const
	{
		if (layer < 0 || layer > 31)
			return false;
		return (m_CollisionMask & (1u << layer)) != 0;
	}

	void GameObject::AddAudio()
	{
		m_HasAudio = true;
	}

	void GameObject::RemoveAudio()
	{
		m_HasAudio = false;
		m_AudioPath.clear();
	}

	void GameObject::SetAudioPath(const std::string& path)
	{
		m_AudioPath = path;
		m_HasAudio = true;
	}

	void GameObject::SetAudioVolume(float volume)
	{
		m_AudioVolume = glm::clamp(volume, 0.0f, 1.0f);
	}

	void GameObject::SetAudioLoop(bool loop)
	{
		m_AudioLoop = loop;
	}

	void GameObject::SetAudioPlayOnStart(bool playOnStart)
	{
		m_AudioPlayOnStart = playOnStart;
	}

	void GameObject::SetAudioFalloff(bool enabled)
	{
		m_AudioFalloff = enabled;
		m_HasAudio = true;
	}

	void GameObject::SetAudioMinDistance(float distance)
	{
		m_AudioMinDistance = glm::max(distance, 0.0f);
		if (m_AudioMaxDistance < m_AudioMinDistance)
			m_AudioMaxDistance = m_AudioMinDistance;
		m_HasAudio = true;
	}

	void GameObject::SetAudioMaxDistance(float distance)
	{
		m_AudioMaxDistance = glm::max(distance, 0.0f);
		if (m_AudioMaxDistance < m_AudioMinDistance)
			m_AudioMinDistance = m_AudioMaxDistance;
		m_HasAudio = true;
	}

	void GameObject::AddParticleEmitter()
	{
		m_HasParticleEmitter = true;
	}

	void GameObject::RemoveParticleEmitter()
	{
		ParticleSystem2D::StopSceneEmitter(this);
		m_HasParticleEmitter = false;
		m_ParticlePath.clear();
	}

	void GameObject::SetParticlePath(const std::string& path)
	{
		m_ParticlePath = path;
		m_HasParticleEmitter = true;
	}

	void GameObject::SetParticlePlayOnStart(bool playOnStart)
	{
		m_ParticlePlayOnStart = playOnStart;
	}

	void GameObject::PlayParticle()
	{
		BZ_INFO("Particle Play: {0}", m_ParticlePath.empty() ? "(no path)" : m_ParticlePath);
		if (!ParticleSystem2D::StartSceneEmitter(this, true))
			BZ_ERROR("Particle Play failed — check the .particle path / mode");
	}

	void GameObject::StopParticle()
	{
		BZ_INFO("Particle Stop");
		ParticleSystem2D::StopSceneEmitter(this);
	}

	bool GameObject::IsParticlePlaying() const
	{
		return ParticleSystem2D::IsSceneEmitterActive(const_cast<GameObject*>(this));
	}

	void GameObject::AddNavAgent()
	{
		m_HasNavAgent = true;
	}

	void GameObject::RemoveNavAgent()
	{
		NavAgentSystem::Stop(this);
		m_HasNavAgent = false;
	}

	void GameObject::SetNavSpeed(float pixelsPerSecond)
	{
		m_NavSpeed = glm::max(pixelsPerSecond, 0.0f);
		m_HasNavAgent = true;
	}

	void GameObject::SetNavAllowDiagonal(bool allow)
	{
		m_NavAllowDiagonal = allow;
		m_HasNavAgent = true;
	}

	void GameObject::SetNavStoppingDistance(float pixels)
	{
		m_NavStoppingDistance = glm::max(pixels, 0.0f);
		m_HasNavAgent = true;
	}

	void GameObject::SetNavDebugDraw(bool enabled)
	{
		m_NavDebugDraw = enabled;
		m_HasNavAgent = true;
	}

	bool GameObject::SetNavDestination(float worldX, float worldY)
	{
		if (!m_HasNavAgent)
			AddNavAgent();
		return NavAgentSystem::SetDestination(this, worldX, worldY);
	}

	void GameObject::StopNav()
	{
		NavAgentSystem::Stop(this);
	}

	bool GameObject::IsNavMoving() const
	{
		return NavAgentSystem::IsMoving(const_cast<GameObject*>(this));
	}

	void GameObject::AddEnemyAI()
	{
		m_HasEnemyAI = true;
	}

	void GameObject::RemoveEnemyAI()
	{
		EnemyAISystem::RemoveObject(this);
		m_HasEnemyAI = false;
	}

	void GameObject::SetEnemyAIEnabled(bool enabled)
	{
		m_EnemyAIEnabled = enabled;
		m_HasEnemyAI = true;
		if (!enabled)
			StopNav();
	}

	void GameObject::SetEnemyDetectRange(float pixels)
	{
		m_EnemyDetectRange = glm::max(pixels, 0.0f);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyLoseRange(float pixels)
	{
		m_EnemyLoseRange = glm::max(pixels, m_EnemyDetectRange);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyPatrolSpeed(float pixelsPerSecond)
	{
		m_EnemyPatrolSpeed = glm::max(pixelsPerSecond, 0.0f);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyChaseSpeed(float pixelsPerSecond)
	{
		m_EnemyChaseSpeed = glm::max(pixelsPerSecond, 0.0f);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyTargetTag(const std::string& tag)
	{
		m_EnemyTargetTag = tag;
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyWaypointWait(float seconds)
	{
		m_EnemyWaypointWait = glm::max(seconds, 0.0f);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyRepathInterval(float seconds)
	{
		m_EnemyRepathInterval = glm::max(seconds, 0.05f);
		m_HasEnemyAI = true;
	}

	void GameObject::ClearEnemyPatrolPoints()
	{
		m_EnemyPatrolPoints.clear();
		m_HasEnemyAI = true;
	}

	void GameObject::AddEnemyPatrolPoint(float x, float y)
	{
		m_EnemyPatrolPoints.emplace_back(x, y);
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyPatrolPoints(const std::vector<glm::vec2>& points)
	{
		m_EnemyPatrolPoints = points;
		m_HasEnemyAI = true;
	}

	void GameObject::SetEnemyPatrolPoint(int index, float x, float y)
	{
		if (index < 0 || index >= static_cast<int>(m_EnemyPatrolPoints.size()))
			return;
		m_EnemyPatrolPoints[static_cast<size_t>(index)] = glm::vec2(x, y);
		m_HasEnemyAI = true;
	}

	bool GameObject::RemoveEnemyPatrolPoint(int index)
	{
		if (index < 0 || index >= static_cast<int>(m_EnemyPatrolPoints.size()))
			return false;
		m_EnemyPatrolPoints.erase(m_EnemyPatrolPoints.begin() + index);
		m_HasEnemyAI = true;
		return true;
	}

	void GameObject::AddScript()
	{
		m_HasScript = true;
	}

	void GameObject::RemoveScript()
	{
		m_HasScript = false;
		m_ScriptPath.clear();
	}

	void GameObject::SetScriptPath(const std::string& path)
	{
		m_ScriptPath = path;
		m_HasScript = true;
	}

	void GameObject::RebuildMatrixFromWorld(const glm::vec3& worldPos, float worldRot, const glm::vec2& worldScale)
	{
		float sx = Camera::m_prop.projection[0][0];
		float sy = Camera::m_prop.projection[1][1];

		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
		{
			m_Matrix = glm::translate(glm::mat4(1.0f), worldPos);
			return;
		}

		glm::mat4 m = glm::translate(glm::mat4(1.0f), worldPos);
		m = glm::scale(m, glm::vec3(sx, sy, 1.0f));
		m = glm::rotate(m, glm::radians(worldRot), glm::vec3(0.0f, 0.0f, 1.0f));
		m = glm::scale(m, glm::vec3(worldScale, 1.0f));
		m = glm::scale(m, glm::vec3(1.0f / sx, 1.0f / sy, 1.0f));
		m_Matrix = m;
	}

	void GameObject::RebuildChildrenMatrices()
	{
		for (GameObject* child : m_Children)
		{
			if (child)
				child->RebuildMatrix();
		}
	}

	void GameObject::RebuildMatrix()
	{
		RebuildMatrixFromWorld(GetWorldPosition(), GetWorldRotation(), GetWorldScale());
		RebuildChildrenMatrices();
	}

	void GameObject::SetTexture(std::string path)
	{
		m_TexturePath = path;
		m_Texture.Generate(path);
	}

	void GameObject::GetWorldAABB(const CameraProp& prop, glm::vec2& outMin, glm::vec2& outMax) const
	{
		float scaleX = prop.projection[0][0];
		float scaleY = prop.projection[1][1];
		const glm::vec3 worldPos = GetWorldPosition();
		const float worldRot = GetWorldRotation();
		const glm::vec2 worldScale = GetWorldScale();

		float c = glm::cos(glm::radians(worldRot));
		float s = glm::sin(glm::radians(worldRot));
		glm::vec2 corners[4] =
		{
			{ m_LocalMin.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMax.y },
			{ m_LocalMin.x, m_LocalMax.y },
		};

		glm::vec2 mn, mx;
		for (int i = 0; i < 4; ++i)
		{
			glm::vec2 v(corners[i].x * worldScale.x, corners[i].y * worldScale.y);
			glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);
			if (i == 0) { mn = r; mx = r; }
			else
			{
				mn.x = std::min(mn.x, r.x); mn.y = std::min(mn.y, r.y);
				mx.x = std::max(mx.x, r.x); mx.y = std::max(mx.y, r.y);
			}
		}

		outMin = glm::vec2(worldPos.x + mn.x * scaleX, worldPos.y + mn.y * scaleY);
		outMax = glm::vec2(worldPos.x + mx.x * scaleX, worldPos.y + mx.y * scaleY);
	}

	bool GameObject::IsHovered(const CameraProp& prop, const glm::vec2& worldPoint) const
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
		{
			float sx = prop.projection[0][0];
			float sy = prop.projection[1][1];
			glm::vec2 pos(GetWorldPosition());
			float dx = (worldPoint.x - pos.x) / sx;
			float dy = (worldPoint.y - pos.y) / sy;
			return (dx * dx + dy * dy) <= (32.0f * 32.0f);
		}

		glm::vec2 min, max;
		GetWorldAABB(prop, min, max);

		return worldPoint.x >= min.x && worldPoint.x <= max.x &&
			   worldPoint.y >= min.y && worldPoint.y <= max.y;
	}
}
