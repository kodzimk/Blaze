#pragma once
#include"bzpch.h"
#include"Blaze/Renderer/VertexArray.h"
#include"Blaze/Renderer/Texture.h"
#include"Blaze/Renderer/FlipbookAsset.h"
#include"glm.hpp"

namespace Blaze
{
	struct CameraProp;
	class Shader;

	enum class ObjectType
	{
		Sprite = 0,
		Empty,
		Camera,
		Circle,
		Box,
		Sound,
		Tilemap,
		Text
	};

	enum class CollisionType
	{
		Circle = 0,
		Block,
		PhysicsRigidbody2D
	};

	struct TilemapLayer
	{
		std::string name = "Layer";
		bool visible = true;
		bool collision = true;
		float opacity = 1.0f;
		std::vector<int> tiles; // -1 = empty
	};

	enum class ColliderShape
	{
		Circle = 0,
		Block
	};

	//  layer indices (0-7). Mask bits use (1u << layer).
	enum CollisionLayer : int
	{
		CollisionLayer_Default = 0,
		CollisionLayer_Player = 1,
		CollisionLayer_Enemy = 2,
		CollisionLayer_Projectile = 3,
		CollisionLayer_Trigger = 4,
		CollisionLayer_Ground = 5,
		CollisionLayer_Pickup = 6,
		CollisionLayer_Custom7 = 7,
		CollisionLayer_Count = 8
	};

	class BLAZE_API GameObject
	{
	public:
		GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color,std::string name);
		virtual ~GameObject();

		void Draw(const CameraProp& prop, Shader& const m_Shader);
		void UpdateAnimation(float dt);

		void SetPosition(glm::vec3& pos);
		void SetLocalPosition(const glm::vec3& pos);
		void Translate(glm::vec3& translate);
		void SetRotation(float degrees);
		void SetLocalRotation(float degrees);
		void Rotate(float degrees);
		void SetScale(glm::vec2 scale);
		void SetLocalScale(glm::vec2 scale);
		void SetColor(const glm::vec4& color);
		void SetColor(float r, float g, float b, float a = 1.0f);
		void SetTexture(std::string path);
		void SetName(const std::string& name) { m_Name = name; }
		void SetVisible(bool visible) { m_Visible = visible; }
		inline bool IsVisible() const { return m_Visible; }

		bool SetParent(GameObject* parent, bool keepWorld = true);
		void ClearParent(bool keepWorld = true);
		GameObject* GetParent() const { return m_Parent; }
		const std::vector<GameObject*>& GetChildren() const { return m_Children; }
		void DetachFromHierarchy();
		void SetTag(const std::string& tag);
		void AddTag(const std::string& tag);
		void RemoveTag(const std::string& tag);
		void RemoveTagAt(size_t index);
		void SetTagAt(size_t index, const std::string& tag);
		void ClearTags();
		void SetTagsFromString(const std::string& tagsCsv);
		bool HasTag(const std::string& tag) const;
		std::string GetTag() const;
		std::string GetTagsString() const;
		const std::vector<std::string>& GetTags() const { return m_Tags; }
		void SetType(ObjectType type);
		void SetDrivesView(bool drives) { m_DrivesView = drives; }
		void SetZoom(float zoom);
		void SetLayer(int layer);

		void AddCollision(CollisionType type = CollisionType::Block);
		void RemoveCollision();
		void SetCollisionType(CollisionType type);
		void SetColliderShape(ColliderShape shape);
		void SetCollisionRadius(float radius);
		void SetCollisionSize(glm::vec2 size);
		void SetRigidbodyMass(float mass);
		void SetRigidbodyGravityScale(float scale);
		void SetRigidbodyKinematic(bool kinematic);
		void SetIsTrigger(bool trigger);
		void SetCollisionLayer(int layer);
		void SetCollisionMask(uint32_t mask);
		void SetCollidesWithLayer(int layer, bool enabled);

		void AddAudio();
		void RemoveAudio();
		void SetAudioPath(const std::string& path);
		void SetAudioVolume(float volume);
		void SetAudioLoop(bool loop);
		void SetAudioPlayOnStart(bool playOnStart);

		void AddScript();
		void RemoveScript();
		void SetScriptPath(const std::string& path);

		void SetFlipbookColumns(int columns);
		void SetFlipbookRows(int rows);
		void SetFlipbookFrameCount(int count);
		void SetFlipbookFps(float fps);
		void SetFlipbookPlaying(bool playing);
		void SetFlipbookLoop(bool loop);
		void SetFlipbookFrame(int frame);
		void PlayFlipbook();
		bool PlayFlipbook(const std::string& clipName);
		void StopFlipbook();
		void PauseFlipbook();

		int AddFlipbookClip(const std::string& name, int startFrame, int endFrame, float fps = -1.0f, bool loop = true);
		bool SetFlipbookClip(const std::string& name, int startFrame, int endFrame, float fps = -1.0f, bool loop = true);
		bool RemoveFlipbookClip(const std::string& name);
		bool RemoveFlipbookClipAt(int index);
		void ClearFlipbookClips();
		void SetFlipbookClips(const std::vector<FlipbookClip>& clips);
		bool SetActiveFlipbookClip(const std::string& name);
		void ClearActiveFlipbookClip();
		int FindFlipbookClip(const std::string& name) const;

		void SetFlipbookAssetPath(const std::string& path, bool loadAsset = true);
		bool ApplyFlipbookAsset(const FlipbookAsset& asset);
		bool LoadFlipbookAsset();
		bool SaveFlipbookAsset() const;
		bool SaveFlipbookAssetAs(const std::string& path);

		void InitTilemapDefaults();
		void SetTilemapSize(int columns, int rows);
		void SetTileCellSize(float size);
		void SetTilesetGrid(int columns, int rows);
		void SetTile(int x, int y, int tileId);
		int GetTile(int x, int y) const;
		void SetTileOnLayer(int layerIndex, int x, int y, int tileId);
		int GetTileOnLayer(int layerIndex, int x, int y) const;
		void FillTiles(int tileId);
		void ClearTiles();
		void SetTilesFromList(const std::vector<int>& tiles);
		const std::vector<int>& GetTiles() const;
		void RefreshTilemapBounds();

		int GetTileLayerCount() const { return (int)m_TileLayers.size(); }
		int GetActiveTileLayer() const { return m_ActiveTileLayer; }
		void SetActiveTileLayer(int index);
		int AddTileLayer(const std::string& name = "Layer");
		bool RemoveTileLayer(int index);
		void ClearTileLayers();
		void SetTileLayers(const std::vector<TilemapLayer>& layers, int activeIndex = 0);
		const std::vector<TilemapLayer>& GetTileLayers() const { return m_TileLayers; }
		const TilemapLayer* GetTileLayer(int index) const;
		TilemapLayer* GetTileLayerMutable(int index);
		void SetTileLayerName(int index, const std::string& name);
		void SetTileLayerVisible(int index, bool visible);
		void SetTileLayerCollision(int index, bool collision);
		void SetTileLayerOpacity(int index, float opacity);
		void SetTileLayerTiles(int index, const std::vector<int>& tiles);
		const std::vector<int>& GetTileLayerTiles(int index) const;

		void SetTileCollisionEnabled(bool enabled) { m_TileCollisionEnabled = enabled; }
		bool IsTileCollisionEnabled() const { return m_TileCollisionEnabled; }
		void SetTilesetSolid(int tileId, bool solid);
		bool IsTilesetSolid(int tileId) const;
		void SetAllTilesetSolid(bool solid);
		void SetTilesetSolidFromList(const std::vector<uint8_t>& solids);
		const std::vector<uint8_t>& GetTilesetSolid() const { return m_TilesetSolid; }
		bool IsSolidAt(int x, int y) const;
		bool IsTileIdSolid(int tileId) const;
		glm::vec2 TileCenterLocal(int tileX, int tileY) const;

		inline ObjectType GetType() const { return m_Type; }
		inline bool IsCamera() const { return m_Type == ObjectType::Camera; }
		inline bool IsSound() const { return m_Type == ObjectType::Sound; }
		inline bool IsTilemap() const { return m_Type == ObjectType::Tilemap; }
		inline bool IsText() const { return m_Type == ObjectType::Text; }

		void SetText(const std::string& text);
		void SetFontSize(float size);
		void SetFontPath(const std::string& path);
		inline const std::string& GetText() const { return m_Text; }
		inline float GetFontSize() const { return m_FontSize; }
		inline const std::string& GetFontPath() const { return m_FontPath; }
		inline bool DrivesView() const { return m_DrivesView; }
		inline float GetZoom() const { return m_Zoom; }
		inline std::string GetName() const { return m_Name; }
		glm::mat4 GetCameraViewMatrix() const;
		inline const std::string& GetTexturePath() const { return m_TexturePath; }
		inline unsigned int GetTextureID() const { return m_Texture.GetTextID(); }
		inline bool HasTexture() const { return m_Texture.IsValid(); }

		glm::vec3 GetPosition() const { return GetWorldPosition(); }
		float GetRotation() const { return GetWorldRotation(); }
		glm::vec2 GetScale() const { return GetWorldScale(); }
		glm::vec3 GetWorldPosition() const;
		float GetWorldRotation() const;
		glm::vec2 GetWorldScale() const;
		glm::vec3 GetLocalPosition() const { return m_Pos; }
		float GetLocalRotation() const { return m_Rotation; }
		glm::vec2 GetLocalScale() const { return m_Scale; }

		inline glm::vec4 GetColor() const { return m_Color; }
		inline int GetLayer() const { return m_Layer; }

		inline bool HasCollision() const { return m_HasCollision; }
		inline CollisionType GetCollisionType() const { return m_CollisionType; }
		inline ColliderShape GetColliderShape() const { return m_ColliderShape; }
		inline float GetCollisionRadius() const { return m_CollisionRadius; }
		inline glm::vec2 GetCollisionSize() const { return m_CollisionSize; }
		inline float GetRigidbodyMass() const { return m_RigidbodyMass; }
		inline float GetRigidbodyGravityScale() const { return m_RigidbodyGravityScale; }
		inline bool IsRigidbodyKinematic() const { return m_RigidbodyKinematic; }
		inline bool IsTrigger() const { return m_IsTrigger; }
		inline int GetCollisionLayer() const { return m_CollisionLayer; }
		inline uint32_t GetCollisionMask() const { return m_CollisionMask; }
		bool CollidesWithLayer(int layer) const;

		inline bool HasAudio() const { return m_HasAudio; }
		inline const std::string& GetAudioPath() const { return m_AudioPath; }
		inline float GetAudioVolume() const { return m_AudioVolume; }
		inline bool GetAudioLoop() const { return m_AudioLoop; }
		inline bool GetAudioPlayOnStart() const { return m_AudioPlayOnStart; }

		inline bool HasScript() const { return m_HasScript; }
		inline const std::string& GetScriptPath() const { return m_ScriptPath; }

		inline int GetFlipbookColumns() const { return m_FlipColumns; }
		inline int GetFlipbookRows() const { return m_FlipRows; }
		inline int GetFlipbookFrameCount() const { return m_FlipFrameCount; }
		inline float GetFlipbookFps() const { return m_FlipFps; }
		inline bool IsFlipbookPlaying() const { return m_FlipPlaying; }
		inline bool GetFlipbookLoop() const { return m_FlipLoop; }
		inline int GetFlipbookFrame() const { return m_FlipFrame; }
		int GetFlipbookTotalFrames() const;
		const std::vector<FlipbookClip>& GetFlipbookClips() const { return m_FlipClips; }
		int GetFlipbookClipCount() const { return (int)m_FlipClips.size(); }
		const FlipbookClip* GetFlipbookClip(int index) const;
		const FlipbookClip* GetFlipbookClip(const std::string& name) const;
		int GetActiveFlipbookClipIndex() const { return m_FlipActiveClip; }
		std::string GetActiveFlipbookClipName() const;
		void GetFlipbookPlaybackRange(int& outStart, int& outEnd) const;
		float GetFlipbookPlaybackFps() const;
		bool GetFlipbookPlaybackLoop() const;
		const std::string& GetFlipbookAssetPath() const { return m_FlipbookAssetPath; }
		bool HasFlipbookAsset() const { return !m_FlipbookAssetPath.empty(); }

		inline int GetTilemapColumns() const { return m_TilemapColumns; }
		inline int GetTilemapRows() const { return m_TilemapRows; }
		inline float GetTileCellSize() const { return m_TileCellSize; }
		inline int GetTilesetColumns() const { return m_TilesetColumns; }
		inline int GetTilesetRows() const { return m_TilesetRows; }
		int GetTilesetTileCount() const;

		bool WorldToTile(const CameraProp& prop, const glm::vec2& worldPoint, int& outX, int& outY) const;
		glm::vec2 TileCenterWorld(const CameraProp& prop, int tileX, int tileY) const;

		void GetWorldAABB(const CameraProp& prop, glm::vec2& outMin, glm::vec2& outMax) const;
		bool IsHovered(const CameraProp& prop, const glm::vec2& worldPoint) const;
	private:
		void RebuildMatrix();
		void RebuildMatrixFromWorld(const glm::vec3& worldPos, float worldRot, const glm::vec2& worldScale);
		void RebuildChildrenMatrices();
		bool WouldCreateParentCycle(GameObject* parent) const;
		void RemoveFromParentChildren();
		void ClampFlipbookFrame();
		void NormalizeFlipbookClip(FlipbookClip& clip) const;
		std::string MakeUniqueFlipbookClipName(const std::string& desired) const;
		glm::vec2 GetFlipbookUVScale() const;
		glm::vec2 GetFlipbookUVOffset() const;
		glm::vec2 GetTilesetUVScale() const;
		glm::vec2 GetTilesetUVOffset(int tileId) const;
		void DrawTilemap(const CameraProp& prop, Shader& shader);
		void RefreshTextBounds();
		void EnsureTileLayers();
		std::vector<int>& ActiveTiles();
		const std::vector<int>& ActiveTiles() const;
		bool IsPaintedTileSolid(int tileId) const;

		VertexArray m_VertexArray;
		VertexBuffer m_VertexBuffer;
		unsigned int m_EBO;
		Texture m_Texture;
		std::string m_TexturePath;
		glm::vec4 m_Color;
		glm::mat4 m_Matrix = glm::mat4(1.0f);
		glm::vec3 m_Pos = glm::vec3(0.0f); // local
		float m_Rotation = 0.0f; // local degrees
		glm::vec2 m_Scale = glm::vec2(1.0f); // local
		GameObject* m_Parent = nullptr;
		std::vector<GameObject*> m_Children;
		int m_Layer = 0;
		glm::vec2 m_LocalMin = glm::vec2(0.0f);
		glm::vec2 m_LocalMax = glm::vec2(0.0f);
		std::string m_Name;
		std::vector<std::string> m_Tags;
		ObjectType m_Type = ObjectType::Sprite;
		bool m_Visible = true;
		bool m_DrivesView = false;
		float m_Zoom = 1.0f;

		bool m_HasCollision = false;
		CollisionType m_CollisionType = CollisionType::Block;
		ColliderShape m_ColliderShape = ColliderShape::Block;
		float m_CollisionRadius = 50.0f;
		glm::vec2 m_CollisionSize = glm::vec2(100.0f, 100.0f);
		float m_RigidbodyMass = 1.0f;
		float m_RigidbodyGravityScale = 1.0f;
		bool m_RigidbodyKinematic = false;
		bool m_IsTrigger = false;
		int m_CollisionLayer = 0;
		uint32_t m_CollisionMask = 0xFFFFFFFFu;

		bool m_HasAudio = false;
		std::string m_AudioPath;
		float m_AudioVolume = 1.0f;
		bool m_AudioLoop = false;
		bool m_AudioPlayOnStart = true;

		bool m_HasScript = false;
		std::string m_ScriptPath;

		int m_FlipColumns = 1;
		int m_FlipRows = 1;
		int m_FlipFrameCount = 0; // 0 = columns * rows
		float m_FlipFps = 12.0f;
		bool m_FlipPlaying = false;
		bool m_FlipLoop = true;
		int m_FlipFrame = 0;
		float m_FlipTime = 0.0f;
		std::vector<FlipbookClip> m_FlipClips;
		int m_FlipActiveClip = -1; // -1 = full sheet / global settings
		std::string m_FlipbookAssetPath;

		int m_TilemapColumns = 0;
		int m_TilemapRows = 0;
		float m_TileCellSize = 32.0f;
		int m_TilesetColumns = 1;
		int m_TilesetRows = 1;
		std::vector<TilemapLayer> m_TileLayers;
		int m_ActiveTileLayer = 0;
		bool m_TileCollisionEnabled = true;
		std::vector<uint8_t> m_TilesetSolid; // per tileset tile id; empty/all-zero => all painted tiles solid

		std::string m_Text = "Text";
		float m_FontSize = 48.0f;
		std::string m_FontPath;
	};
}
