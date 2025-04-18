#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include"Blaze/Logging/Log.h"

#define BIND_EVENT_FN(x) std::bind(&Application::x,std::placeholders::_1)