#include "ActorUpdateManager.h"
#include "BodyGenInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "SkinInterface.h"


extern BodyGenInterface		g_bodyGenInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverlayInterface		g_overlayInterface;
extern SkinInterface		g_skinInterface;

extern bool g_bEnableBodygen;
extern bool g_bEnableBodyMorphs;
extern bool g_bEnableOverlays;
extern bool g_bEnableSkinOverrides;

BSEventNotifyControl	ActorUpdateManager::ProcessEvent(const TESObjectLoadedEvent & evn, BSTEventSource<TESObjectLoadedEvent> * dispatcher)
{
	if(evn.loaded)
	{
		// We need to collect pending loads because these will fire before the load game event
		TESForm * form = LookupFormByID(evn.formID);
		if(!form)
			return BSEventNotifyControl::kContinue;

		Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
		if(!actor)
			return BSEventNotifyControl::kContinue;

		TESNPC * npc =  DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(!npc)
			return BSEventNotifyControl::kContinue;

		SEX gender = CALL_MEMBER_FN(npc, GetSex)();
		bool isFemale = gender == SEX::kFemale ? true : false;

		m_pendingLock.Lock();
		if(m_loading) // We're mid-load, lets just push these to pending
		{
			m_pendingActors.insert(((UInt64)gender << 32) | form->formID);
		}
		else
		{
			// We've loaded the game, we can just generate and apply morphs if we don't already have any and we meet the outlined criteria for generation
			auto morphMap = g_bodyMorphInterface.GetMorphMap(actor, isFemale);
			if(!morphMap && g_bEnableBodygen)
			{
				if(g_bodyGenInterface.EvaluateBodyMorphs(actor, isFemale))
					morphMap = g_bodyMorphInterface.GetMorphMap(actor, isFemale);
			}
			if(g_bEnableSkinOverrides)
			{
				g_skinInterface.UpdateSkinOverride(actor, false);
			}
			if(morphMap && g_bEnableBodyMorphs)
			{
				g_bodyMorphInterface.UpdateMorphs(actor);
			}
			if(g_bEnableOverlays)
			{
				g_overlayInterface.UpdateOverlays(actor);
			}
		}
		m_pendingLock.Release();
	}

	return BSEventNotifyControl::kContinue;
};

BSEventNotifyControl	ActorUpdateManager::ProcessEvent(const TESInitScriptEvent & evn, BSTEventSource<TESInitScriptEvent> * dispatcher)
{
	// Don't do any generation if BodyGen not enabled
	if(!g_bEnableBodygen)
		return BSEventNotifyControl::kContinue;

	// We need to collect pending loads because these will fire before the load game event
	Actor * actor = DYNAMIC_CAST(evn.hObjectInitialized, TESForm, Actor);
	if(!actor)
		return BSEventNotifyControl::kContinue;

	TESNPC * npc =  DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
	if(!npc)
		return BSEventNotifyControl::kContinue;

	SEX gender = CALL_MEMBER_FN(npc, GetSex)();
	bool isFemale = gender == SEX::kFemale ? true : false;

	if(!m_loading)
	{
		// We've spawned a new Reference, can we morph it?
		auto morphMap = g_bodyMorphInterface.GetMorphMap(actor, isFemale);
		if(!morphMap)
		{
			if(g_bodyGenInterface.EvaluateBodyMorphs(actor, isFemale))
				g_bodyMorphInterface.UpdateMorphs(actor);
		}
	}
	return BSEventNotifyControl::kContinue;
}

void ActorUpdateManager::ResolvePendingBodyGen()
{
	// We don't need to generate any new morphs at this time
	if(g_bEnableBodygen)
	{
		m_pendingLock.Lock();
		for(auto & uid : m_pendingActors)
		{
			UInt32 formID = uid & 0xFFFFFFFF;
			bool isFemale = uid & (1LL << 32) ? true : false;

			TESForm * form = LookupFormByID(formID);
			if(form && form->formType == Actor::FORM_ID)
			{
				Actor * actor = static_cast<Actor*>(form);
				auto morphMap = g_bodyMorphInterface.GetMorphMap(actor, isFemale);
				if(!morphMap)
				{
					if(g_bodyGenInterface.EvaluateBodyMorphs(actor, isFemale))
						m_pendingUpdates.emplace(formID);
				}
			}
		}

		m_pendingActors.clear();
		m_pendingLock.Release();
	}
}

BSEventNotifyControl ActorUpdateManager::ProcessEvent(const TESLoadGameEvent & evn, BSTEventSource<TESLoadGameEvent> * dispatcher)
{
	if(m_loading)
		m_loading = false;

	Flush();
	return BSEventNotifyControl::kContinue;
}

void ActorUpdateManager::Flush()
{
	m_pendingLock.Lock();
	for(auto & uid : m_pendingUpdates)
	{
		TESForm * form = LookupFormByID((UInt32)uid);
		if(form && form->formType == Actor::FORM_ID)
		{
			Actor * actor = static_cast<Actor*>(form);
			if(g_bEnableSkinOverrides)
				g_skinInterface.UpdateSkinOverride(actor, false); // Load game doesnt need to update skin
			if(g_bEnableBodyMorphs)
				g_bodyMorphInterface.UpdateMorphs(actor);
			if(g_bEnableOverlays)
				g_overlayInterface.UpdateOverlays(actor);
		}
	}

	m_pendingUpdates.clear();
	m_pendingLock.Release();
}

void ActorUpdateManager::PushUpdate(Actor * actor)
{
	m_pendingLock.Lock();
	m_pendingUpdates.emplace(actor->formID);
	m_pendingLock.Release();
}

void ActorUpdateManager::Revert()
{
	m_pendingLock.Lock();
	m_pendingActors.clear();
	m_pendingUpdates.clear();
	m_pendingLock.Release();
}