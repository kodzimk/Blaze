#include <Blaze.h>
#include<glad/glad.h>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer(vertexShaderSource,fragmentShaderSource);
		render->CreateObject(vertices,indices);
		render->CreateObject(vertices, indices);
		render->GetGameObject("Object2").SetTexture("res/texture/wall.jpg");
		render->GetGameObject("Object2").SetPosition(glm::vec3(0.0f,0.5f,0.0f));
		render->GetGameObject("Object1").SetTexture("res/texture/container.jpg");
		camera = Blaze::Camera::CreateCamera(glm::vec3(0.0f,0.0f,0.0f));
		
	}

	~ExampleLayer()
	{
		delete render;
		delete camera;
	}

	void OnUpdate() override
	{
		
		camera->CameraMove();
		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();
	}

	void OnEvent(Blaze::Event& event) override
	{
		static float height = 900.f, width = 1600.f;
		if (event.GetEventType() == Blaze::EventType::WindowResize)
		{
			auto& e = (Blaze::WindowResizeEvent&)event;
			camera->zoom *= e.GetWidth() / width;
			camera->zoom *= e.GetHeight() / height;

			camera->GetCameraProp().projection = glm::ortho((float)(-e.GetWidth() / 2), (float)(e.GetWidth() / 2), (float)(-e.GetHeight() / 2), (float)(e.GetHeight() / 2), -100.f, 100.f);

			width = e.GetWidth();
			height = e.GetHeight();
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			if (e.GetKeyCode() == BZ_KEY_SPACE)
			{
				render->GetGameObject("Object1").SetTexture("res/texture/wall.jpg");
			}
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			if (e.GetKeyCode() == BZ_KEY_C)
			{
				render->GetGizmo().Focus(&render->GetGameObject("Object1"));
			}
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			if (e.GetKeyCode() == BZ_KEY_1)
			{
				render->GetGizmo().UnFocus();
			}
		}

		if (event.GetEventType() == Blaze::EventType::KeyPressed)
		{
			auto& e = (Blaze::KeyPressedEvent&)event;
			if (e.GetKeyCode() == BZ_KEY_T)
			{
				render->GetGizmo().MoveGizmo();
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