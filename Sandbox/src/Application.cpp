#include <Blaze.h>
#include<memory>

class Sandbox : public Blaze::Application
{
public:
	Sandbox()
	{
	}

	~Sandbox()
	{

	}

};

Blaze::Application* Blaze::CreateApplication()
{
	return new Sandbox();
}