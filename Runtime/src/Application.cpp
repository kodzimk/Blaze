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
		Blaze::PostProcess2D::Shutdown();
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
		Blaze::ParticleSystem2D::OnPlayModeStart(render->GetObjects());
		m_Running = true;
		BZ_INFO("Game started: {0}", Blaze::Project::GetGameName());
	}

	void SwitchMap(const std::string& path)
	{
		Blaze::AudioEngine::OnPlayModeStop();
		Blaze::ParticleSystem2D::OnPlayModeStop();
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
		Blaze::ParticleSystem2D::OnPlayModeStart(render->GetObjects());
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
		float rawDt = std::chrono::duration<float>(now - m_LastTime).count();
		m_LastTime = now;

		Blaze::Time::Tick(rawDt);
		const float dt = Blaze::Time::GetDeltaTime();
		Blaze::InputActions::Tick();

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
		Blaze::ScriptEngine::UpdateCameraFollow(dt);
		render->Update(dt);

		Blaze::PostProcess2D::BeginSceneCapture(width, height);

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

		{
			const glm::mat4 savedView = Blaze::Camera::m_prop.matrix;
			if (cameraObject)
				Blaze::Camera::m_prop.matrix = cameraObject->GetCameraViewMatrix();
			Blaze::ParticleSystem2D::Flush(*camera);
			Blaze::Camera::m_prop.matrix = savedView;
		}

		if (!Blaze::UISystem::IsReady())
			Blaze::UISystem::Init();
		Blaze::UISystem::Draw(width, height);

		Blaze::PostProcess2D::EndSceneCapturePresent(render->GetObjects(), width, height);
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
		"out vec2 WorldPos;\n"
		"void main()\n"
		"{\n"
		"   vec4 local = ortho * vec4(pos, 1.0);\n"
		"   vec4 world = object_matrix * local;\n"
		"   WorldPos = world.xy;\n"
		"   gl_Position = matrix * world;\n"
		"   TexCoord = texCoord * uv_scale + uv_offset;\n"
		"}\0";

	const char* fragmentShaderSource =
		"#version 330 core\n"
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
