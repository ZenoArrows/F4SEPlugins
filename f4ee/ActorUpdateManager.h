#pragma once

#include "Utilities.h"

#include <unordered_set>

class ActorUpdateManager :
	public BSTEventSink<TESInitScriptEvent>,
	public BSTEventSink<TESObjectLoadedEvent>,
	public BSTEventSink<TESLoadGameEvent>
{
public:
	ActorUpdateManager() : m_loading(false) { }
	virtual ~ActorUpdateManager() { }

	virtual	BSEventNotifyControl	ProcessEvent(const TESObjectLoadedEvent & evn, BSTEventSource<TESObjectLoadedEvent> * dispatcher) override;
	virtual	BSEventNotifyControl	ProcessEvent(const TESLoadGameEvent & evn, BSTEventSource<TESLoadGameEvent> * dispatcher) override;
	virtual	BSEventNotifyControl	ProcessEvent(const TESInitScriptEvent & evn, BSTEventSource<TESInitScriptEvent> * dispatcher) override;

	virtual void Flush();
	virtual void PushUpdate(Actor * actor);
	virtual void Revert();

	void SetLoading(bool loading) { m_loading = loading; }
	void ResolvePendingBodyGen();

	SimpleLock					m_pendingLock;
	bool						m_loading;			// True when the game is loading, false when the cell has loaded
	std::unordered_set<UInt64>	m_pendingActors;	// Stores the pending actors while loading (Populated while loading, erased during load, remaining actors get new morphs, cleared after)
	std::unordered_set<UInt64>	m_pendingUpdates;	// Stores the actors for update
};