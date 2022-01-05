#include "Settings.h"

namespace NotAChild
{
	void Install()
	{
		//FF 90 F0 02 00 00	- call qword ptr [rax+2F0h]

		std::array targets{
			std::make_pair(37534, 0x165),  // npcAgression
			std::make_pair(40713, 0x82),   // targetLockHUD
			std::make_pair(40834, 0xBE),   // targetLockHUD2
			std::make_pair(40828, 0x31),   // targetLockActor
			std::make_pair(46957, 0x8D),   // lowCombatSelector
			std::make_pair(47196, 0x5C),   // encounterZoneTargetResist (function got un-inlined, previously this was part of 47195)
			std::make_pair(47195, 0x578),  // encounterZoneTargetResist2
		};

		struct Patch : Xbyak::CodeGenerator
		{
			Patch()
			{
				call(qword[rax + 0x830]);  //unused vfunc? that returns false for actor classes
			}
		};

		Patch patch;
		patch.ready();

		for (const auto& [id, offset] : targets) {
			REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
			REL::safe_write(target.address() + offset, std::span{ patch.getCode(), patch.getSize() });
		}
	}
}

namespace NotChildRace
{
	void Install()
	{
		//text : 0000000140628B18 mov ecx, [rax + 164h].
		//text : 0000000140628B1E shr ecx, 1
		//text : 0000000140628B20 cl, 1
		//text : 0000000140628B23 jz short loc_140628B55

		std::array targets{
			std::make_tuple(38626, 0x58F, 0x5A3),  // addTargetToCombatGroup
			std::make_tuple(38561, 0x2D7, 0x2EB),  // createCombatController
		};

		for (const auto& [id, start, end] : targets) {
			REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
			for (uintptr_t i = start; i < end; ++i) {
				REL::safe_write(target.address() + i, REL::NOP);
			}
		}
	}
}

namespace NoOverridePackage
{
	namespace Base
	{
		void Install()
		{
			const auto settings = Settings::GetSingleton();

			const auto aggressionLvl = static_cast<RE::ACTOR_AGGRESSION>(settings->aggression);
			const auto confidenceLvl = static_cast<RE::ACTOR_CONFIDENCE>(settings->confidence);
			const auto assistanceLvl = static_cast<RE::ACTOR_ASSISTANCE>(settings->assistance);

			if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
				for (const auto& npc : dataHandler->GetFormArray<RE::TESNPC>()) {
					if (npc && npc->race && npc->race->IsChildRace()) {
						if (npc->GetAggressionLevel() < aggressionLvl) {
							npc->SetAggressionLevel(aggressionLvl);
						}
						if (npc->GetConfidenceLevel() < confidenceLvl) {
							npc->SetConfidenceLevel(confidenceLvl);
						}
						if (npc->GetAssistanceLevel() < assistanceLvl) {
							npc->SetAssistanceLevel(assistanceLvl);
						}

						npc->spectatorOverRidePackList = nullptr;
						npc->enterCombatOverRidePackList = nullptr;
					}
				}
			}
		}
	}

	namespace Quest
	{
		struct FindEnterCombatOverrideInteruptPackage
		{
			static RE::TESPackage* thunk(RE::ExtraAliasInstanceArray* a_this, RE::Actor* a_actor)
			{
				const auto package = func(a_this, a_actor);
				return a_actor && a_actor->IsChild() ? nullptr : package;
			}
			static inline REL::Relocation<decltype(&thunk)> func;
		};

		struct FindSpectatorOverrideInteruptPackage
		{
			static RE::TESPackage* thunk(RE::ExtraAliasInstanceArray* a_this, RE::Actor* a_actor)
			{
				const auto package = func(a_this, a_actor);
				return a_actor && a_actor->IsChild() ? nullptr : package;
			}
			static inline REL::Relocation<decltype(&thunk)> func;
		};

		void Install()
		{
			REL::Relocation<std::uintptr_t> combat_override_interupt{ REL::ID(24673) };
			stl::write_thunk_call<FindEnterCombatOverrideInteruptPackage>(combat_override_interupt.address() + 0x24);

			REL::Relocation<std::uintptr_t> spectator_override_interupt{ REL::ID(24670) };
			stl::write_thunk_call<FindSpectatorOverrideInteruptPackage>(spectator_override_interupt.address() + 0x24);
		}
	}
}

void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
		NoOverridePackage::Base::Install();
	}
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Savage Offspring SKSE");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(28);

	NotAChild::Install();
	NoOverridePackage::Quest::Install();

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	return true;
}
