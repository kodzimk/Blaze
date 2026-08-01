#include "AnimStateMachineAsset.h"
#include "Blaze/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Blaze
{
	namespace
	{
		static std::string ParamTypeToString(AnimParamType type)
		{
			switch (type)
			{
			case AnimParamType::Trigger: return "Trigger";
			case AnimParamType::Float: return "Float";
			default: return "Bool";
			}
		}

		static AnimParamType ParseParamType(const std::string& s)
		{
			if (s == "Trigger" || s == "trigger")
				return AnimParamType::Trigger;
			if (s == "Float" || s == "float")
				return AnimParamType::Float;
			return AnimParamType::Bool;
		}

		static const char* FloatCmpToString(AnimFloatCmp cmp)
		{
			switch (cmp)
			{
			case AnimFloatCmp::Less: return "<";
			case AnimFloatCmp::GreaterEqual: return ">=";
			case AnimFloatCmp::LessEqual: return "<=";
			case AnimFloatCmp::Equal: return "==";
			default: return ">";
			}
		}

		static AnimFloatCmp ParseFloatCmp(const std::string& s)
		{
			if (s == "<") return AnimFloatCmp::Less;
			if (s == ">=") return AnimFloatCmp::GreaterEqual;
			if (s == "<=") return AnimFloatCmp::LessEqual;
			if (s == "==" || s == "=") return AnimFloatCmp::Equal;
			return AnimFloatCmp::Greater;
		}
	}

	int AnimStateMachineAsset::FindParam(const std::string& name) const
	{
		for (int i = 0; i < (int)m_Params.size(); ++i)
		{
			if (m_Params[i].name == name)
				return i;
		}
		return -1;
	}

	int AnimStateMachineAsset::FindState(const std::string& name) const
	{
		for (int i = 0; i < (int)m_States.size(); ++i)
		{
			if (m_States[i].name == name)
				return i;
		}
		return -1;
	}

	int AnimStateMachineAsset::AddParam(const AnimParam& param)
	{
		if (param.name.empty())
			return -1;
		const int existing = FindParam(param.name);
		if (existing >= 0)
		{
			m_Params[existing] = param;
			return existing;
		}
		m_Params.push_back(param);
		return (int)m_Params.size() - 1;
	}

	int AnimStateMachineAsset::AddState(const std::string& name, const std::string& clipName)
	{
		std::string unique = name.empty() ? "State" : name;
		if (FindState(unique) >= 0)
		{
			for (int i = 2; i < 10000; ++i)
			{
				const std::string candidate = unique + "_" + std::to_string(i);
				if (FindState(candidate) < 0)
				{
					unique = candidate;
					break;
				}
			}
		}
		AnimState state;
		state.name = unique;
		state.clipName = clipName.empty() ? unique : clipName;
		m_States.push_back(state);
		if (m_DefaultState.empty())
			m_DefaultState = unique;
		return (int)m_States.size() - 1;
	}

	void AnimStateMachineAsset::AddTransition(const AnimTransition& transition)
	{
		if (transition.to.empty())
			return;
		m_Transitions.push_back(transition);
	}

	bool AnimStateMachineAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_CORE_ERROR("Failed to open anim asset: {0}", path);
			return false;
		}

		m_Path = path;
		m_DefaultState.clear();
		m_Params.clear();
		m_States.clear();
		m_Transitions.clear();

		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeAnim")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			if (key == "default")
			{
				stream >> m_DefaultState;
			}
			else if (key == "param")
			{
				std::string typeStr, name;
				stream >> typeStr >> name;
				if (name.empty())
					continue;
				AnimParam param;
				param.name = name;
				param.type = ParseParamType(typeStr);
				if (param.type == AnimParamType::Bool)
				{
					int v = 0;
					stream >> v;
					param.boolDefault = v != 0;
				}
				else if (param.type == AnimParamType::Float)
				{
					float v = 0.0f;
					stream >> v;
					param.floatDefault = v;
				}
				AddParam(param);
			}
			else if (key == "state")
			{
				std::string name, clip;
				stream >> name >> clip;
				if (name.empty())
					continue;
				if (clip.empty())
					clip = name;
				AnimState state;
				state.name = name;
				state.clipName = clip;
				if (FindState(name) < 0)
					m_States.push_back(state);
			}
			else if (key == "trans")
			{
				AnimTransition t;
				stream >> t.from >> t.to;
				if (t.from.empty() || t.to.empty())
					continue;

				std::string token;
				while (stream >> token)
				{
					if (token == "finished")
					{
						t.onFinished = true;
					}
					else if (token == "bool")
					{
						int equals = 1;
						stream >> t.boolName >> equals;
						t.hasBool = !t.boolName.empty();
						t.boolEquals = equals != 0;
					}
					else if (token == "trigger")
					{
						stream >> t.triggerName;
						t.hasTrigger = !t.triggerName.empty();
					}
					else if (token == "float")
					{
						std::string cmp;
						stream >> t.floatName >> cmp >> t.floatValue;
						t.hasFloat = !t.floatName.empty();
						t.floatCmp = ParseFloatCmp(cmp);
					}
				}

				if (t.onFinished || t.hasBool || t.hasTrigger || t.hasFloat)
					m_Transitions.push_back(t);
			}
		}

		if (!headerOk)
		{
			BZ_CORE_ERROR("Invalid anim asset header: {0}", path);
			return false;
		}
		if (m_DefaultState.empty() && !m_States.empty())
			m_DefaultState = m_States.front().name;
		return true;
	}

	bool AnimStateMachineAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to save anim asset: {0}", path);
			return false;
		}

		out << "BlazeAnim 1\n";
		if (!m_DefaultState.empty())
			out << "default " << m_DefaultState << "\n";

		for (const AnimParam& param : m_Params)
		{
			out << "param " << ParamTypeToString(param.type) << " " << param.name;
			if (param.type == AnimParamType::Bool)
				out << " " << (param.boolDefault ? 1 : 0);
			else if (param.type == AnimParamType::Float)
				out << " " << param.floatDefault;
			out << "\n";
		}

		for (const AnimState& state : m_States)
			out << "state " << state.name << " " << state.clipName << "\n";

		for (const AnimTransition& t : m_Transitions)
		{
			out << "trans " << t.from << " " << t.to;
			if (t.onFinished)
				out << " finished";
			if (t.hasBool)
				out << " bool " << t.boolName << " " << (t.boolEquals ? 1 : 0);
			if (t.hasTrigger)
				out << " trigger " << t.triggerName;
			if (t.hasFloat)
				out << " float " << t.floatName << " " << FloatCmpToString(t.floatCmp) << " " << t.floatValue;
			out << "\n";
		}
		return true;
	}

	AnimStateMachineAsset AnimStateMachineAsset::CreateDefault(const std::string& name)
	{
		AnimStateMachineAsset asset;
		(void)name;
		asset.SetDefaultState("Idle");

		AnimParam running;
		running.name = "IsRunning";
		running.type = AnimParamType::Bool;
		running.boolDefault = false;
		asset.AddParam(running);

		AnimParam attack;
		attack.name = "Attack";
		attack.type = AnimParamType::Trigger;
		asset.AddParam(attack);

		asset.AddState("Idle", "Idle");
		asset.AddState("Run", "Run");
		asset.AddState("Attack", "Attack");

		AnimTransition idleToRun;
		idleToRun.from = "Idle";
		idleToRun.to = "Run";
		idleToRun.hasBool = true;
		idleToRun.boolName = "IsRunning";
		idleToRun.boolEquals = true;
		asset.AddTransition(idleToRun);

		AnimTransition runToIdle;
		runToIdle.from = "Run";
		runToIdle.to = "Idle";
		runToIdle.hasBool = true;
		runToIdle.boolName = "IsRunning";
		runToIdle.boolEquals = false;
		asset.AddTransition(runToIdle);

		AnimTransition anyAttack;
		anyAttack.from = "*";
		anyAttack.to = "Attack";
		anyAttack.hasTrigger = true;
		anyAttack.triggerName = "Attack";
		asset.AddTransition(anyAttack);

		AnimTransition attackDone;
		attackDone.from = "Attack";
		attackDone.to = "Idle";
		attackDone.onFinished = true;
		asset.AddTransition(attackDone);

		return asset;
	}
}
