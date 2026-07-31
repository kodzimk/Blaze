#include <Blaze.h>
#include<glad/glad.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cctype>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer(vertexShaderSource,fragmentShaderSource);
		camera = Blaze::Camera::CreateCamera(glm::vec3(0.0f,0.0f,0.0f));
		gizmo = new Blaze::Gizmo2D();
		physics = new Blaze::PhysicsWorld2D();
		Blaze::UndoStack::Get().SetRenderer(render);
		Blaze::UndoStack::Get().SetMeshDefaults(vertices, indices);
		m_LastTime = std::chrono::steady_clock::now();
	}


	void RenderGameView()
	{
		Blaze::GameObject* cameraObject = FindCameraObject();
		if (!cameraObject)
			return;

		Blaze::GameWindow::BeginCapture();

		render->BeginScene(cameraObject->GetCameraViewMatrix(), "matrix");
		render->Render(*camera);
		render->EndScene();

		{
			const glm::mat4 savedView = Blaze::Camera::m_prop.matrix;
			Blaze::Camera::m_prop.matrix = cameraObject->GetCameraViewMatrix();
			Blaze::DebugDraw2D::Flush(*camera);
			Blaze::ParticleSystem2D::Flush(*camera);
			Blaze::Camera::m_prop.matrix = savedView;
		}

		if (!Blaze::UISystem::IsReady())
			Blaze::UISystem::Init();
		Blaze::UISystem::Draw(
			Blaze::GameWindow::GetFrameWidth(), Blaze::GameWindow::GetFrameHeight());

		auto& window = Blaze::Application::Get().GetWindow();
		Blaze::GameWindow::EndCapture((int)window.GetWidth(), (int)window.GetHeight());
	}

	~ExampleLayer()
	{
		Blaze::AudioEngine::Shutdown();
		Blaze::ScriptEngine::Shutdown();
		delete physics;
		delete gizmo;
		Blaze::UISystem::Shutdown();
		delete render;
		delete camera;
	}

	glm::vec2 GetMouseWorldPos() const
	{
		auto& window = Blaze::Application::Get().GetWindow();
		return Blaze::Camera::ScreenToWorld(
			Blaze::Input::GetMouseX(), Blaze::Input::GetMouseY(),
			(float)window.GetWidth(), (float)window.GetHeight());
	}


	float GetMouseAngle(const glm::vec2& world) const
	{
		if (!m_Selected)
			return 0.0f;
		auto prop = camera->GetCameraProp();
		glm::vec2 c(m_Selected->GetPosition());
		return glm::degrees(glm::atan(
			(world.y - c.y) / prop.projection[1][1],
			(world.x - c.x) / prop.projection[0][0]));
	}

	Blaze::GameObject* FindCameraObject() const
	{
		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (object && object->IsCamera() && object->DrivesView())
				return object;
		}
		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (object && object->IsCamera())
				return object;
		}
		return nullptr;
	}

	int CountCameraObjects() const
	{
		int count = 0;
		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (object && object->IsCamera())
				++count;
		}
		return count;
	}

	void FocusEditorOnObject(Blaze::GameObject* object)
	{
		if (!object || !camera)
			return;

		camera->FocusOn(object->GetPosition());
	}

	Blaze::ObjectWindow* GetObjectsWindow()
	{
		if (auto* context = Blaze::Application::Get().GetContext())
			return context->m_objectWindow;
		return nullptr;
	}

	void ClearScene()
	{
		m_Selected = nullptr;
		m_DragAxis = Blaze::GizmoAxis::None;
		EndGizmoDrag(false);
		EndTileStroke(false);
		Blaze::UndoStack::Get().Clear();
		if (Blaze::ObjectWindow* objects = GetObjectsWindow())
			objects->ClearHierarchy();
		while (!render->GetObjects().empty())
			render->DestroyObject(render->GetObjects().back());
	}

	void PushCreatedObject(Blaze::GameObject& object)
	{
		auto cmd = std::make_unique<Blaze::CreateObjectCommand>(Blaze::ObjectSnapshot::Capture(object));
		cmd->BindLiveObject(&object);
		Blaze::UndoStack::Get().Push(std::move(cmd));
	}

	void DeleteObjectWithUndo(Blaze::GameObject* object)
	{
		if (!object)
			return;
		Blaze::UndoStack::Get().Push(
			std::make_unique<Blaze::DeleteObjectCommand>(Blaze::ObjectSnapshot::Capture(*object)));
		if (m_Selected == object)
			m_Selected = nullptr;
		m_DragAxis = Blaze::GizmoAxis::None;
		EndGizmoDrag(false);
		EndTileStroke(false);
		render->DestroyObject(object);
	}

	void BeginGizmoDrag()
	{
		if (!m_Selected)
			return;
		m_GizmoUndoBefore = Blaze::LocalTransformSnap::Capture(*m_Selected);
		m_GizmoDragTracked = true;
	}

	void EndGizmoDrag(bool commit)
	{
		if (!m_GizmoDragTracked)
			return;
		m_GizmoDragTracked = false;
		if (commit && m_Selected && Blaze::IsObjectLive(m_Selected))
		{
			Blaze::LocalTransformSnap after = Blaze::LocalTransformSnap::Capture(*m_Selected);
			if (!m_GizmoUndoBefore.Equals(after))
			{
				Blaze::UndoStack::Get().Push(std::make_unique<Blaze::TransformCommand>(
					m_Selected, m_GizmoUndoBefore, after));
			}
		}
	}

	void PaintTileWithUndo(Blaze::GameObject* object, int x, int y, int tileId)
	{
		if (!object || !object->IsTilemap())
			return;

		if (m_TileStrokeObject != object)
		{
			EndTileStroke(true);
			m_TileStrokeObject = object;
		}

		const int oldId = object->GetTile(x, y);
		if (oldId == tileId)
			return;

		for (Blaze::TileChange& change : m_TileStrokeChanges)
		{
			if (change.x == x && change.y == y)
			{
				change.newId = tileId;
				object->SetTile(x, y, tileId);
				return;
			}
		}

		m_TileStrokeChanges.push_back({ x, y, oldId, tileId });
		object->SetTile(x, y, tileId);
	}

	void EndTileStroke(bool commit)
	{
		if (commit && m_TileStrokeObject && !m_TileStrokeChanges.empty()
			&& Blaze::IsObjectLive(m_TileStrokeObject))
		{
			Blaze::UndoStack::Get().Push(std::make_unique<Blaze::TileStrokeCommand>(
				m_TileStrokeObject, std::move(m_TileStrokeChanges)));
		}
		m_TileStrokeChanges.clear();
		m_TileStrokeObject = nullptr;
	}

	void SyncSelectionAfterHistory()
	{
		if (m_Selected && !Blaze::IsObjectLive(m_Selected))
		{
			m_Selected = nullptr;
			m_DragAxis = Blaze::GizmoAxis::None;
		}
	}

	void CreateDefaultCamera()
	{
		render->CreateObject(vertices, indices);
		Blaze::GameObject& cam = *render->GetObjects().back();
		cam.SetName("Main Camera");
		cam.SetType(Blaze::ObjectType::Camera);
		cam.SetDrivesView(true);
	}

	void ApplyProjectToBrowser()
	{
		if (auto* context = Blaze::Application::Get().GetContext())
			context->m_contextWindow->SetRootDirectory(Blaze::Project::GetContentDirectory());
	}

	void OpenMapFile(const std::string& path)
	{
		if (path.empty() || !Blaze::Project::HasProject())
			return;

		if (Blaze::StartWindow::IsPlaying())
			Blaze::StartWindow::SetPlaying(false);

		m_Selected = nullptr;
		m_DragAxis = Blaze::GizmoAxis::None;
		EndGizmoDrag(false);
		EndTileStroke(false);
		Blaze::UndoStack::Get().Clear();
		if (!Blaze::Project::LoadMap(path, *render, vertices, indices, GetObjectsWindow()))
			return;

		if (CountCameraObjects() == 0)
			CreateDefaultCamera();

		ApplyProjectToBrowser();
		BZ_INFO("Opened map: {0}", path);
	}

	// In-game map change only. Editor map is restored when play stops.
	void SwitchPlayMap(const std::string& path)
	{
		if (path.empty() || !Blaze::Project::HasProject())
			return;

		Blaze::AudioEngine::OnPlayModeStop();
		Blaze::ScriptEngine::OnPlayModeStop();
		physics->Reset();
		m_Selected = nullptr;
		m_DragAxis = Blaze::GizmoAxis::None;
		m_EditorSnapshot.clear();

		if (!Blaze::Project::LoadMap(path, *render, vertices, indices, GetObjectsWindow()))
		{
			BZ_ERROR("OpenMap failed during play: {0}", path);
			Blaze::StartWindow::SetPlaying(false);
			return;
		}

		m_PlaySwitchedMap = true;
		CaptureEditorSnapshot();
		Blaze::ScriptEngine::SetRenderer(render);
		Blaze::ScriptEngine::SetPhysicsWorld(physics);
		if (!Blaze::ScriptEngine::OnPlayModeStart(render->GetObjects()))
		{
			Blaze::StartWindow::SetPlaying(false);
			return;
		}

		physics->Reset();
		Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
		BZ_INFO("Play map switched: {0}", path);
	}

	void HandleHubActions()
	{
		switch (Blaze::ProjectHub::ConsumeAction())
		{
			case Blaze::HubAction::CreateProject:
			{
				ClearScene();
				CreateDefaultCamera();
				Blaze::Project::SaveScene(*render, GetObjectsWindow());
				ApplyProjectToBrowser();
				break;
			}
			case Blaze::HubAction::LoadProject:
			{
				m_Selected = nullptr;
				m_DragAxis = Blaze::GizmoAxis::None;
				EndGizmoDrag(false);
				EndTileStroke(false);
				Blaze::UndoStack::Get().Clear();
				Blaze::Project::LoadScene(*render, vertices, indices, GetObjectsWindow());
				ApplyProjectToBrowser();
				break;
			}
			default:
				break;
		}
	}

	void HandleFileMenu()
	{
		switch (Blaze::MenuBar::ConsumeFileAction())
		{
			case Blaze::FileAction::NewProject:
			{
				if (Blaze::Project::NewProject())
				{
					ClearScene();
					CreateDefaultCamera();
					Blaze::Project::SaveScene(*render, GetObjectsWindow());
					ApplyProjectToBrowser();
				}
				break;
			}
			case Blaze::FileAction::OpenProject:
			{
				if (Blaze::Project::OpenProjectDialog())
				{
					m_Selected = nullptr;
					m_DragAxis = Blaze::GizmoAxis::None;
					EndGizmoDrag(false);
					EndTileStroke(false);
					Blaze::UndoStack::Get().Clear();
					if (Blaze::Project::LoadScene(*render, vertices, indices, GetObjectsWindow()))
						ApplyProjectToBrowser();
				}
				break;
			}
			case Blaze::FileAction::Save:
			{
				if (Blaze::Project::SaveScene(*render, GetObjectsWindow()))
					ApplyProjectToBrowser();
				break;
			}
			case Blaze::FileAction::SaveAs:
			{
				if (Blaze::Project::SaveSceneAs(*render, GetObjectsWindow()))
					ApplyProjectToBrowser();
				break;
			}
			case Blaze::FileAction::CloseProject:
			{
				ClearScene();
				Blaze::Project::CloseProject();
				if (auto* context = Blaze::Application::Get().GetContext())
					context->m_contextWindow->SetRootDirectory("content");
				break;
			}
			case Blaze::FileAction::ProjectSettings:
			{
				if (auto* context = Blaze::Application::Get().GetContext())
					context->m_projectSettings->Open();
				break;
			}
			case Blaze::FileAction::BuildProject:
			{
				if (Blaze::Project::SaveScene(*render, GetObjectsWindow()))
					Blaze::Project::BuildGame();
				break;
			}
			default:
				break;
		}
	}

	struct EditorPose
	{
		Blaze::GameObject* object = nullptr;
		glm::vec3 position = glm::vec3(0.0f);
		float rotation = 0.0f;
		glm::vec2 scale = glm::vec2(1.0f);
	};

	void CaptureEditorSnapshot()
	{
		m_EditorSnapshot.clear();
		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (!object)
				continue;
			// Local space so parenting / play movement never pollutes the editor pose.
			EditorPose pose;
			pose.object = object;
			pose.position = object->GetLocalPosition();
			pose.rotation = object->GetLocalRotation();
			pose.scale = object->GetLocalScale();
			m_EditorSnapshot.push_back(pose);
		}
	}

	void RestoreEditorSnapshot()
	{
		for (const EditorPose& pose : m_EditorSnapshot)
		{
			if (!pose.object)
				continue;
			pose.object->SetLocalPosition(pose.position);
			pose.object->SetLocalRotation(pose.rotation);
			pose.object->SetLocalScale(pose.scale);
		}
	}

	void FlushScriptDestroys()
	{
		std::vector<Blaze::GameObject*> doomed = Blaze::ScriptEngine::ConsumePendingDestroys();
		if (doomed.empty())
			return;

		for (Blaze::GameObject* object : doomed)
		{
			if (!object)
				continue;

			m_EditorSnapshot.erase(
				std::remove_if(m_EditorSnapshot.begin(), m_EditorSnapshot.end(),
					[object](const EditorPose& pose) { return pose.object == object; }),
				m_EditorSnapshot.end());

			if (m_Selected == object)
			{
				m_Selected = nullptr;
				m_DragAxis = Blaze::GizmoAxis::None;
			}

			physics->RemoveObject(object);
			render->DestroyObject(object);
		}

		if (auto* context = Blaze::Application::Get().GetContext())
		{
			context->m_objectWindow->SetObjects(render->GetObjects());
			if (context->m_objectWindow->IsMapSelected())
			{
				m_Selected = nullptr;
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(true);
			}
			else
			{
				context->m_objectWindow->SetSelectedObject(m_Selected);
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(false);
				context->m_componentsWindow->SetSelectedObject(m_Selected);
			}
		}
	}

	void OnUpdate() override
	{
		HandleHubActions();

		if (!Blaze::Project::HasProject())
			return;

		if (!Blaze::UIBlocksMainEditor() && !Blaze::StartWindow::IsPlaying())
		{
			// Right-click erases tiles on a selected tilemap — don't pan the camera.
			const bool blockCameraPan =
				m_TileEraseStrokeActive ||
				(m_Selected && m_Selected->IsTilemap()
					&& Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT));
			if (!blockCameraPan)
				camera->CameraMove();
		}
		HandleFileMenu();

		std::string openMapPath;
		if (Blaze::ContentBrowser::ConsumePendingOpenMap(openMapPath))
			OpenMapFile(openMapPath);
		else if (Blaze::ProjectSettingsWindow::ConsumePendingOpenMap(openMapPath))
			OpenMapFile(openMapPath);
		else if (Blaze::ScriptEngine::ConsumePendingOpenMap(openMapPath))
		{
			if (Blaze::StartWindow::IsPlaying())
				SwitchPlayMap(openMapPath);
			else
				OpenMapFile(openMapPath);
		}

		if (Blaze::UIBlocksMainEditor())
		{
			EndGizmoDrag(true);
			EndTileStroke(true);
			m_DragAxis = Blaze::GizmoAxis::None;
		}

		if (auto* context = Blaze::Application::Get().GetContext())
		{
			if (Blaze::GameObject* picked = context->m_objectWindow->ConsumePickedObject())
				m_Selected = picked;

			if (Blaze::GameObject* focusObject = context->m_objectWindow->ConsumePendingFocus())
			{
				m_Selected = focusObject;
				FocusEditorOnObject(focusObject);
			}

			if (Blaze::GameObject* toDelete = context->m_objectWindow->ConsumePendingDelete())
			{
				if (toDelete->IsCamera() && CountCameraObjects() <= 1)
				{
					BZ_INFO("Cannot delete the last camera");
				}
				else
				{
					DeleteObjectWithUndo(toDelete);
				}
			}

			context->m_objectWindow->SetObjects(render->GetObjects());
			if (context->m_objectWindow->IsMapSelected())
			{
				m_Selected = nullptr;
				m_DragAxis = Blaze::GizmoAxis::None;
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(true);
			}
			else
			{
				context->m_objectWindow->SetSelectedObject(m_Selected);
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(false);
				context->m_componentsWindow->SetSelectedObject(m_Selected);
			}
		}

		std::string droppedObjectType;
		if (Blaze::StartWindow::IsPlaying())
		{
			Blaze::PropertiesWindow::ConsumePendingDrop(droppedObjectType);
		}
		else if (Blaze::PropertiesWindow::ConsumePendingDrop(droppedObjectType))
		{
			render->CreateObject(vertices, indices);
			Blaze::GameObject& object = *render->GetObjects().back();
			glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
			object.SetPosition(spawnPos);

			if (droppedObjectType == "sprite")
			{
				object.SetType(Blaze::ObjectType::Sprite);
				object.SetTexture("content/texture/container.jpg");
			}
			else if (droppedObjectType == "tilemap")
			{
				object.SetName("Tilemap");
				object.SetType(Blaze::ObjectType::Tilemap);
				object.SetTexture("content/texture/container.jpg");
				object.InitTilemapDefaults();
				object.SetTilesetGrid(1, 1);
				object.SetTileCollisionEnabled(true);
				object.SetAllTilesetSolid(true);
			}
			else if (droppedObjectType == "text")
			{
				object.SetName("Text");
				object.SetType(Blaze::ObjectType::Text);
				object.SetText("Text");
				object.SetFontSize(48.0f);
				object.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else if (droppedObjectType == "circle")
			{
				object.SetName("Circle");
				object.SetType(Blaze::ObjectType::Circle);
				object.AddCollision(Blaze::CollisionType::Circle);
				object.SetCollisionRadius(50.0f);
			}
			else if (droppedObjectType == "box")
			{
				object.SetName("Box");
				object.SetType(Blaze::ObjectType::Box);
				object.AddCollision(Blaze::CollisionType::Block);
				object.SetCollisionSize(glm::vec2(100.0f, 100.0f));
			}
			else if (droppedObjectType == "camera")
			{
				object.SetName("Camera");
				object.SetType(Blaze::ObjectType::Camera);
				if (!FindCameraObject() || !FindCameraObject()->DrivesView())
					object.SetDrivesView(true);
			}
			else if (droppedObjectType == "sound")
			{
				object.SetName("Sound");
				object.SetType(Blaze::ObjectType::Sound);
				object.AddAudio();
			}

			m_Selected = &object;
			PushCreatedObject(object);
		}

		std::string droppedAsset;
		if (Blaze::StartWindow::IsPlaying())
		{
			Blaze::ContentBrowser::ConsumePendingDrop(droppedAsset);
		}
		else if (Blaze::ContentBrowser::ConsumePendingDrop(droppedAsset))
		{
			render->CreateObject(vertices, indices);
			Blaze::GameObject& object = *render->GetObjects().back();
			glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
			object.SetPosition(spawnPos);

			std::string ext = std::filesystem::path(droppedAsset).extension().string();
			for (auto& c : ext)
				c = (char)std::tolower((unsigned char)c);

			if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
			{
				object.SetName("Sound");
				object.SetType(Blaze::ObjectType::Sound);
				object.AddAudio();
				object.SetAudioPath(droppedAsset);
			}
			else
			{
				object.SetTexture(droppedAsset);
			}
			m_Selected = &object;
			PushCreatedObject(object);
		}

		if (m_DragAxis != Blaze::GizmoAxis::None && !Blaze::UIBlocksMainEditor()
			&& !Blaze::StartWindow::IsPlaying())
		{
			if (m_Selected && Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT))
			{
				auto prop = camera->GetCameraProp();
				float sx = prop.projection[0][0];
				float sy = prop.projection[1][1];

				switch (gizmo->GetMode())
				{
					case Blaze::GizmoMode::Translate:
					{
						glm::vec2 world = GetMouseWorldPos();
						glm::vec3 delta(world - m_LastMouseWorld, 0.0f);
						m_LastMouseWorld = world;

						if (m_DragAxis == Blaze::GizmoAxis::X)
							delta.y = 0.0f;
						else if (m_DragAxis == Blaze::GizmoAxis::Y)
							delta.x = 0.0f;
						m_Selected->Translate(delta);
						break;
					}
					case Blaze::GizmoMode::Rotate:
					{
						glm::vec2 world = GetMouseWorldPos();
						glm::vec2 c(m_Selected->GetPosition());
						float angle = glm::degrees(glm::atan((world.y - c.y) / sy, (world.x - c.x) / sx));
						float delta = angle - m_LastMouseAngle;
						while (delta > 180.0f) delta -= 360.0f;
						while (delta < -180.0f) delta += 360.0f;
						m_Selected->Rotate(delta);
						m_LastMouseAngle = angle;
						m_LastMouseWorld = world;
						break;
					}
					case Blaze::GizmoMode::Scale:
					{
						glm::vec2 world = GetMouseWorldPos();
						float dxPx = (world.x - m_LastMouseWorld.x) / sx;
						float dyPx = (world.y - m_LastMouseWorld.y) / sy;
						glm::vec2 scale = m_Selected->GetScale();
						if (m_DragAxis == Blaze::GizmoAxis::X)
							scale.x += dxPx / 100.0f;
						else if (m_DragAxis == Blaze::GizmoAxis::Y)
							scale.y += dyPx / 100.0f;
						else
							scale += (dxPx + dyPx) * 0.5f / 100.0f;
						scale.x = std::max(scale.x, 0.05f);
						scale.y = std::max(scale.y, 0.05f);
						m_Selected->SetScale(scale);
						m_LastMouseWorld = world;
						break;
					}
				}
			}
			else
			{
				EndGizmoDrag(true);
				m_DragAxis = Blaze::GizmoAxis::None;
			}
		}
		else if (!Blaze::UIBlocksMainEditor() && !Blaze::StartWindow::IsPlaying()
			&& m_Selected && m_Selected->IsTilemap())
		{
			auto prop = camera->GetCameraProp();
			glm::vec2 world = GetMouseWorldPos();
			int tx = -1, ty = -1;
			const bool overTile = m_Selected->WorldToTile(prop, world, tx, ty);
			m_HoverTileX = overTile ? tx : -1;
			m_HoverTileY = overTile ? ty : -1;

			if (overTile && !Blaze::UIWantCaptureMouse())
			{
				if (m_TileStrokeActive || Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT))
				{
					if (Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT))
					{
						m_TileStrokeActive = true;
						PaintTileWithUndo(m_Selected, tx, ty, Blaze::ComponentsWindow::GetTilePaintBrush());
					}
				}
				if (m_TileEraseStrokeActive || Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
				{
					if (Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
					{
						m_TileEraseStrokeActive = true;
						PaintTileWithUndo(m_Selected, tx, ty, -1);
					}
				}
			}

			if (!Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT))
			{
				if (m_TileStrokeActive)
					EndTileStroke(true);
				m_TileStrokeActive = false;
			}
			if (!Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
			{
				if (m_TileEraseStrokeActive)
					EndTileStroke(true);
				m_TileEraseStrokeActive = false;
			}
		}
		else
		{
			m_HoverTileX = -1;
			m_HoverTileY = -1;
			if (m_TileStrokeActive || m_TileEraseStrokeActive)
				EndTileStroke(true);
			m_TileStrokeActive = false;
			m_TileEraseStrokeActive = false;
		}

		const bool playing = Blaze::StartWindow::IsPlaying();
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;

		if (playing)
		{
			if (!m_WasPlaying)
			{
				m_EditorMapPath = Blaze::Project::GetActiveMap();
				m_PlaySwitchedMap = false;
				CaptureEditorSnapshot();
				Blaze::ScriptEngine::SetRenderer(render);
				Blaze::ScriptEngine::SetPhysicsWorld(physics);
				if (!Blaze::ScriptEngine::OnPlayModeStart(render->GetObjects()))
				{
					// Syntax/compile error — abort play before physics/audio/scripts run
					Blaze::StartWindow::SetPlaying(false);
					m_EditorSnapshot.clear();
					m_EditorMapPath.clear();
				}
				else
				{
					physics->Reset();
					Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
				}
			}

			if (Blaze::StartWindow::IsPlaying())
			{
				Blaze::ScriptEngine::OnUpdate(dt, render->GetObjects());
				FlushScriptDestroys();

				physics->Step(dt, render->GetObjects());
				Blaze::ScriptEngine::OnCollisionEvents(physics->ConsumeCollisionEvents());
				FlushScriptDestroys();
				physics->ApplyPlayTransforms(render->GetObjects());
				Blaze::ScriptEngine::UpdateCameraFollow(dt);
				// Keep play poses (incl. physics + camera follow) so next frame can resume
				// after the editor scene is frozen below.
				Blaze::ScriptEngine::CapturePlayTransforms(render->GetObjects());
				render->Update(dt);
				RenderGameView();
				// Freeze scene/editor at the poses from when play started.
				RestoreEditorSnapshot();
			}
		}
		else if (m_WasPlaying)
		{
			physics->Reset();
			Blaze::AudioEngine::OnPlayModeStop();
			Blaze::ScriptEngine::OnPlayModeStop();
			Blaze::UISystem::HideAll();
			m_Selected = nullptr;

			// OpenMap during play must not leave the play map open in the editor.
			if (m_PlaySwitchedMap && !m_EditorMapPath.empty())
			{
				m_EditorSnapshot.clear();
				Blaze::Project::LoadMap(m_EditorMapPath, *render, vertices, indices, GetObjectsWindow());
				if (CountCameraObjects() == 0)
					CreateDefaultCamera();
				ApplyProjectToBrowser();
			}
			else
			{
				RestoreEditorSnapshot();
				m_EditorSnapshot.clear();
			}

			m_EditorMapPath.clear();
			m_PlaySwitchedMap = false;
		}
		m_WasPlaying = Blaze::StartWindow::IsPlaying();

		if (!Blaze::StartWindow::IsPlaying())
			render->Update(dt);

		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();

		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (!object || !object->IsVisible())
				continue;
			if (object->IsCamera())
			{
				gizmo->DrawCameraIcon(*camera, *object);
				if (object->DrivesView())
					gizmo->DrawCameraRange(*camera, *object);
			}
			if (object->IsSound())
				gizmo->DrawSoundIcon(*camera, *object);
			if (object->HasCollision() && !object->IsSound() && !object->IsCamera())
				gizmo->DrawCollision(*camera, *object);
		}

		if (m_Selected)
			gizmo->Draw(*camera, *m_Selected);

		if (m_Selected && m_Selected->IsTilemap())
			gizmo->DrawTilemapGrid(*camera, *m_Selected, m_HoverTileX, m_HoverTileY);
	}

	void OnEvent(Blaze::Event& event) override
	{
		static float height = 900.f, width = 1600.f;
		if (event.GetEventType() == Blaze::EventType::WindowResize)
		{
			auto& e = (Blaze::WindowResizeEvent&)event;
			// Minimized windows report 0x0 — ignore so zoom/projection stay valid.
			if (e.GetWidth() == 0 || e.GetHeight() == 0)
				return;

			if (width > 0.0f && height > 0.0f)
			{
				camera->zoom *= (float)e.GetWidth() / width;
				camera->zoom *= (float)e.GetHeight() / height;
			}

			Blaze::Camera::m_prop.projection = glm::ortho(
				(float)(-(int)e.GetWidth() / 2), (float)((int)e.GetWidth() / 2),
				(float)(-(int)e.GetHeight() / 2), (float)((int)e.GetHeight() / 2),
				-100.f, 100.f);

			width = (float)e.GetWidth();
			height = (float)e.GetHeight();
		}

		if (!Blaze::Project::HasProject())
			return;

		// Play mode owns the simulation — don't edit/pick/paint in the editor scene.
		if (Blaze::StartWindow::IsPlaying())
		{
			if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
			{
				EndGizmoDrag(false);
				m_DragAxis = Blaze::GizmoAxis::None;
				m_TileStrokeActive = false;
				m_TileEraseStrokeActive = false;
			}
			return;
		}

		// Script editor / text fields own input — ignore scene shortcuts & picking.
		if (Blaze::UIBlocksMainEditor())
		{
			if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
			{
				EndGizmoDrag(true);
				m_DragAxis = Blaze::GizmoAxis::None;
			}
			return;
		}

		if (event.GetEventType() == Blaze::EventType::MouseButtonPressed)
		{
			auto& e = (Blaze::MouseButtonPressedEvent&)event;
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT && !Blaze::UIWantCaptureMouse())
			{
				glm::vec2 world = GetMouseWorldPos();
				auto prop = camera->GetCameraProp();

				auto& objects = render->GetObjects();

				if (m_Selected)
				{
					if (m_Selected->IsTilemap())
					{
						// Tilemap: paint on the grid by default. Only X/Y arrows move it.
						Blaze::GizmoAxis axis = gizmo->HitTest(prop, *m_Selected, world);
						const bool armHandle = (axis == Blaze::GizmoAxis::X || axis == Blaze::GizmoAxis::Y);
						if (armHandle && !Blaze::ComponentsWindow::IsTilePaintMode())
						{
							m_DragAxis = axis;
							m_LastMouseWorld = world;
							m_LastMouseAngle = GetMouseAngle(world);
							BeginGizmoDrag();
							return;
						}

						int tx = -1, ty = -1;
						const bool overTile = m_Selected->WorldToTile(prop, world, tx, ty);

						Blaze::GameObject* otherUnderCursor = nullptr;
						int otherLayer = 0;
						bool otherFound = false;
						for (auto it = objects.rbegin(); it != objects.rend(); ++it)
						{
							Blaze::GameObject* object = *it;
							if (!object || object == m_Selected || !object->IsHovered(prop, world))
								continue;
							const int layer = object->GetLayer();
							if (!otherFound || layer > otherLayer)
							{
								otherUnderCursor = object;
								otherLayer = layer;
								otherFound = true;
							}
						}

						// Clicking another object selects it (even over the map),
						// unless tile-paint mode is on and the click is on a tile.
						if (otherFound &&
							!(Blaze::ComponentsWindow::IsTilePaintMode() && overTile))
						{
							m_Selected = otherUnderCursor;
							if (auto* context = Blaze::Application::Get().GetContext())
								context->m_objectWindow->SetMapSelected(false);

							const auto now = std::chrono::steady_clock::now();
							const double elapsed = std::chrono::duration<double>(now - m_LastViewportClickTime).count();
							if (m_Selected == m_LastViewportClickObject && elapsed < 0.35)
							{
								FocusEditorOnObject(m_Selected);
								m_LastViewportClickObject = nullptr;
								m_LastViewportClickTime = {};
							}
							else
							{
								m_LastViewportClickObject = m_Selected;
								m_LastViewportClickTime = now;
							}

							if (m_Selected && gizmo->GetMode() == Blaze::GizmoMode::Translate
								&& !m_Selected->IsTilemap())
							{
								m_DragAxis = Blaze::GizmoAxis::XY;
								m_LastMouseWorld = world;
								BeginGizmoDrag();
							}
							return;
						}
						else if (overTile)
						{
							m_DragAxis = Blaze::GizmoAxis::None;
							PaintTileWithUndo(m_Selected, tx, ty, Blaze::ComponentsWindow::GetTilePaintBrush());
							m_HoverTileX = tx;
							m_HoverTileY = ty;
							m_TileStrokeActive = true;
							return;
						}
						// Missed the grid — fall through so other objects can be picked.
					}
					else
					{
						Blaze::GizmoAxis axis = gizmo->HitTest(prop, *m_Selected, world);
						if (axis != Blaze::GizmoAxis::None)
						{
							m_DragAxis = axis;
							m_LastMouseWorld = world;
							m_LastMouseAngle = GetMouseAngle(world);
							BeginGizmoDrag();
							return;
						}
					}
				}

				m_Selected = nullptr;
				bool found = false;
				int bestLayer = 0;
				for (auto it = objects.rbegin(); it != objects.rend(); ++it)
				{
					if (!(*it)->IsHovered(prop, world))
						continue;
					const int layer = (*it)->GetLayer();
					if (!found || layer > bestLayer)
					{
						m_Selected = *it;
						bestLayer = layer;
						found = true;
					}
				}

				if (auto* context = Blaze::Application::Get().GetContext())
				{
					if (found)
						context->m_objectWindow->SetMapSelected(false);
				}

				if (found && m_Selected)
				{
					const auto now = std::chrono::steady_clock::now();
					const double elapsed = std::chrono::duration<double>(now - m_LastViewportClickTime).count();
					if (m_Selected == m_LastViewportClickObject && elapsed < 0.35)
					{
						FocusEditorOnObject(m_Selected);
						m_LastViewportClickObject = nullptr;
						m_LastViewportClickTime = {};
					}
					else
					{
						m_LastViewportClickObject = m_Selected;
						m_LastViewportClickTime = now;
					}
				}

				if (m_Selected && gizmo->GetMode() == Blaze::GizmoMode::Translate
					&& !m_Selected->IsTilemap())
				{
					m_DragAxis = Blaze::GizmoAxis::XY;
					m_LastMouseWorld = world;
					BeginGizmoDrag();
				}
			}
		}

		if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
		{
			auto& e = (Blaze::MouseButtonReleasedEvent&)event;
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT)
			{
				EndGizmoDrag(true);
				m_DragAxis = Blaze::GizmoAxis::None;
				if (m_TileStrokeActive)
					EndTileStroke(true);
				m_TileStrokeActive = false;
			}
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_RIGHT)
			{
				if (m_TileEraseStrokeActive)
					EndTileStroke(true);
				m_TileEraseStrokeActive = false;
			}
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			const bool ctrl = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_CONTROL) ||
				Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_CONTROL);
			const bool shift = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_SHIFT) ||
				Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_SHIFT);

			if (e.GetKeyCode() == BZ_KEY_F && !ctrl && m_Selected)
			{
				FocusEditorOnObject(m_Selected);
				return;
			}

			if (e.GetKeyCode() == BZ_KEY_S && ctrl)
			{
				if (Blaze::Project::SaveScene(*render, GetObjectsWindow()))
					ApplyProjectToBrowser();
				return;
			}

			if (ctrl && e.GetKeyCode() == BZ_KEY_Z && !shift)
			{
				EndGizmoDrag(true);
				EndTileStroke(true);
				Blaze::UndoStack::Get().Undo();
				SyncSelectionAfterHistory();
				return;
			}
			if ((ctrl && e.GetKeyCode() == BZ_KEY_Y)
				|| (ctrl && shift && e.GetKeyCode() == BZ_KEY_Z))
			{
				EndGizmoDrag(true);
				EndTileStroke(true);
				Blaze::UndoStack::Get().Redo();
				SyncSelectionAfterHistory();
				return;
			}

			if (e.GetKeyCode() == BZ_KEY_R && !ctrl)
				gizmo->SetMode(Blaze::GizmoMode::Translate);
			if (e.GetKeyCode() == BZ_KEY_T && !ctrl)
				gizmo->SetMode(Blaze::GizmoMode::Rotate);
			if (e.GetKeyCode() == BZ_KEY_Y && !ctrl)
				gizmo->SetMode(Blaze::GizmoMode::Scale);
			if (e.GetKeyCode() == BZ_KEY_B && !ctrl && m_Selected && m_Selected->IsTilemap())
			{
				Blaze::ComponentsWindow::ToggleTilePaintMode();
				return;
			}

			if (e.GetKeyCode() == BZ_KEY_DELETE && m_Selected)
			{
				if (m_Selected->IsCamera() && CountCameraObjects() <= 1)
				{
					BZ_INFO("Cannot delete the last camera");
				}
				else
				{
					DeleteObjectWithUndo(m_Selected);
				}
			}
		}
	}

public:
	std::vector<float> vertices =
	{
		 50.f,  50.f, 0.0f,1.0f, 1.0f,  // top right
		 50.f, -50.f, 0.0f,1.0f, 0.0f, // bottom right
		-50.f, -50.f, 0.0f,0.0f, 0.0f, // bottom left
		-50.f,  50.f, 0.0f,0.0f, 1.0f,
	};

	std::vector<unsigned int> indices =
	{
		0, 1, 3,
		1, 2, 3 
	};


	Blaze::Renderer* render;
	Blaze::Camera* camera;
	Blaze::Gizmo2D* gizmo;
	Blaze::PhysicsWorld2D* physics;

	Blaze::GameObject* m_Selected = nullptr;
	Blaze::GizmoAxis m_DragAxis = Blaze::GizmoAxis::None;
	int m_HoverTileX = -1;
	int m_HoverTileY = -1;
	bool m_TileStrokeActive = false;
	bool m_TileEraseStrokeActive = false;
	bool m_GizmoDragTracked = false;
	Blaze::LocalTransformSnap m_GizmoUndoBefore;
	std::vector<Blaze::TileChange> m_TileStrokeChanges;
	Blaze::GameObject* m_TileStrokeObject = nullptr;
	glm::vec2 m_LastMouseWorld = glm::vec2(0.0f);
	float m_LastMouseAngle = 0.0f;
	std::chrono::steady_clock::time_point m_LastTime;
	bool m_WasPlaying = false;
	bool m_PlaySwitchedMap = false;
	std::string m_EditorMapPath;
	std::vector<EditorPose> m_EditorSnapshot;
	Blaze::GameObject* m_LastViewportClickObject = nullptr;
	std::chrono::steady_clock::time_point m_LastViewportClickTime{};

	const char* vertexShaderSource = "#version 330 core\n"
		"layout (location = 0) in vec3 pos;\n"
		"layout (location = 1) in vec2 texCoord;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 ortho;\n"
		"uniform mat4 object_matrix;\n"
		"uniform vec2 uv_scale;\n"
		"uniform vec2 uv_offset;\n"
		"out vec2 TexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(matrix * object_matrix * ortho * vec4(pos,1.0f));\n"
		"   TexCoord = texCoord * uv_scale + uv_offset;\n"
		"}\0";

	const char* fragmentShaderSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 FragColor;\n"
		"in vec2 TexCoord;\n"
		"uniform sampler2D in_texture;\n"
		"void main()\n"
		"{\n"
		"   FragColor = texture(in_texture,TexCoord) * color;\n"
		"}\n\0";
};

class Sandbox : public Blaze::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{

	}

};

Blaze::Application* Blaze::CreateApplication()
{
	return new Sandbox();
}