#pragma once
#include"Log.h"

void GLFWErrorCallback(int error, const char* description)
{
    BZ_CORE_ERROR("Error: %s\n", description);
}
