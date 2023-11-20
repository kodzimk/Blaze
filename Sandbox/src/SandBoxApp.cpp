#include<Blaze.h>

class SandBox :public Blaze::Application
{
public:
	SandBox()
	{

	}
	~SandBox()
	{

	}
};

Blaze::Application* Blaze::CreateApplication()
{
	return new SandBox();
}