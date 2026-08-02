#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <vector>

namespace Blaze
{
	enum class AnimParamType
	{
		Bool = 0,
		Trigger,
		Float
	};

	struct BLAZE_API AnimParam
	{
		std::string name;
		AnimParamType type = AnimParamType::Bool;
		bool boolDefault = false;
		float floatDefault = 0.0f;
	};

	struct BLAZE_API AnimState
	{
		std::string name;
		std::string clipName; // FlipbookClip name
	};

	enum class AnimFloatCmp
	{
		Greater = 0,
		Less,
		GreaterEqual,
		LessEqual,
		Equal
	};

	struct BLAZE_API AnimTransition
	{
		std::string from; // exact state, or "*" for any
		std::string to;
		bool onFinished = false;
		bool hasBool = false;
		std::string boolName;
		bool boolEquals = true;
		bool hasTrigger = false;
		std::string triggerName;
		bool hasFloat = false;
		std::string floatName;
		float floatValue = 0.0f;
		AnimFloatCmp floatCmp = AnimFloatCmp::GreaterEqual;
	};

	class BLAZE_API AnimStateMachineAsset
	{
	public:
		AnimStateMachineAsset() = default;

		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		const std::string& GetDefaultState() const { return m_DefaultState; }
		void SetDefaultState(const std::string& name) { m_DefaultState = name; }

		std::vector<AnimParam>& GetParams() { return m_Params; }
		const std::vector<AnimParam>& GetParams() const { return m_Params; }
		std::vector<AnimState>& GetStates() { return m_States; }
		const std::vector<AnimState>& GetStates() const { return m_States; }
		std::vector<AnimTransition>& GetTransitions() { return m_Transitions; }
		const std::vector<AnimTransition>& GetTransitions() const { return m_Transitions; }

		int FindParam(const std::string& name) const;
		int FindState(const std::string& name) const;
		int AddParam(const AnimParam& param);
		int AddState(const std::string& name, const std::string& clipName);
		void AddTransition(const AnimTransition& transition);

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		static AnimStateMachineAsset CreateDefault(const std::string& name = "Anim");

	private:
		std::string m_Path;
		std::string m_DefaultState = "Idle";
		std::vector<AnimParam> m_Params;
		std::vector<AnimState> m_States;
		std::vector<AnimTransition> m_Transitions;
	};
}
