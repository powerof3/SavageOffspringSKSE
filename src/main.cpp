#include "Settings.h"

namespace NotAChild
{
	void Install()
	{
		//FF 90 F0 02 00 00	- call qword ptr [rax+2F0h]

		std::array targets{
			std::make_pair(36534, 0x161),  // npcAgression
			std::make_pair(39627, 0x82),   // targetLockHUD
			std::make_pair(39727, 0xB2),   // targetLockHUD2
			std::make_pair(39725, 0x31),   // targetLockActor
			std::make_pair(45660, 0xA8),   // lowCombatSelector
			std::make_pair(45922, 0x316),  // encounterZoneTargetResist
			std::make_pair(45922, 0x632),  // encounterZoneTargetResist2
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
			std::make_tuple(37672, 0x571, 0x585),  // addTargetToCombatGroup
			std::make_tuple(37608, 0x2DD, 0x2F1),  // createCombatController
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
			REL::Relocation<std::uintptr_t> combat_override_interupt{ REL::ID(24169) };
			stl::write_thunk_call<FindEnterCombatOverrideInteruptPackage>(combat_override_interupt.address() + 0x24);

			REL::Relocation<std::uintptr_t> spectator_override_interupt{ REL::ID(24166) };
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

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "Slayable Offspring SKSE";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(28);

	NotAChild::Install();
	NoOverridePackage::Quest::Install();

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	return true;
}
