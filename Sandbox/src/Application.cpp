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

		auto& window = Blaze::Application::Get().GetWindow();
		Blaze::GameWindow::EndCapture((int)window.GetWidth(), (int)window.GetHeight());
	}

	~ExampleLayer()
	{
		Blaze::AudioEngine::Shutdown();
		Blaze::ScriptEngine::Shutdown();
		delete physics;
		delete gizmo;
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
		if (Blaze::ObjectWindow* objects = GetObjectsWindow())
			objects->ClearHierarchy();
		while (!render->GetObjects().empty())
			render->DestroyObject(render->GetObjects().back());
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
		if (!Blaze::Project::LoadMap(path, *render, vertices, indices, GetObjectsWindow()))
			return;

		if (CountCameraObjects() == 0)
			CreateDefaultCamera();

		ApplyProjectToBrowser();
		BZ_INFO("Opened map: {0}", path);
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
			EditorPose pose;
			pose.object = object;
			pose.position = object->GetPosition();
			pose.rotation = object->GetRotation();
			pose.scale = object->GetScale();
			m_EditorSnapshot.push_back(pose);
		}
	}

	void RestoreEditorSnapshot()
	{
		for (const EditorPose& pose : m_EditorSnapshot)
		{
			if (!pose.object)
				continue;
			glm::vec3 position = pose.position;
			pose.object->SetPosition(position);
			pose.object->SetRotation(pose.rotation);
			pose.object->SetScale(pose.scale);
		}
	}

	void RefreshEditorSnapshotFromScene()
	{
		m_EditorSnapshot.clear();
		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (!object)
				continue;
			EditorPose pose;
			pose.object = object;
			pose.position = object->GetPosition();
			pose.rotation = object->GetRotation();
			pose.scale = object->GetScale();
			m_EditorSnapshot.push_back(pose);
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

		if (!Blaze::UIBlocksMainEditor())
			camera->CameraMove();
		HandleFileMenu();

		std::string openMapPath;
		if (Blaze::ContentBrowser::ConsumePendingOpenMap(openMapPath))
			OpenMapFile(openMapPath);
		else if (Blaze::ProjectSettingsWindow::ConsumePendingOpenMap(openMapPath))
			OpenMapFile(openMapPath);
		else if (Blaze::ScriptEngine::ConsumePendingOpenMap(openMapPath))
		{
			OpenMapFile(openMapPath);
			m_WasPlaying = false;
			Blaze::StartWindow::SetPlaying(true);
		}

		if (Blaze::UIBlocksMainEditor())
			m_DragAxis = Blaze::GizmoAxis::None;

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
					if (m_Selected == toDelete)
						m_Selected = nullptr;
					m_DragAxis = Blaze::GizmoAxis::None;
					render->DestroyObject(toDelete);
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
		if (Blaze::PropertiesWindow::ConsumePendingDrop(droppedObjectType))
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
		}

		std::string droppedAsset;
		if (Blaze::ContentBrowser::ConsumePendingDrop(droppedAsset))
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
		}

		if (m_DragAxis != Blaze::GizmoAxis::None && !Blaze::UIBlocksMainEditor())
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
				m_DragAxis = Blaze::GizmoAxis::None;
			}
		}

		const bool playing = Blaze::StartWindow::IsPlaying();
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;

		if (playing)
		{
			if (!m_WasPlaying)
			{
				CaptureEditorSnapshot();
				Blaze::ScriptEngine::SetRenderer(render);
				Blaze::ScriptEngine::SetPhysicsWorld(physics);
				if (!Blaze::ScriptEngine::OnPlayModeStart(render->GetObjects()))
				{
					// Syntax/compile error — abort play before physics/audio/scripts run
					Blaze::StartWindow::SetPlaying(false);
					m_EditorSnapshot.clear();
				}
				else
				{
					physics->Reset();
					Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
				}
			}

			if (Blaze::StartWindow::IsPlaying())
			{
				if (m_WasPlaying)
					RefreshEditorSnapshotFromScene();

				Blaze::ScriptEngine::OnUpdate(dt, render->GetObjects());
				FlushScriptDestroys();
				physics->Step(dt, render->GetObjects());
				Blaze::ScriptEngine::OnCollisionEvents(physics->ConsumeCollisionEvents());
				FlushScriptDestroys();
				physics->ApplyPlayTransforms(render->GetObjects());
				render->Update(dt);
				RenderGameView();
				RestoreEditorSnapshot();
			}
		}
		else if (m_WasPlaying)
		{
			RestoreEditorSnapshot();
			physics->Reset();
			Blaze::AudioEngine::OnPlayModeStop();
			Blaze::ScriptEngine::OnPlayModeStop();
			m_EditorSnapshot.clear();
		}
		m_WasPlaying = Blaze::StartWindow::IsPlaying();

		if (!Blaze::StartWindow::IsPlaying())
			render->Update(dt);

		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();

		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (object && object->IsCamera())
			{
				gizmo->DrawCameraIcon(*camera, *object);
				if (object->DrivesView())
					gizmo->DrawCameraRange(*camera, *object);
			}
			if (object && object->IsSound())
				gizmo->DrawSoundIcon(*camera, *object);
			if (object && object->HasCollision() && !object->IsSound() && !object->IsCamera())
				gizmo->DrawCollision(*camera, *object);
		}

		if (m_Selected)
			gizmo->Draw(*camera, *m_Selected);
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

		// Script editor / text fields own input — ignore scene shortcuts & picking.
		if (Blaze::UIBlocksMainEditor())
		{
			if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
				m_DragAxis = Blaze::GizmoAxis::None;
			return;
		}

		if (event.GetEventType() == Blaze::EventType::MouseButtonPressed)
		{
			auto& e = (Blaze::MouseButtonPressedEvent&)event;
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT && !Blaze::UIWantCaptureMouse())
			{
				glm::vec2 world = GetMouseWorldPos();
				auto prop = camera->GetCameraProp();

				if (m_Selected)
				{
					Blaze::GizmoAxis axis = gizmo->HitTest(prop, *m_Selected, world);
					if (axis != Blaze::GizmoAxis::None)
					{
						m_DragAxis = axis;
						m_LastMouseWorld = world;
						m_LastMouseAngle = GetMouseAngle(world);
						return;
					}
				}

				m_Selected = nullptr;
				bool found = false;
				int bestLayer = 0;
				auto& objects = render->GetObjects();
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

				if (m_Selected && gizmo->GetMode() == Blaze::GizmoMode::Translate)
				{
					m_DragAxis = Blaze::GizmoAxis::XY;
					m_LastMouseWorld = world;
				}
			}
		}

		if (event.GetEventType() == Blaze::EventType::MouseButtonReleased)
		{
			auto& e = (Blaze::MouseButtonReleasedEvent&)event;
			if (e.GetMouseButton() == BZ_MOUSE_BUTTON_LEFT)
				m_DragAxis = Blaze::GizmoAxis::None;
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			const bool ctrl = Blaze::Input::IsKeyPressed(BZ_KEY_LEFT_CONTROL) ||
				Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT_CONTROL);

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

			if (e.GetKeyCode() == BZ_KEY_R)
				gizmo->SetMode(Blaze::GizmoMode::Translate);
			if (e.GetKeyCode() == BZ_KEY_T)
				gizmo->SetMode(Blaze::GizmoMode::Rotate);
			if (e.GetKeyCode() == BZ_KEY_Y)
				gizmo->SetMode(Blaze::GizmoMode::Scale);

			if (e.GetKeyCode() == BZ_KEY_DELETE && m_Selected)
			{
				if (m_Selected->IsCamera() && CountCameraObjects() <= 1)
				{
					BZ_INFO("Cannot delete the last camera");
				}
				else
				{
					render->DestroyObject(m_Selected);
					m_Selected = nullptr;
					m_DragAxis = Blaze::GizmoAxis::None;
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
	glm::vec2 m_LastMouseWorld = glm::vec2(0.0f);
	float m_LastMouseAngle = 0.0f;
	std::chrono::steady_clock::time_point m_LastTime;
	bool m_WasPlaying = false;
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