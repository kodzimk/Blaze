#include <Blaze.h>
#include<glad/glad.h>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer(vertexShaderSource,fragmentShaderSource);
		camera = Blaze::Camera::CreateCamera(glm::vec3(0.0f,0.0f,0.0f));
		gizmo = new Blaze::Gizmo2D();
	}


	void RenderGameView()
	{
		Blaze::GameObject* cameraObject = FindCameraObject();
		if (!cameraObject)
			return;

		Blaze::GameWindow::BeginCapture();

		float zoom = cameraObject->GetZoom();
		glm::mat4 gameView(1.0f);
		gameView = glm::scale(gameView, glm::vec3(zoom, zoom, 1.0f));
		gameView = glm::translate(gameView, -cameraObject->GetPosition());

		render->BeginScene(gameView, "matrix");
		render->Render(*camera);
		render->EndScene();

		auto& window = Blaze::Application::Get().GetWindow();
		Blaze::GameWindow::EndCapture((int)window.GetWidth(), (int)window.GetHeight());
	}

	~ExampleLayer()
	{
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

	void ClearScene()
	{
		m_Selected = nullptr;
		m_DragAxis = Blaze::GizmoAxis::None;
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

	void HandleHubActions()
	{
		switch (Blaze::ProjectHub::ConsumeAction())
		{
			case Blaze::HubAction::CreateProject:
			{
				ClearScene();
				CreateDefaultCamera();
				Blaze::Project::SaveScene(*render);
				ApplyProjectToBrowser();
				break;
			}
			case Blaze::HubAction::LoadProject:
			{
				m_Selected = nullptr;
				m_DragAxis = Blaze::GizmoAxis::None;
				Blaze::Project::LoadScene(*render, vertices, indices);
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
					Blaze::Project::SaveScene(*render);
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
					if (Blaze::Project::LoadScene(*render, vertices, indices))
						ApplyProjectToBrowser();
				}
				break;
			}
			case Blaze::FileAction::Save:
			{
				if (Blaze::Project::SaveScene(*render))
					ApplyProjectToBrowser();
				break;
			}
			case Blaze::FileAction::SaveAs:
			{
				if (Blaze::Project::SaveSceneAs(*render))
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
			default:
				break;
		}
	}

	void OnUpdate() override
	{
		HandleHubActions();

		if (!Blaze::Project::HasProject())
			return;

		camera->CameraMove();
		HandleFileMenu();

		if (auto* context = Blaze::Application::Get().GetContext())
		{
			if (Blaze::GameObject* picked = context->m_objectWindow->ConsumePickedObject())
				m_Selected = picked;

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
			context->m_objectWindow->SetSelectedObject(m_Selected);
			context->m_componentsWindow->SetSelectedObject(m_Selected);
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
			else if (droppedObjectType == "empty")
			{
				object.SetType(Blaze::ObjectType::Empty);
			}
			else if (droppedObjectType == "camera")
			{
				object.SetName("Camera");
				object.SetType(Blaze::ObjectType::Camera);
				if (!FindCameraObject() || !FindCameraObject()->DrivesView())
					object.SetDrivesView(true);
			}

			m_Selected = &object;
		}

		std::string droppedTexture;
		if (Blaze::ContentBrowser::ConsumePendingDrop(droppedTexture))
		{
			render->CreateObject(vertices, indices);
			Blaze::GameObject& object = *render->GetObjects().back();
			object.SetTexture(droppedTexture);
			glm::vec3 spawnPos(GetMouseWorldPos(), 0.0f);
			object.SetPosition(spawnPos);
			m_Selected = &object;
		}
		if (m_DragAxis != Blaze::GizmoAxis::None)
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

		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();

		for (Blaze::GameObject* object : render->GetObjects())
		{
			if (object && object->IsCamera() && object->DrivesView())
				gizmo->DrawCameraRange(*camera, *object);
		}

		if (m_Selected)
			gizmo->Draw(*camera, *m_Selected);

		if (Blaze::StartWindow::IsPlaying())
			RenderGameView();
	}

	void OnEvent(Blaze::Event& event) override
	{
		static float height = 900.f, width = 1600.f;
		if (event.GetEventType() == Blaze::EventType::WindowResize)
		{
			auto& e = (Blaze::WindowResizeEvent&)event;
			camera->zoom *= e.GetWidth() / width;
			camera->zoom *= e.GetHeight() / height;

			Blaze::Camera::m_prop.projection = glm::ortho((float)(-e.GetWidth() / 2), (float)(e.GetWidth() / 2), (float)(-e.GetHeight() / 2), (float)(e.GetHeight() / 2), -100.f, 100.f);

			width = e.GetWidth();
			height = e.GetHeight();
		}

		if (!Blaze::Project::HasProject())
			return;

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
				auto& objects = render->GetObjects();
				for (auto it = objects.rbegin(); it != objects.rend(); ++it)
				{
					if ((*it)->IsHovered(prop, world))
					{
						m_Selected = *it;
						break;
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

			if (e.GetKeyCode() == BZ_KEY_S && ctrl && !Blaze::UIWantTextInput())
			{
				if (Blaze::Project::SaveScene(*render))
					ApplyProjectToBrowser();
				return;
			}

			if (e.GetKeyCode() == BZ_KEY_SPACE)
			{
				render->GetGameObject("Object3").SetTexture("content/texture/wall.jpg");
			}

			if (e.GetKeyCode() == BZ_KEY_R)
				gizmo->SetMode(Blaze::GizmoMode::Translate);
			if (e.GetKeyCode() == BZ_KEY_T)
				gizmo->SetMode(Blaze::GizmoMode::Rotate);
			if (e.GetKeyCode() == BZ_KEY_Y)
				gizmo->SetMode(Blaze::GizmoMode::Scale);

			if (e.GetKeyCode() == BZ_KEY_DELETE && m_Selected && !Blaze::UIWantTextInput())
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

	Blaze::GameObject* m_Selected = nullptr;
	Blaze::GizmoAxis m_DragAxis = Blaze::GizmoAxis::None;
	glm::vec2 m_LastMouseWorld = glm::vec2(0.0f);
	float m_LastMouseAngle = 0.0f;

	const char* vertexShaderSource = "#version 330 core\n"
		"layout (location = 0) in vec3 pos;\n"
		"layout (location = 1) in vec2 texCoord;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 ortho;\n"
		"out vec2 TexCoord;\n"
		"uniform mat4 object_matrix;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(matrix * object_matrix * ortho * vec4(pos,1.0f));\n"
		"   TexCoord = texCoord;\n"
		"}\0";

	const char* fragmentShaderSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 FragColor;\n"
		"in vec2 TexCoord;\n"
		"uniform sampler2D in_texture;\n"
		"void main()\n"
		"{\n"
		"   FragColor = texture(in_texture,TexCoord);\n"
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