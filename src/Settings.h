#pragma once

class Settings
{
public:
	[[nodiscard]] static Settings* GetSingleton()
	{
		static Settings singleton;
		return std::addressof(singleton);
	}

	void Load()
	{
		constexpr auto path = L"Data/SKSE/Plugins/po3_SavageOffspring.ini";

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(path);

		detail::get_value(ini, aggression, "AI Settings", "Aggression", ";Set base aggression of all child NPCs.\n;0 - Unaggressive, 1 - Aggressive, 2 - Very Aggressive, 3 - Frenzied.");
		detail::get_value(ini, confidence, "AI Settings", "Confidence", ";Set base confidence of all child NPCs.\n;0 - Cowardly, 1 - Cautious, 2 - Average, 3 - Brave, 4 - Foolhardy.");
		detail::get_value(ini, assistance, "AI Settings", "Assistance", ";Set base assistance of all child NPCs.\n;0 - Helps Nobody, 1 - Helps Allies, 2 - Helps Friends and Allies.");
	}

	std::uint32_t aggression{ 1 };
	std::uint32_t confidence{ 2 };
	std::uint32_t assistance{ 1 };

private:
	struct detail
	{
		static void get_value(CSimpleIniA& a_ini, std::uint32_t& a_value, const char* a_section, const char* a_key, const char* a_comment)
		{
			a_value = string::lexical_cast<std::uint32_t>(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str()));
			a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
		};
	};
};
