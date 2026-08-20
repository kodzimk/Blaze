#include <Blaze.h>
#include<glad/glad.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

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
			Blaze::ParticleSystem2D::Flush(*camera, *gizmo);
			Blaze::Camera::m_prop.matrix = savedView;
		}

		if (!Blaze::UISystem::IsReady())
			Blaze::UISystem::Init();
		Blaze::UISystem::Draw(
			Blaze::GameWindow::GetFrameWidth(), Blaze::GameWindow::GetFrameHeight());

		Blaze::PostProcess2D::Apply(render->GetObjects(),
			Blaze::GameWindow::GetColorTexture(),
			Blaze::GameWindow::GetFramebuffer(),
			Blaze::GameWindow::GetFrameWidth(),
			Blaze::GameWindow::GetFrameHeight());

		auto& window = Blaze::Application::Get().GetWindow();
		Blaze::GameWindow::EndCapture((int)window.GetWidth(), (int)window.GetHeight());
	}

	~ExampleLayer()
	{
		Blaze::AudioEngine::Shutdown();
		Blaze::ScriptEngine::Shutdown();
		Blaze::PostProcess2D::Shutdown();
		delete physics;
		delete gizmo;
		Blaze::UISystem::Shutdown();
		delete render;
		delete camera;
	}

	glm::vec2 GetMouseWorldPos() const
	{
		float sx = 0.0f, sy = 0.0f;
		if (Blaze::SceneWindow::TryGetMouseScenePixel(sx, sy))
		{
			return Blaze::Camera::ScreenToWorld(
				sx, sy,
				(float)Blaze::SceneWindow::GetFrameWidth(),
				(float)Blaze::SceneWindow::GetFrameHeight());
		}

		auto& window = Blaze::Application::Get().GetWindow();
		return Blaze::Camera::ScreenToWorld(
			Blaze::Input::GetMouseX(), Blaze::Input::GetMouseY(),
			(float)window.GetWidth(), (float)window.GetHeight());
	}

	bool CanInteractWithScene() const
	{
		if (Blaze::StartWindow::IsPlaying())
			return false;
		if (Blaze::SceneWindow::WantsEditorInput())
			return true;
		return !Blaze::UIWantCaptureMouse();
	}

	void SyncEditorCameraToSceneSize(int w, int h)
	{
		w = std::max(w, 1);
		h = std::max(h, 1);
		if (w == m_SceneCamW && h == m_SceneCamH)
			return;

		if (m_SceneCamW > 0 && m_SceneCamH > 0)
		{
			camera->zoom *= (float)w / (float)m_SceneCamW;
			camera->zoom *= (float)h / (float)m_SceneCamH;
			if (!std::isfinite(camera->zoom) || camera->zoom < 0.05f)
				camera->zoom = 0.05f;
			if (camera->zoom > 20.0f)
				camera->zoom = 20.0f;
			Blaze::Camera::ScaleCamera();
		}

		Blaze::Camera::m_prop.projection = glm::ortho(
			(float)(-w / 2), (float)(w / 2),
			(float)(-h / 2), (float)(h / 2),
			-100.f, 100.f);
		m_SceneCamW = w;
		m_SceneCamH = h;
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

	void ClearSelection()
	{
		m_Selection.clear();
		m_Selected = nullptr;
	}

	bool IsInSelection(Blaze::GameObject* object) const
	{
		for (Blaze::GameObject* sel : m_Selection)
		{
			if (sel == object)
				return true;
		}
		return false;
	}

	void SetSoleSelection(Blaze::GameObject* object)
	{
		m_Selection.clear();
		if (object)
			m_Selection.push_back(object);
		m_Selected = object;
	}

	void AddToSelection(Blaze::GameObject* object)
	{
		if (!object)
			return;
		if (!IsInSelection(object))
			m_Selection.push_back(object);
		m_Selected = object;
	}

	void ToggleSelection(Blaze::GameObject* object)
	{
		if (!object)
			return;
		for (auto it = m_Selection.begin(); it != m_Selection.end(); ++it)
		{
			if (*it == object)
			{
				m_Selection.erase(it);
				if (m_Selected == object)
					m_Selected = m_Selection.empty() ? nullptr : m_Selection.back();
				return;
			}
		}
		m_Selection.push_back(object);
		m_Selected = object;
	}

	void PruneSelection()
	{
		m_Selection.erase(
			std::remove_if(m_Selection.begin(), m_Selection.end(),
				[](Blaze::GameObject* o) { return !o || !Blaze::IsObjectLive(o); }),
			m_Selection.end());
		if (m_Selected && !IsInSelection(m_Selected))
			m_Selected = m_Selection.empty() ? nullptr : m_Selection.back();
	}

	void SyncSelectionToUi()
	{
		if (auto* context = Blaze::Application::Get().GetContext())
		{
			if (context->m_objectWindow->IsMapSelected())
				return;
			context->m_objectWindow->SetSelectedObjects(m_Selection, m_Selected);
			context->m_componentsWindow->SetSelectedObject(m_Selected);
		}
	}

	void FocusEditorOnSelection()
	{
		PruneSelection();
		if (m_Selection.empty() || !camera)
			return;
		if (m_Selection.size() == 1)
		{
			FocusEditorOnObject(m_Selection.front());
			return;
		}

		auto prop = camera->GetCameraProp();
		glm::vec2 mn(1.0e9f), mx(-1.0e9f);
		bool any = false;
		for (Blaze::GameObject* object : m_Selection)
		{
			glm::vec2 a, b;
			object->GetWorldAABB(prop, a, b);
			mn = glm::min(mn, a);
			mx = glm::max(mx, b);
			any = true;
		}
		if (any)
			camera->FocusOnBounds(mn, mx);
	}

	std::vector<Blaze::GameObject*> GetSelectionRoots()
	{
		PruneSelection();
		std::vector<Blaze::GameObject*> roots;
		for (Blaze::GameObject* object : m_Selection)
		{
			if (!object)
				continue;
			bool childOfSelected = false;
			for (Blaze::GameObject* parent = object->GetParent(); parent; parent = parent->GetParent())
			{
				if (IsInSelection(parent))
				{
					childOfSelected = true;
					break;
				}
			}
			if (!childOfSelected)
				roots.push_back(object);
		}
		return roots;
	}

	void DuplicateSelection()
	{
		if (Blaze::StartWindow::IsPlaying() || Blaze::UIBlocksMainEditor())
			return;

		std::vector<Blaze::GameObject*> roots = GetSelectionRoots();
		if (roots.empty())
			return;

		EndGizmoDrag(true);
		EndTileStroke(true);

		std::vector<Blaze::GameObject*> newSelection;
		// Small nudge so the copy sits next to the original.
		const glm::vec3 offset(0.1f, 0.1f, 0.0f);
		for (Blaze::GameObject* root : roots)
		{
			if (!root || !Blaze::IsObjectLive(root))
				continue;
			std::vector<Blaze::GameObject*> created;
			if (Blaze::GameObject* copy = Blaze::PrefabAsset::Duplicate(
				root, *render, vertices, indices, offset, &created))
			{
				PushCreatedObjects(created);
				newSelection.push_back(copy);
			}
		}
		if (!newSelection.empty())
		{
			m_Selection = newSelection;
			m_Selected = newSelection.back();
			m_DragAxis = Blaze::GizmoAxis::None;
			if (auto* context = Blaze::Application::Get().GetContext())
				context->m_objectWindow->SetMapSelected(false);
			SyncSelectionToUi();
		}
	}

	void CopySelection()
	{
		if (Blaze::StartWindow::IsPlaying() || Blaze::UIBlocksMainEditor())
			return;

		std::vector<Blaze::GameObject*> roots = GetSelectionRoots();
		if (roots.empty())
			return;

		std::ostringstream out;
		out << "BlazeClipboard 1\n";
		for (Blaze::GameObject* root : roots)
			Blaze::PrefabAsset::WriteSubtree(out, root);
		m_Clipboard = out.str();
	}

	void PasteClipboard()
	{
		if (Blaze::StartWindow::IsPlaying() || Blaze::UIBlocksMainEditor())
			return;
		if (m_Clipboard.empty() || m_Clipboard.find("BlazePrefab") == std::string::npos)
			return;

		EndGizmoDrag(true);
		EndTileStroke(true);

		glm::vec3 basePos(0.0f);
		if (Blaze::SceneWindow::IsMouseOverSceneView())
		{
			glm::vec2 world = GetMouseWorldPos();
			basePos = glm::vec3(world, 0.0f);
		}
		else if (m_Selected && Blaze::IsObjectLive(m_Selected))
		{
			basePos = m_Selected->GetPosition() + glm::vec3(40.0f, 40.0f, 0.0f);
		}

		std::vector<Blaze::GameObject*> newSelection;
		size_t pos = 0;
		int index = 0;
		while (pos < m_Clipboard.size())
		{
			size_t start = m_Clipboard.find("BlazePrefab", pos);
			if (start == std::string::npos)
				break;
			size_t next = m_Clipboard.find("BlazePrefab", start + 11);
			std::string chunk = (next == std::string::npos)
				? m_Clipboard.substr(start)
				: m_Clipboard.substr(start, next - start);

			std::istringstream in(chunk);
			std::vector<Blaze::GameObject*> created;
			glm::vec3 spawn = basePos + glm::vec3(40.0f * (float)index, 40.0f * (float)index, 0.0f);
			if (Blaze::GameObject* root = Blaze::PrefabAsset::InstantiateFromStream(
				in, *render, vertices, indices, spawn, &created))
			{
				PushCreatedObjects(created);
				newSelection.push_back(root);
				++index;
			}
			if (next == std::string::npos)
				break;
			pos = next;
		}

		if (!newSelection.empty())
		{
			m_Selection = newSelection;
			m_Selected = newSelection.back();
			m_DragAxis = Blaze::GizmoAxis::None;
			if (auto* context = Blaze::Application::Get().GetContext())
				context->m_objectWindow->SetMapSelected(false);
			SyncSelectionToUi();
		}
	}

	Blaze::ObjectWindow* GetObjectsWindow()
	{
		if (auto* context = Blaze::Application::Get().GetContext())
			return context->m_objectWindow;
		return nullptr;
	}

	void ClearScene()
	{
		ClearSelection();
		m_DragAxis = Blaze::GizmoAxis::None;
		m_BoxSelecting = false;
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

	void PushCreatedObjects(const std::vector<Blaze::GameObject*>& objects)
	{
		for (Blaze::GameObject* object : objects)
		{
			if (object)
				PushCreatedObject(*object);
		}
	}

	bool SaveObjectAsPrefab(Blaze::GameObject* object)
	{
		if (!object)
			return false;

		char filePath[MAX_PATH] = {};
		std::string suggested = object->GetName().empty() ? "Prefab" : object->GetName();
		for (char& c : suggested)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
				c = '_';
		}
		std::string defaultDir;
		if (Blaze::Project::HasProject())
			defaultDir = (std::filesystem::path(Blaze::Project::GetContentDirectory()) / "prefabs").string();
		std::string defaultName = suggested + ".prefab";
		if (!defaultDir.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(defaultDir, ec);
			std::snprintf(filePath, MAX_PATH, "%s\\%s", defaultDir.c_str(), defaultName.c_str());
		}
		else
			std::snprintf(filePath, MAX_PATH, "%s", defaultName.c_str());

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "Blaze Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		ofn.lpstrDefExt = "prefab";
		if (!defaultDir.empty())
			ofn.lpstrInitialDir = defaultDir.c_str();

		if (!GetSaveFileNameA(&ofn))
			return false;

		Blaze::PrefabAsset asset;
		return asset.SaveFromRoot(object, filePath);
	}

	void DeleteObjectWithUndo(Blaze::GameObject* object)
	{
		if (!object)
			return;
		Blaze::UndoStack::Get().Push(
			std::make_unique<Blaze::DeleteObjectCommand>(Blaze::ObjectSnapshot::Capture(*object)));
		for (auto it = m_Selection.begin(); it != m_Selection.end(); )
		{
			if (*it == object)
				it = m_Selection.erase(it);
			else
				++it;
		}
		if (m_Selected == object)
			m_Selected = m_Selection.empty() ? nullptr : m_Selection.back();
		m_DragAxis = Blaze::GizmoAxis::None;
		EndGizmoDrag(false);
		EndTileStroke(false);
		render->DestroyObject(object);
	}

	void DeleteSelectionWithUndo()
	{
		PruneSelection();
		std::vector<Blaze::GameObject*> toDelete = m_Selection;
		for (Blaze::GameObject* object : toDelete)
		{
			if (!object || !Blaze::IsObjectLive(object))
				continue;
			if (object->IsCamera() && CountCameraObjects() <= 1)
			{
				BZ_INFO("Cannot delete the last camera");
				continue;
			}
			DeleteObjectWithUndo(object);
		}
	}

	void BeginGizmoDrag()
	{
		PruneSelection();
		if (!m_Selected)
			return;
		m_GizmoUndoBefores.clear();
		const std::vector<Blaze::GameObject*>& targets =
			m_Selection.empty() ? std::vector<Blaze::GameObject*>{ m_Selected } : m_Selection;
		for (Blaze::GameObject* object : targets)
		{
			if (object && Blaze::IsObjectLive(object))
				m_GizmoUndoBefores.push_back({ object, Blaze::LocalTransformSnap::Capture(*object) });
		}
		m_GizmoDragTracked = !m_GizmoUndoBefores.empty();
	}

	void EndGizmoDrag(bool commit)
	{
		if (!m_GizmoDragTracked)
			return;
		m_GizmoDragTracked = false;
		if (!commit)
		{
			m_GizmoUndoBefores.clear();
			return;
		}

		std::vector<Blaze::MultiTransformCommand::Entry> entries;
		for (auto& before : m_GizmoUndoBefores)
		{
			if (!before.object || !Blaze::IsObjectLive(before.object))
				continue;
			Blaze::LocalTransformSnap after = Blaze::LocalTransformSnap::Capture(*before.object);
			if (!before.snap.Equals(after))
				entries.push_back({ before.object, before.snap, after });
		}
		m_GizmoUndoBefores.clear();
		if (entries.size() == 1)
		{
			Blaze::UndoStack::Get().Push(std::make_unique<Blaze::TransformCommand>(
				entries[0].object, entries[0].before, entries[0].after));
		}
		else if (entries.size() > 1)
		{
			Blaze::UndoStack::Get().Push(std::make_unique<Blaze::MultiTransformCommand>(std::move(entries)));
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
		PruneSelection();
		if (!m_Selected)
			m_DragAxis = Blaze::GizmoAxis::None;
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

		ClearSelection();
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

	void SwitchPlayMap(const std::string& path)
	{
		if (path.empty() || !Blaze::Project::HasProject())
			return;

		Blaze::AudioEngine::OnPlayModeStop();
		Blaze::ParticleSystem2D::OnPlayModeStop();
		Blaze::ScriptEngine::OnPlayModeStop();
		physics->Reset();
		ClearSelection();
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
		Blaze::ParticleSystem2D::OnPlayModeStart(render->GetObjects());
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
				ClearSelection();
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
					ClearSelection();
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
			case Blaze::FileAction::BuildSettings:
			{
				if (auto* context = Blaze::Application::Get().GetContext())
				{
					context->m_buildSettings->SetSaveSceneCallback([this]() -> bool
					{
						return Blaze::Project::SaveScene(*render, GetObjectsWindow());
					});
					context->m_buildSettings->Open();
				}
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
				for (auto it = m_Selection.begin(); it != m_Selection.end(); )
				{
					if (*it == object)
						it = m_Selection.erase(it);
					else
						++it;
				}
				m_Selected = m_Selection.empty() ? nullptr : m_Selection.back();
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
				ClearSelection();
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(true);
			}
			else
			{
				SyncSelectionToUi();
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(false);
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
			const bool blockCameraPan =
				m_TileEraseStrokeActive ||
				(m_Selected && m_Selected->IsTilemap()
					&& Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT)) ||
				(Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT)
					&& !CanInteractWithScene());
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
			{
				const bool additive = context->m_objectWindow->ConsumePickedAdditive();
				if (additive)
					ToggleSelection(picked);
				else
					SetSoleSelection(picked);
			}

			if (Blaze::GameObject* focusObject = context->m_objectWindow->ConsumePendingFocus())
			{
				SetSoleSelection(focusObject);
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

			if (!Blaze::StartWindow::IsPlaying())
			{
				if (Blaze::GameObject* toSave = context->m_objectWindow->ConsumePendingSavePrefab())
					SaveObjectAsPrefab(toSave);

				if (Blaze::GameObject* toDup = context->m_objectWindow->ConsumePendingDuplicate())
				{
					std::vector<Blaze::GameObject*> created;
					if (Blaze::GameObject* copy = Blaze::PrefabAsset::Duplicate(
						toDup, *render, vertices, indices, glm::vec3(0.1f, 0.1f, 0.0f), &created))
					{
						PushCreatedObjects(created);
						SetSoleSelection(copy);
						SyncSelectionToUi();
					}
				}
			}

			context->m_objectWindow->SetObjects(render->GetObjects());
			if (context->m_objectWindow->IsMapSelected())
			{
				ClearSelection();
				m_DragAxis = Blaze::GizmoAxis::None;
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(true);
			}
			else
			{
				PruneSelection();
				SyncSelectionToUi();
				context->m_componentsWindow->SetObjects(render->GetObjects());
				context->m_componentsWindow->SetMapSelected(false);
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
			else if (droppedObjectType == "light")
			{
				object.SetName("Light");
				object.SetType(Blaze::ObjectType::Light);
				object.SetLightKind(Blaze::LightKind::Point);
				object.SetLightRadius(300.0f);
				object.SetLightIntensity(1.0f);
				object.SetColor(1.0f, 0.92f, 0.75f, 1.0f);
			}
			else if (droppedObjectType == "spotlight")
			{
				object.SetName("Spot Light");
				object.SetType(Blaze::ObjectType::Light);
				object.SetLightKind(Blaze::LightKind::Spot);
				object.SetLightRadius(400.0f);
				object.SetLightIntensity(1.2f);
				object.SetLightInnerAngle(20.0f);
				object.SetLightOuterAngle(35.0f);
				object.SetColor(1.0f, 0.92f, 0.75f, 1.0f);
			}
			else if (droppedObjectType == "postprocess")
			{
				object.SetName("Post Process");
				object.SetType(Blaze::ObjectType::PostProcess);
			}

			SetSoleSelection(&object);
			PushCreatedObject(object);
		}

		std::string droppedAsset;
		if (Blaze::StartWindow::IsPlaying())
		{
			Blaze::ContentBrowser::ConsumePendingDrop(droppedAsset);
		}
		else if (Blaze::ContentBrowser::ConsumePendingDrop(droppedAsset))
		{
			std::string ext = std::filesystem::path(droppedAsset).extension().string();
			for (auto& c : ext)
				c = (char)std::tolower((unsigned char)c);

			if (ext == ".prefab")
			{
				std::vector<Blaze::GameObject*> created;
				glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
				if (Blaze::GameObject* root = Blaze::PrefabAsset::Instantiate(
					droppedAsset, *render, vertices, indices, spawnPos, &created))
				{
					PushCreatedObjects(created);
					SetSoleSelection(root);
				}
			}
			else if (ext == ".particle")
			{
				render->CreateObject(vertices, indices);
				Blaze::GameObject& object = *render->GetObjects().back();
				glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
				object.SetPosition(spawnPos);
				object.SetName(std::filesystem::path(droppedAsset).stem().string());
				object.SetType(Blaze::ObjectType::Empty);
				object.AddParticleEmitter();
				object.SetParticlePath(droppedAsset);
				object.SetParticlePlayOnStart(true);
				SetSoleSelection(&object);
				PushCreatedObject(object);
			}
			else
			{
				render->CreateObject(vertices, indices);
				Blaze::GameObject& object = *render->GetObjects().back();
				glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
				object.SetPosition(spawnPos);

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
				SetSoleSelection(&object);
				PushCreatedObject(object);
			}
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
						for (Blaze::GameObject* object : m_Selection)
						{
							if (object && Blaze::IsObjectLive(object))
								object->Translate(delta);
						}
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
						for (Blaze::GameObject* object : m_Selection)
						{
							if (object && Blaze::IsObjectLive(object))
								object->Rotate(delta);
						}
						m_LastMouseAngle = angle;
						m_LastMouseWorld = world;
						break;
					}
					case Blaze::GizmoMode::Scale:
					{
						glm::vec2 world = GetMouseWorldPos();
						float dxPx = (world.x - m_LastMouseWorld.x) / sx;
						float dyPx = (world.y - m_LastMouseWorld.y) / sy;
						for (Blaze::GameObject* object : m_Selection)
						{
							if (!object || !Blaze::IsObjectLive(object))
								continue;
							glm::vec2 scale = object->GetScale();
							if (m_DragAxis == Blaze::GizmoAxis::X)
								scale.x += dxPx / 100.0f;
							else if (m_DragAxis == Blaze::GizmoAxis::Y)
								scale.y += dyPx / 100.0f;
							else
								scale += (dxPx + dyPx) * 0.5f / 100.0f;
							scale.x = std::max(scale.x, 0.05f);
							scale.y = std::max(scale.y, 0.05f);
							object->SetScale(scale);
						}
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

			if (overTile && CanInteractWithScene())
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
		float rawDt = std::chrono::duration<float>(now - m_LastTime).count();
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
					Blaze::StartWindow::SetPlaying(false);
					m_EditorSnapshot.clear();
					m_EditorMapPath.clear();
				}
				else
				{
					physics->Reset();
					Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
					Blaze::ParticleSystem2D::OnPlayModeStart(render->GetObjects());
					Blaze::EnemyAISystem::SetObjects(&render->GetObjects());
					Blaze::NavAgentSystem::SetObjects(&render->GetObjects());
				}
			}

			if (Blaze::StartWindow::IsPlaying())
			{
				Blaze::Time::Tick(rawDt);
				const float dt = Blaze::Time::GetDeltaTime();
				Blaze::InputActions::Tick();

				Blaze::ScriptEngine::OnUpdate(dt, render->GetObjects());
				FlushScriptDestroys();

				physics->Step(dt, render->GetObjects());
				Blaze::ScriptEngine::OnCollisionEvents(physics->ConsumeCollisionEvents());
				FlushScriptDestroys();
				physics->ApplyPlayTransforms(render->GetObjects());
				Blaze::ScriptEngine::UpdateCameraFollow(dt);
				Blaze::ScriptEngine::CapturePlayTransforms(render->GetObjects());
				render->Update(dt);
				RenderGameView();
				RestoreEditorSnapshot();
			}
		}
		else if (m_WasPlaying)
		{
			physics->Reset();
			Blaze::AudioEngine::OnPlayModeStop();
			Blaze::ParticleSystem2D::OnPlayModeStop();
			Blaze::EnemyAISystem::Clear();
			Blaze::NavAgentSystem::Clear();
			Blaze::ScriptEngine::OnPlayModeStop();
			Blaze::UISystem::HideAll();
			ClearSelection();

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
		{
			Blaze::EnemyAISystem::SetObjects(&render->GetObjects());
			Blaze::NavAgentSystem::SetObjects(&render->GetObjects());
			render->Update(rawDt);
			Blaze::ParticleSystem2D::UpdateSceneEmitters(rawDt);
			Blaze::ParticleSystem2D::Update(rawDt);
			Blaze::EnemyAISystem::Update(rawDt);
			Blaze::NavAgentSystem::Update(rawDt);

			if (Blaze::GameObject* listener = FindCameraObject())
			{
				const glm::vec3 pos = listener->GetPosition();
				Blaze::AudioEngine::SetListenerPosition(pos.x, pos.y);
			}
			else if (camera)
			{
				Blaze::AudioEngine::SetListenerPosition(camera->GetCameraProp().pos.x, camera->GetCameraProp().pos.y);
			}
			Blaze::AudioEngine::Update();
		}

		auto& window = Blaze::Application::Get().GetWindow();
		const int winW = (int)window.GetWidth();
		const int winH = (int)window.GetHeight();

		const bool sceneOpen = Blaze::Application::Get().GetContext()
			&& Blaze::Application::Get().GetContext()->m_sceneWindow
			&& Blaze::Application::Get().GetContext()->m_sceneWindow->IsOpen();

		if (sceneOpen)
		{
			Blaze::SceneWindow::EnsureSize(
				Blaze::SceneWindow::GetDesiredWidth(),
				Blaze::SceneWindow::GetDesiredHeight());
			SyncEditorCameraToSceneSize(
				Blaze::SceneWindow::GetFrameWidth(),
				Blaze::SceneWindow::GetFrameHeight());
			Blaze::SceneWindow::BeginCapture();
		}

		const bool gradeEditor = Blaze::PostProcess2D::FindActive(render->GetObjects()) != nullptr;
		if (!sceneOpen && gradeEditor)
			Blaze::PostProcess2D::BeginSceneCapture(winW, winH);

		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();

		Blaze::ParticleSystem2D::Flush(*camera, *gizmo);

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
			if (object->HasParticleEmitter())
				gizmo->DrawParticleEmitterIcon(*camera, *object);
			if (object->IsLight())
				gizmo->DrawLightGizmo(*camera, *object);
			if (object->HasCollision() && !object->IsSound() && !object->IsCamera() && !object->IsLight()
				&& !object->IsPostProcess()
				&& !object->HasParticleEmitter())
				gizmo->DrawCollision(*camera, *object);
		}

		if (m_Selected)
			gizmo->Draw(*camera, *m_Selected);

		for (Blaze::GameObject* object : m_Selection)
		{
			if (!object || object == m_Selected || !Blaze::IsObjectLive(object))
				continue;
			gizmo->DrawSelection(*camera, *object);
		}
		if (m_Selected)
			gizmo->DrawSelection(*camera, *m_Selected);

		if (m_BoxSelecting)
		{
			m_BoxEndWorld = GetMouseWorldPos();
			if (glm::length(m_BoxEndWorld - m_BoxStartWorld) > 0.02f)
				m_BoxMoved = true;
			const glm::vec4 boxColor(0.25f, 0.65f, 1.0f, 0.9f);
			gizmo->DrawDebugLine(*camera, m_BoxStartWorld, glm::vec2(m_BoxEndWorld.x, m_BoxStartWorld.y), boxColor, 2.0f);
			gizmo->DrawDebugLine(*camera, glm::vec2(m_BoxEndWorld.x, m_BoxStartWorld.y), m_BoxEndWorld, boxColor, 2.0f);
			gizmo->DrawDebugLine(*camera, m_BoxEndWorld, glm::vec2(m_BoxStartWorld.x, m_BoxEndWorld.y), boxColor, 2.0f);
			gizmo->DrawDebugLine(*camera, glm::vec2(m_BoxStartWorld.x, m_BoxEndWorld.y), m_BoxStartWorld, boxColor, 2.0f);
		}

		if (m_Selected && m_Selected->IsTilemap())
			gizmo->DrawTilemapGrid(*camera, *m_Selected, m_HoverTileX, m_HoverTileY);

		if (sceneOpen)
		{
			if (gradeEditor)
			{
				Blaze::PostProcess2D::Apply(render->GetObjects(),
					Blaze::SceneWindow::GetColorTexture(),
					Blaze::SceneWindow::GetFramebuffer(),
					Blaze::SceneWindow::GetFrameWidth(),
					Blaze::SceneWindow::GetFrameHeight());
			}
			Blaze::SceneWindow::EndCapture(winW, winH);
		}
		else if (gradeEditor)
		{
			Blaze::PostProcess2D::EndSceneCapturePresent(render->GetObjects(), winW, winH);
		}
	}

	void OnEvent(Blaze::Event& event) override
	{
		static float height = 900.f, width = 1600.f;
		if (event.GetEventType() == Blaze::EventType::WindowResize)
		{
			auto& e = (Blaze::WindowResizeEvent&)event;
			if (e.GetWidth() == 0 || e.GetHeight() == 0)
				return;

			const bool sceneOpen = Blaze::Application::Get().GetContext()
				&& Blaze::Application::Get().GetContext()->m_sceneWindow
				&& Blaze::Application::Get().GetContext()->m_sceneWindow->IsOpen();

			if (!sceneOpen)
			{
				if (width > 0.0f && height > 0.0f)
				{
					camera->zoom *= (float)e.GetWidth() / width;
					camera->zoom *= (float)e.GetHeight() / height;
				}

				Blaze::Camera::m_prop.projection = glm::ortho(
					(float)(-(int)e.GetWidth() / 2), (float)((int)e.GetWidth() / 2),
					(float)(-(int)e.GetHeight() / 2), (float)((int)e.GetHeight() / 2),
					-100.f, 100.f);
				m_SceneCamW = 0;
				m_SceneCamH = 0;
			}

			width = (float)e.GetWidth();
			height = (float)e.GetHeight();
		}

		if (!Blaze::Project::HasProject())
			return;

		if (event.GetEventType() == Blaze::EventType::MouseScrolled)
		{
			if (!Blaze::StartWindow::IsPlaying() && !Blaze::UIBlocksMainEditor()
				&& Blaze::SceneWindow::IsMouseOverSceneView() && camera)
			{
				const bool ctrl = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_CONTROL) ||
					Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_CONTROL);
				if (ctrl)
				{
					auto& e = (Blaze::MouseScrolledEvent&)event;
					float next = Blaze::Camera::zoom + e.GetYOffset() * 0.1f;
					if (!std::isfinite(next))
						next = 1.0f;
					next = std::max(0.05f, std::min(next, 20.0f));
					Blaze::Camera::zoom = next;
					Blaze::Camera::ScaleCamera();
					return;
				}
			}
		}

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
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT && CanInteractWithScene())
			{
				glm::vec2 world = GetMouseWorldPos();
				auto prop = camera->GetCameraProp();

				auto& objects = render->GetObjects();

				if (m_Selected)
				{
					if (m_Selected->IsTilemap())
					{
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

						if (otherFound &&
							!(Blaze::ComponentsWindow::IsTilePaintMode() && overTile))
						{
							const bool ctrl = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_CONTROL) ||
								Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_CONTROL);
							if (ctrl)
								ToggleSelection(otherUnderCursor);
							else
								SetSoleSelection(otherUnderCursor);
							if (auto* context = Blaze::Application::Get().GetContext())
								context->m_objectWindow->SetMapSelected(false);

							const auto now = std::chrono::steady_clock::now();
							const double elapsed = std::chrono::duration<double>(now - m_LastViewportClickTime).count();
							if (m_Selected == m_LastViewportClickObject && elapsed < 0.35)
							{
								FocusEditorOnSelection();
								m_LastViewportClickObject = nullptr;
								m_LastViewportClickTime = {};
							}
							else
							{
								m_LastViewportClickObject = m_Selected;
								m_LastViewportClickTime = now;
							}

							if (m_Selected && gizmo->GetMode() == Blaze::GizmoMode::Translate
								&& !m_Selected->IsTilemap() && !ctrl)
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

				const bool ctrl = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_CONTROL) ||
					Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_CONTROL);

				Blaze::GameObject* hit = nullptr;
				int bestLayer = 0;
				bool found = false;
				for (auto it = objects.rbegin(); it != objects.rend(); ++it)
				{
					if (!(*it)->IsHovered(prop, world))
						continue;
					const int layer = (*it)->GetLayer();
					if (!found || layer > bestLayer)
					{
						hit = *it;
						bestLayer = layer;
						found = true;
					}
				}

				if (found && hit)
				{
					if (ctrl)
						ToggleSelection(hit);
					else
						SetSoleSelection(hit);

					if (auto* context = Blaze::Application::Get().GetContext())
						context->m_objectWindow->SetMapSelected(false);

					const auto now = std::chrono::steady_clock::now();
					const double elapsed = std::chrono::duration<double>(now - m_LastViewportClickTime).count();
					if (m_Selected == m_LastViewportClickObject && elapsed < 0.35)
					{
						FocusEditorOnSelection();
						m_LastViewportClickObject = nullptr;
						m_LastViewportClickTime = {};
					}
					else
					{
						m_LastViewportClickObject = m_Selected;
						m_LastViewportClickTime = now;
					}

					if (m_Selected && gizmo->GetMode() == Blaze::GizmoMode::Translate
						&& !m_Selected->IsTilemap() && !ctrl)
					{
						m_DragAxis = Blaze::GizmoAxis::XY;
						m_LastMouseWorld = world;
						BeginGizmoDrag();
					}
				}
				else
				{
					if (!ctrl)
						ClearSelection();
					m_BoxSelecting = true;
					m_BoxMoved = false;
					m_BoxStartWorld = world;
					m_BoxEndWorld = world;
					m_BoxAdditive = ctrl;
				}
			}
		}

		if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
		{
			auto& e = (Blaze::MouseButtonReleasedEvent&)event;
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT)
			{
				if (m_BoxSelecting)
				{
					m_BoxEndWorld = GetMouseWorldPos();
					const glm::vec2 d = m_BoxEndWorld - m_BoxStartWorld;
					const float drag = glm::length(d);
					if (drag > 0.02f || m_BoxMoved)
					{
						auto prop = camera->GetCameraProp();
						const float minX = std::min(m_BoxStartWorld.x, m_BoxEndWorld.x);
						const float maxX = std::max(m_BoxStartWorld.x, m_BoxEndWorld.x);
						const float minY = std::min(m_BoxStartWorld.y, m_BoxEndWorld.y);
						const float maxY = std::max(m_BoxStartWorld.y, m_BoxEndWorld.y);
						if (!m_BoxAdditive)
							ClearSelection();
						for (Blaze::GameObject* object : render->GetObjects())
						{
							if (!object || !object->IsVisible())
								continue;
							glm::vec2 a, b;
							object->GetWorldAABB(prop, a, b);
							if (a.x > maxX || b.x < minX || a.y > maxY || b.y < minY)
								continue;
							AddToSelection(object);
						}
						if (auto* context = Blaze::Application::Get().GetContext())
						{
							if (!m_Selection.empty())
								context->m_objectWindow->SetMapSelected(false);
						}
					}
					m_BoxSelecting = false;
					m_BoxMoved = false;
				}

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

			if (e.GetKeyCode() == BZ_KEY_F && !ctrl && !m_Selection.empty())
			{
				FocusEditorOnSelection();
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

			if (ctrl && e.GetKeyCode() == BZ_KEY_D && e.GetRepeatCount() == 0)
			{
				DuplicateSelection();
				return;
			}
			if (ctrl && e.GetKeyCode() == BZ_KEY_C && e.GetRepeatCount() == 0)
			{
				CopySelection();
				return;
			}
			if (ctrl && e.GetKeyCode() == BZ_KEY_V && e.GetRepeatCount() == 0)
			{
				PasteClipboard();
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

			if (e.GetKeyCode() == BZ_KEY_DELETE && !m_Selection.empty())
			{
				DeleteSelectionWithUndo();
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
	std::vector<Blaze::GameObject*> m_Selection;
	std::string m_Clipboard;
	Blaze::GizmoAxis m_DragAxis = Blaze::GizmoAxis::None;
	int m_SceneCamW = 0;
	int m_SceneCamH = 0;
	int m_HoverTileX = -1;
	int m_HoverTileY = -1;
	bool m_TileStrokeActive = false;
	bool m_TileEraseStrokeActive = false;
	bool m_GizmoDragTracked = false;
	bool m_BoxSelecting = false;
	bool m_BoxMoved = false;
	bool m_BoxAdditive = false;
	glm::vec2 m_BoxStartWorld = glm::vec2(0.0f);
	glm::vec2 m_BoxEndWorld = glm::vec2(0.0f);
	struct GizmoUndoEntry
	{
		Blaze::GameObject* object = nullptr;
		Blaze::LocalTransformSnap snap;
	};
	std::vector<GizmoUndoEntry> m_GizmoUndoBefores;
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
		"out vec2 WorldPos;\n"
		"void main()\n"
		"{\n"
		"   vec4 local = ortho * vec4(pos, 1.0);\n"
		"   vec4 world = object_matrix * local;\n"
		"   // Same space as GetWorldPosition / gizmo rings (post-ortho).\n"
		"   WorldPos = world.xy;\n"
		"   gl_Position = matrix * world;\n"
		"   TexCoord = texCoord * uv_scale + uv_offset;\n"
		"}\0";

	const char* fragmentShaderSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"uniform mat4 ortho;\n"
		"uniform int u_LightingEnabled;\n"
		"uniform vec3 u_Ambient;\n"
		"uniform int u_LightCount;\n"
		"uniform vec2 u_LightPos[16];\n"
		"uniform vec3 u_LightColor[16];\n"
		"uniform float u_LightRadius[16];\n"
		"uniform float u_LightIntensity[16];\n"
		"uniform float u_LightType[16];\n"
		"uniform vec2 u_LightDir[16];\n"
		"uniform float u_LightInnerCos[16];\n"
		"uniform float u_LightOuterCos[16];\n"
		"out vec4 FragColor;\n"
		"in vec2 TexCoord;\n"
		"in vec2 WorldPos;\n"
		"uniform sampler2D in_texture;\n"
		"void main()\n"
		"{\n"
		"   vec4 albedo = texture(in_texture, TexCoord) * color;\n"
		"   if (u_LightingEnabled == 0) {\n"
		"      FragColor = albedo;\n"
		"      return;\n"
		"   }\n"
		"   // Radius is in mesh/pixel units (same as collision / light gizmo).\n"
		"   vec2 scale = vec2(max(abs(ortho[0][0]), 1e-8), max(abs(ortho[1][1]), 1e-8));\n"
		"   vec3 lighting = u_Ambient;\n"
		"   int count = clamp(u_LightCount, 0, 16);\n"
		"   for (int i = 0; i < 16; ++i) {\n"
		"      if (i >= count) break;\n"
		"      vec2 deltaPx = (WorldPos - u_LightPos[i]) / scale;\n"
		"      float dist = length(deltaPx);\n"
		"      float r = max(u_LightRadius[i], 1.0);\n"
		"      float t = clamp(1.0 - dist / r, 0.0, 1.0);\n"
		"      float atten = t * t * u_LightIntensity[i];\n"
		"      if (u_LightType[i] > 0.5) {\n"
		"         vec2 toFrag = (dist > 1e-4) ? (deltaPx / dist) : vec2(0.0);\n"
		"         float cosTheta = dot(toFrag, normalize(u_LightDir[i]));\n"
		"         float spot = smoothstep(u_LightOuterCos[i], u_LightInnerCos[i], cosTheta);\n"
		"         atten *= spot;\n"
		"      }\n"
		"      lighting += u_LightColor[i] * atten;\n"
		"   }\n"
		"   FragColor = vec4(albedo.rgb * lighting, albedo.a);\n"
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