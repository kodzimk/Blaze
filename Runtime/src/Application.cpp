#include <Blaze.h>
#include <glad/glad.h>
#include <chrono>
#include <filesystem>
#include <windows.h>

class GameLayer : public Blaze::Layer
{
public:
	GameLayer()
		: Layer("Game")
	{
		render = new Blaze::Renderer(vertexShaderSource, fragmentShaderSource);
		camera = Blaze::Camera::CreateCamera(glm::vec3(0.0f, 0.0f, 0.0f));
		physics = new Blaze::PhysicsWorld2D();
		m_LastTime = std::chrono::steady_clock::now();
		BootGame();
	}

	~GameLayer() override
	{
		Blaze::UISystem::HideAll();
		Blaze::UISystem::Shutdown();
		Blaze::AudioEngine::Shutdown();
		Blaze::ScriptEngine::Shutdown();
		delete physics;
		delete render;
		delete camera;
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

	void BootGame()
	{
		char modulePath[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
		const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();

		std::error_code ec;
		std::filesystem::current_path(exeDir, ec);

		const std::filesystem::path projectFile = exeDir / "game.blaze";
		if (!std::filesystem::exists(projectFile, ec))
		{
			BZ_ERROR("game.blaze not found next to the executable");
			return;
		}

		Blaze::Project::SetProjectFromFile(projectFile.string());
		if (!Blaze::Project::LoadScene(*render, vertices, indices, nullptr))
		{
			BZ_ERROR("Failed to load starting map");
			return;
		}

		Blaze::Application::Get().GetWindow().SetTitle(Blaze::Project::GetGameName());

		if (!Blaze::UISystem::IsReady())
			Blaze::UISystem::Init();

		Blaze::ScriptEngine::SetRenderer(render);
		Blaze::ScriptEngine::SetPhysicsWorld(physics);
		if (!Blaze::ScriptEngine::OnPlayModeStart(render->GetObjects()))
		{
			BZ_ERROR("Failed to start scripts");
			return;
		}

		physics->Reset();
		Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
		m_Running = true;
		BZ_INFO("Game started: {0}", Blaze::Project::GetGameName());
	}

	void SwitchMap(const std::string& path)
	{
		Blaze::AudioEngine::OnPlayModeStop();
		Blaze::ScriptEngine::OnPlayModeStop();
		physics->Reset();
		// LoadMap also HideAll() widgets from the previous map.

		if (!Blaze::Project::LoadMap(path, *render, vertices, indices, nullptr))
		{
			BZ_ERROR("Failed to open map: {0}", path);
			m_Running = false;
			return;
		}

		Blaze::ScriptEngine::SetRenderer(render);
		Blaze::ScriptEngine::SetPhysicsWorld(physics);
		if (!Blaze::ScriptEngine::OnPlayModeStart(render->GetObjects()))
		{
			BZ_ERROR("Failed to start scripts for map: {0}", path);
			m_Running = false;
			return;
		}

		physics->Reset();
		Blaze::AudioEngine::OnPlayModeStart(render->GetObjects());
		BZ_INFO("Switched map: {0}", path);
	}

	void FlushScriptDestroys()
	{
		std::vector<Blaze::GameObject*> doomed = Blaze::ScriptEngine::ConsumePendingDestroys();
		for (Blaze::GameObject* object : doomed)
		{
			if (!object)
				continue;
			physics->RemoveObject(object);
			render->DestroyObject(object);
		}
	}

	void UpdateWidgetInput(int width, int height)
	{
		if (!Blaze::UISystem::IsReady() || width <= 0 || height <= 0)
			return;

		const float mx = Blaze::Input::GetMouseX();
		const float my = Blaze::Input::GetMouseY();
		const bool mouseDown = Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT);
		const bool mousePressed = mouseDown && !m_WasMouseDown;
		const bool mouseReleased = !mouseDown && m_WasMouseDown;
		m_WasMouseDown = mouseDown;

		Blaze::UISystem::UpdateInput(width, height, mx, my, mouseDown, mousePressed, mouseReleased);
	}

	void OnUpdate() override
	{
		if (!m_Running)
			return;

		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;
		if (dt > 0.1f)
			dt = 0.1f;

		auto& window = Blaze::Application::Get().GetWindow();
		const int width = (int)window.GetWidth();
		const int height = (int)window.GetHeight();

		UpdateWidgetInput(width, height);

		Blaze::ScriptEngine::OnUpdate(dt, render->GetObjects());
		FlushScriptDestroys();

		std::string openMapPath;
		if (Blaze::ScriptEngine::ConsumePendingOpenMap(openMapPath))
		{
			SwitchMap(openMapPath);
			return;
		}

		physics->Step(dt, render->GetObjects());
		Blaze::ScriptEngine::OnCollisionEvents(physics->ConsumeCollisionEvents());
		FlushScriptDestroys();
		physics->ApplyPlayTransforms(render->GetObjects());
		render->Update(dt);

		Blaze::GameObject* cameraObject = FindCameraObject();
		if (cameraObject)
		{
			render->BeginScene(cameraObject->GetCameraViewMatrix(), "matrix");
		}
		else
		{
			render->BeginScene(*camera, "matrix");
		}

		render->Render(*camera);
		render->EndScene();

		if (!Blaze::UISystem::IsReady())
			Blaze::UISystem::Init();
		Blaze::UISystem::Draw(width, height);
	}

	void OnEvent(Blaze::Event& event) override
	{
		if (event.GetEventType() != Blaze::EventType::WindowResize)
			return;

		auto& e = (Blaze::WindowResizeEvent&)event;
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
			return;

		Blaze::Camera::m_prop.projection = glm::ortho(
			(float)(-(int)e.GetWidth() / 2), (float)((int)e.GetWidth() / 2),
			(float)(-(int)e.GetHeight() / 2), (float)((int)e.GetHeight() / 2),
			-100.f, 100.f);
	}

private:
	std::vector<float> vertices = {
		 50.f,  50.f, 0.0f, 1.0f, 1.0f,
		 50.f, -50.f, 0.0f, 1.0f, 0.0f,
		-50.f, -50.f, 0.0f, 0.0f, 0.0f,
		-50.f,  50.f, 0.0f, 0.0f, 1.0f,
	};

	std::vector<unsigned int> indices = {
		0, 1, 3,
		1, 2, 3
	};

	Blaze::Renderer* render = nullptr;
	Blaze::Camera* camera = nullptr;
	Blaze::PhysicsWorld2D* physics = nullptr;
	std::chrono::steady_clock::time_point m_LastTime;
	bool m_Running = false;
	bool m_WasMouseDown = false;

	const char* vertexShaderSource =
		"#version 330 core\n"
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

	const char* fragmentShaderSource =
		"#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 FragColor;\n"
		"in vec2 TexCoord;\n"
		"uniform sampler2D in_texture;\n"
		"void main()\n"
		"{\n"
		"   FragColor = texture(in_texture,TexCoord) * color;\n"
		"}\n\0";
};

class GameApp : public Blaze::Application
{
public:
	GameApp()
		: Application(false)
	{
		PushLayer(new GameLayer());
	}
};

Blaze::Application* Blaze::CreateApplication()
{
	return new GameApp();
}
