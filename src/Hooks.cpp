#include "Hooks.h"
#include "Settings.h"

namespace Hooks
{
	namespace NotAChild
	{
		void Install()
		{
			//FF 90 F0 02 00 00	- call qword ptr [rax+2F0h]

			std::array targets{
#ifdef SKYRIM_AE
				std::make_pair(37534, 0x165),  // npcAgression
				std::make_pair(40713, 0x82),   // targetLockHUD
				std::make_pair(40834, 0xBE),   // targetLockHUD2
				std::make_pair(40828, 0x31),   // targetLockActor
				std::make_pair(46957, 0x8D),   // lowCombatSelector
				std::make_pair(47196, 0x5C),   // encounterZoneTargetResist (function got un-inlined, previously this was part of 47195)
				std::make_pair(47195, 0x578),  // encounterZoneTargetResist2
#else
				std::make_pair(36534, 0x161),          // npcAgression
				std::make_pair(39627, 0x82),           // targetLockHUD
				std::make_pair(39727, 0xB2),           // targetLockHUD2
				std::make_pair(39725, 0x31),           // targetLockActor
				std::make_pair(45660, 0xA8),           // lowCombatSelector
				std::make_pair(45922, 0x316),          // encounterZoneTargetResist
				std::make_pair(45922, 0x632),          // encounterZoneTargetResist2
#endif
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
#ifdef SKYRIM_AE
				std::make_tuple(38626, 0x58F, 0x5A3),  // addTargetToCombatGroup
				std::make_tuple(38561, 0x2D7, 0x2EB),  // createCombatController
#else
				std::make_tuple(37672, 0x571, 0x585),  // addTargetToCombatGroup
				std::make_tuple(37608, 0x2DD, 0x2F1),  // createCombatController
#endif
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
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct FindSpectatorOverrideInteruptPackage
			{
				static RE::TESPackage* thunk(RE::ExtraAliasInstanceArray* a_this, RE::Actor* a_actor)
				{
					const auto package = func(a_this, a_actor);
					return a_actor && a_actor->IsChild() ? nullptr : package;
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			void Install()
			{
				REL::Relocation<std::uintptr_t> combat_override_interupt{ RELOCATION_ID(24169, 24673), 0x24 };
				stl::write_thunk_call<FindEnterCombatOverrideInteruptPackage>(combat_override_interupt.address());

				REL::Relocation<std::uintptr_t> spectator_override_interupt{ RELOCATION_ID(24166, 24670), 0x24 };
				stl::write_thunk_call<FindSpectatorOverrideInteruptPackage>(spectator_override_interupt.address());
			}
		}
	}

	void InstallOnPostLoad()
	{
		Settings::GetSingleton()->Load();

		SKSE::AllocTrampoline(28);

		NotAChild::Install();
		NoOverridePackage::Quest::Install();
	}

	void InstallOnDataLoad()
	{
		NoOverridePackage::Base::Install();
	}
}
