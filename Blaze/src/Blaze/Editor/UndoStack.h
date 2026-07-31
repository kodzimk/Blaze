#pragma once
#include "Blaze/Core/Core.h"
#include "Blaze/Renderer/GameObject.h"
#include "glm.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace Blaze
{
	class Renderer;

	class BLAZE_API IUndoCommand
	{
	public:
		virtual ~IUndoCommand() = default;
		virtual void Undo() = 0;
		virtual void Redo() = 0;
		virtual const char* GetName() const = 0;
	};

	struct BLAZE_API LocalTransformSnap
	{
		glm::vec3 position = glm::vec3(0.0f);
		float rotation = 0.0f;
		glm::vec2 scale = glm::vec2(1.0f);

		static LocalTransformSnap Capture(const GameObject& object);
		void Apply(GameObject& object) const;
		bool Equals(const LocalTransformSnap& other) const;
	};

	struct BLAZE_API TileChange
	{
		int x = 0;
		int y = 0;
		int oldId = -1;
		int newId = -1;
	};

	struct BLAZE_API ObjectSnapshot
	{
		std::string name;
		ObjectType type = ObjectType::Sprite;
		LocalTransformSnap transform;
		int layer = 0;
		glm::vec4 color = glm::vec4(1.0f);
		bool visible = true;
		std::vector<std::string> tags;
		std::string texturePath;
		bool drivesView = false;
		float zoom = 1.0f;

		bool hasCollision = false;
		CollisionType collisionType = CollisionType::Block;
		ColliderShape colliderShape = ColliderShape::Block;
		float collisionRadius = 50.0f;
		glm::vec2 collisionSize = glm::vec2(100.0f);
		float rbMass = 1.0f;
		float rbGravity = 1.0f;
		bool rbKinematic = false;
		bool isTrigger = false;
		int collisionLayer = 0;
		uint32_t collisionMask = 0xFFFFFFFFu;

		bool hasAudio = false;
		std::string audioPath;
		float audioVolume = 1.0f;
		bool audioLoop = false;
		bool audioPlayOnStart = true;

		bool hasScript = false;
		std::string scriptPath;

		int flipCols = 1;
		int flipRows = 1;
		int flipFrameCount = 0;
		float flipFps = 12.0f;
		bool flipPlaying = false;
		bool flipLoop = true;
		int flipFrame = 0;
		std::vector<FlipbookClip> flipClips;
		int flipActiveClip = -1;
		std::string flipbookAssetPath;

		int tilemapCols = 0;
		int tilemapRows = 0;
		float tileCellSize = 32.0f;
		int tilesetCols = 1;
		int tilesetRows = 1;
		std::vector<int> tiles;
		bool tileCollisionEnabled = true;
		std::vector<uint8_t> tilesetSolid;
		std::vector<TilemapLayer> tileLayers;
		int activeTileLayer = 0;

		std::string text = "Text";
		float fontSize = 48.0f;
		std::string fontPath;

		std::string parentName;

		static ObjectSnapshot Capture(const GameObject& object);
		GameObject* Restore(Renderer& renderer,
			const std::vector<float>& vertices,
			const std::vector<unsigned int>& indices) const;
	};

	class BLAZE_API UndoStack
	{
	public:
		static UndoStack& Get();

		UndoStack(const UndoStack&) = delete;
		UndoStack& operator=(const UndoStack&) = delete;
		UndoStack(UndoStack&&) = delete;
		UndoStack& operator=(UndoStack&&) = delete;

		void SetRenderer(Renderer* renderer) { m_Renderer = renderer; }
		Renderer* GetRenderer() const { return m_Renderer; }
		void SetMeshDefaults(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);

		void Push(std::unique_ptr<IUndoCommand> command);
		bool CanUndo() const { return !m_Undo.empty(); }
		bool CanRedo() const { return !m_Redo.empty(); }
		void Undo();
		void Redo();
		void Clear();

		size_t GetLimit() const { return m_Limit; }
		void SetLimit(size_t limit) { m_Limit = limit; }

		const std::vector<float>& GetDefaultVertices() const { return m_Vertices; }
		const std::vector<unsigned int>& GetDefaultIndices() const { return m_Indices; }

	private:
		UndoStack() = default;
		Renderer* m_Renderer = nullptr;
		std::vector<std::unique_ptr<IUndoCommand>> m_Undo;
		std::vector<std::unique_ptr<IUndoCommand>> m_Redo;
		std::vector<float> m_Vertices;
		std::vector<unsigned int> m_Indices;
		size_t m_Limit = 100;
	};

	class BLAZE_API TransformCommand : public IUndoCommand
	{
	public:
		TransformCommand(GameObject* object, const LocalTransformSnap& before, const LocalTransformSnap& after);
		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Transform"; }
	private:
		GameObject* m_Object = nullptr;
		LocalTransformSnap m_Before;
		LocalTransformSnap m_After;
	};

	class BLAZE_API TileStrokeCommand : public IUndoCommand
	{
	public:
		TileStrokeCommand(GameObject* object, std::vector<TileChange> changes);
		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Paint Tiles"; }
	private:
		GameObject* m_Object = nullptr;
		std::vector<TileChange> m_Changes;
	};

	class BLAZE_API ParentCommand : public IUndoCommand
	{
	public:
		ParentCommand(GameObject* child, GameObject* oldParent, GameObject* newParent, bool keepWorld);
		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Parent"; }
	private:
		GameObject* m_Child = nullptr;
		GameObject* m_OldParent = nullptr;
		GameObject* m_NewParent = nullptr;
		bool m_KeepWorld = true;
	};

	class BLAZE_API CreateObjectCommand : public IUndoCommand
	{
	public:
		explicit CreateObjectCommand(ObjectSnapshot snapshot);
		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Create Object"; }
		void BindLiveObject(GameObject* object) { m_Live = object; }
	private:
		ObjectSnapshot m_Snapshot;
		GameObject* m_Live = nullptr;
	};

	class BLAZE_API DeleteObjectCommand : public IUndoCommand
	{
	public:
		explicit DeleteObjectCommand(ObjectSnapshot snapshot);
		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Delete Object"; }
	private:
		ObjectSnapshot m_Snapshot;
		GameObject* m_Live = nullptr;
	};

	BLAZE_API bool IsObjectLive(GameObject* object);
}
