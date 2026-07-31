#include "UndoStack.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Blaze
{
	UndoStack& UndoStack::Get()
	{
		static UndoStack instance;
		return instance;
	}

	void UndoStack::SetMeshDefaults(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
	{
		m_Vertices = vertices;
		m_Indices = indices;
	}

	void UndoStack::Push(std::unique_ptr<IUndoCommand> command)
	{
		if (!command)
			return;
		m_Undo.push_back(std::move(command));
		m_Redo.clear();
		while (m_Undo.size() > m_Limit)
		{
			m_Undo.front().reset();
			m_Undo.erase(m_Undo.begin());
		}
	}

	void UndoStack::Undo()
	{
		if (m_Undo.empty())
			return;
		std::unique_ptr<IUndoCommand> cmd = std::move(m_Undo.back());
		m_Undo.pop_back();
		cmd->Undo();
		m_Redo.push_back(std::move(cmd));
		BZ_INFO("Undo: {0}", m_Redo.back()->GetName());
	}

	void UndoStack::Redo()
	{
		if (m_Redo.empty())
			return;
		std::unique_ptr<IUndoCommand> cmd = std::move(m_Redo.back());
		m_Redo.pop_back();
		cmd->Redo();
		m_Undo.push_back(std::move(cmd));
		BZ_INFO("Redo: {0}", m_Undo.back()->GetName());
	}

	void UndoStack::Clear()
	{
		m_Undo.clear();
		m_Redo.clear();
	}

	bool IsObjectLive(GameObject* object)
	{
		if (!object)
			return false;
		Renderer* renderer = UndoStack::Get().GetRenderer();
		if (!renderer)
			return false;
		const auto& objects = renderer->GetObjects();
		return std::find(objects.begin(), objects.end(), object) != objects.end();
	}

	LocalTransformSnap LocalTransformSnap::Capture(const GameObject& object)
	{
		LocalTransformSnap snap;
		snap.position = object.GetLocalPosition();
		snap.rotation = object.GetLocalRotation();
		snap.scale = object.GetLocalScale();
		return snap;
	}

	void LocalTransformSnap::Apply(GameObject& object) const
	{
		object.SetLocalPosition(position);
		object.SetLocalRotation(rotation);
		object.SetLocalScale(scale);
	}

	bool LocalTransformSnap::Equals(const LocalTransformSnap& other) const
	{
		const float e = 1e-4f;
		return std::abs(position.x - other.position.x) < e
			&& std::abs(position.y - other.position.y) < e
			&& std::abs(position.z - other.position.z) < e
			&& std::abs(rotation - other.rotation) < e
			&& std::abs(scale.x - other.scale.x) < e
			&& std::abs(scale.y - other.scale.y) < e;
	}

	ObjectSnapshot ObjectSnapshot::Capture(const GameObject& object)
	{
		ObjectSnapshot snap;
		snap.name = object.GetName();
		snap.type = object.GetType();
		snap.transform = LocalTransformSnap::Capture(object);
		snap.layer = object.GetLayer();
		snap.color = object.GetColor();
		snap.visible = object.IsVisible();
		snap.tags = object.GetTags();
		snap.texturePath = object.GetTexturePath();
		snap.drivesView = object.DrivesView();
		snap.zoom = object.GetZoom();

		snap.hasCollision = object.HasCollision();
		snap.collisionType = object.GetCollisionType();
		snap.colliderShape = object.GetColliderShape();
		snap.collisionRadius = object.GetCollisionRadius();
		snap.collisionSize = object.GetCollisionSize();
		snap.rbMass = object.GetRigidbodyMass();
		snap.rbGravity = object.GetRigidbodyGravityScale();
		snap.rbKinematic = object.IsRigidbodyKinematic();
		snap.isTrigger = object.IsTrigger();
		snap.collisionLayer = object.GetCollisionLayer();
		snap.collisionMask = object.GetCollisionMask();

		snap.hasAudio = object.HasAudio();
		snap.audioPath = object.GetAudioPath();
		snap.audioVolume = object.GetAudioVolume();
		snap.audioLoop = object.GetAudioLoop();
		snap.audioPlayOnStart = object.GetAudioPlayOnStart();

		snap.hasScript = object.HasScript();
		snap.scriptPath = object.GetScriptPath();

		snap.flipCols = object.GetFlipbookColumns();
		snap.flipRows = object.GetFlipbookRows();
		snap.flipFrameCount = object.GetFlipbookFrameCount();
		snap.flipFps = object.GetFlipbookFps();
		snap.flipPlaying = object.IsFlipbookPlaying();
		snap.flipLoop = object.GetFlipbookLoop();
		snap.flipFrame = object.GetFlipbookFrame();
		snap.flipClips = object.GetFlipbookClips();
		snap.flipActiveClip = object.GetActiveFlipbookClipIndex();
		snap.flipbookAssetPath = object.GetFlipbookAssetPath();

		snap.tilemapCols = object.GetTilemapColumns();
		snap.tilemapRows = object.GetTilemapRows();
		snap.tileCellSize = object.GetTileCellSize();
		snap.tilesetCols = object.GetTilesetColumns();
		snap.tilesetRows = object.GetTilesetRows();
		snap.tiles = object.GetTiles();
		snap.tileCollisionEnabled = object.IsTileCollisionEnabled();
		snap.tilesetSolid = object.GetTilesetSolid();
		snap.tileLayers = object.GetTileLayers();
		snap.activeTileLayer = object.GetActiveTileLayer();

		snap.text = object.GetText();
		snap.fontSize = object.GetFontSize();
		snap.fontPath = object.GetFontPath();

		if (GameObject* parent = object.GetParent())
			snap.parentName = parent->GetName();
		return snap;
	}

	GameObject* ObjectSnapshot::Restore(Renderer& renderer,
		const std::vector<float>& vertices,
		const std::vector<unsigned int>& indices) const
	{
		renderer.CreateObject(vertices, indices, color);
		if (renderer.GetObjects().empty())
			return nullptr;

		GameObject* object = renderer.GetObjects().back();
		object->SetName(name);
		object->SetType(type);
		transform.Apply(*object);
		object->SetLayer(layer);
		object->SetColor(color);
		object->SetVisible(visible);
		object->ClearTags();
		for (const std::string& tag : tags)
			object->AddTag(tag);

		if (!texturePath.empty())
			object->SetTexture(texturePath);

		if (object->IsCamera())
		{
			object->SetZoom(zoom);
			object->SetDrivesView(drivesView);
		}

		if (hasCollision)
		{
			object->AddCollision(collisionType);
			object->SetColliderShape(colliderShape);
			object->SetCollisionRadius(collisionRadius);
			object->SetCollisionSize(collisionSize);
			object->SetRigidbodyMass(rbMass);
			object->SetRigidbodyGravityScale(rbGravity);
			object->SetRigidbodyKinematic(rbKinematic);
			object->SetIsTrigger(isTrigger);
			object->SetCollisionLayer(collisionLayer);
			object->SetCollisionMask(collisionMask);
		}

		if (hasAudio)
		{
			object->AddAudio();
			if (!audioPath.empty())
				object->SetAudioPath(audioPath);
			object->SetAudioVolume(audioVolume);
			object->SetAudioLoop(audioLoop);
			object->SetAudioPlayOnStart(audioPlayOnStart);
		}

		if (hasScript)
		{
			object->AddScript();
			if (!scriptPath.empty())
				object->SetScriptPath(scriptPath);
		}

		object->SetFlipbookColumns(flipCols);
		object->SetFlipbookRows(flipRows);
		object->SetFlipbookFrameCount(flipFrameCount);
		object->SetFlipbookFps(flipFps);
		object->SetFlipbookLoop(flipLoop);
		object->SetFlipbookClips(flipClips);
		object->SetFlipbookAssetPath(flipbookAssetPath, false);
		if (flipActiveClip >= 0 && flipActiveClip < (int)flipClips.size())
			object->SetActiveFlipbookClip(flipClips[flipActiveClip].name);
		else
			object->ClearActiveFlipbookClip();
		object->SetFlipbookFrame(flipFrame);
		object->SetFlipbookPlaying(flipPlaying);

		if (object->IsTilemap())
		{
			object->SetTilemapSize(tilemapCols, tilemapRows);
			object->SetTileCellSize(tileCellSize);
			object->SetTilesetGrid(tilesetCols, tilesetRows);
			if (!tileLayers.empty())
				object->SetTileLayers(tileLayers, activeTileLayer);
			else
				object->SetTilesFromList(tiles);
			object->SetTileCollisionEnabled(tileCollisionEnabled);
			object->SetTilesetSolidFromList(tilesetSolid);
		}

		if (object->IsText())
		{
			object->SetText(text);
			object->SetFontSize(fontSize);
			object->SetFontPath(fontPath);
		}

		if (!parentName.empty())
		{
			for (GameObject* candidate : renderer.GetObjects())
			{
				if (candidate && candidate != object && candidate->GetName() == parentName)
				{
					object->SetParent(candidate, false);
					break;
				}
			}
		}

		return object;
	}

	TransformCommand::TransformCommand(GameObject* object, const LocalTransformSnap& before, const LocalTransformSnap& after)
		: m_Object(object), m_Before(before), m_After(after)
	{
	}

	void TransformCommand::Undo()
	{
		if (IsObjectLive(m_Object))
			m_Before.Apply(*m_Object);
	}

	void TransformCommand::Redo()
	{
		if (IsObjectLive(m_Object))
			m_After.Apply(*m_Object);
	}

	TileStrokeCommand::TileStrokeCommand(GameObject* object, std::vector<TileChange> changes)
		: m_Object(object), m_Changes(std::move(changes))
	{
	}

	void TileStrokeCommand::Undo()
	{
		if (!IsObjectLive(m_Object) || !m_Object->IsTilemap())
			return;
		for (const TileChange& change : m_Changes)
			m_Object->SetTile(change.x, change.y, change.oldId);
	}

	void TileStrokeCommand::Redo()
	{
		if (!IsObjectLive(m_Object) || !m_Object->IsTilemap())
			return;
		for (const TileChange& change : m_Changes)
			m_Object->SetTile(change.x, change.y, change.newId);
	}

	ParentCommand::ParentCommand(GameObject* child, GameObject* oldParent, GameObject* newParent, bool keepWorld)
		: m_Child(child), m_OldParent(oldParent), m_NewParent(newParent), m_KeepWorld(keepWorld)
	{
	}

	void ParentCommand::Undo()
	{
		if (!IsObjectLive(m_Child))
			return;
		GameObject* parent = IsObjectLive(m_OldParent) ? m_OldParent : nullptr;
		m_Child->SetParent(parent, m_KeepWorld);
	}

	void ParentCommand::Redo()
	{
		if (!IsObjectLive(m_Child))
			return;
		GameObject* parent = IsObjectLive(m_NewParent) ? m_NewParent : nullptr;
		m_Child->SetParent(parent, m_KeepWorld);
	}

	CreateObjectCommand::CreateObjectCommand(ObjectSnapshot snapshot)
		: m_Snapshot(std::move(snapshot))
	{
	}

	void CreateObjectCommand::Undo()
	{
		Renderer* renderer = UndoStack::Get().GetRenderer();
		if (!renderer || !IsObjectLive(m_Live))
			return;
		m_Snapshot = ObjectSnapshot::Capture(*m_Live);
		renderer->DestroyObject(m_Live);
		m_Live = nullptr;
	}

	void CreateObjectCommand::Redo()
	{
		Renderer* renderer = UndoStack::Get().GetRenderer();
		if (!renderer)
			return;
		m_Live = m_Snapshot.Restore(*renderer,
			UndoStack::Get().GetDefaultVertices(),
			UndoStack::Get().GetDefaultIndices());
	}

	DeleteObjectCommand::DeleteObjectCommand(ObjectSnapshot snapshot)
		: m_Snapshot(std::move(snapshot))
	{
	}

	void DeleteObjectCommand::Undo()
	{
		Renderer* renderer = UndoStack::Get().GetRenderer();
		if (!renderer)
			return;
		m_Live = m_Snapshot.Restore(*renderer,
			UndoStack::Get().GetDefaultVertices(),
			UndoStack::Get().GetDefaultIndices());
	}

	void DeleteObjectCommand::Redo()
	{
		Renderer* renderer = UndoStack::Get().GetRenderer();
		if (!renderer)
			return;

		if (!IsObjectLive(m_Live))
		{
			// Find by name as fallback after other undos
			m_Live = nullptr;
			for (GameObject* object : renderer->GetObjects())
			{
				if (object && object->GetName() == m_Snapshot.name)
				{
					m_Live = object;
					break;
				}
			}
		}
		if (!m_Live)
			return;
		m_Snapshot = ObjectSnapshot::Capture(*m_Live);
		renderer->DestroyObject(m_Live);
		m_Live = nullptr;
	}
}
