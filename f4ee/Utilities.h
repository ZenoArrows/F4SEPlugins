#pragma once

#include <cmath>
#include <string>
#include <functional>
#include <mutex>
#include <map>

typedef RE::TESFile ModInfo;

class F4EEFixedString;

namespace Serialization
{
	// template helper functions
	template <typename T>
	bool WriteData(const F4SE::SerializationInterface * intfc, const T * data)
	{
		return intfc->WriteRecordData(data, sizeof(T));
	}

	template <typename T>
	bool ReadData(const F4SE::SerializationInterface * intfc, T * data)
	{
		return intfc->ReadRecordData(data, sizeof(T)) > 0;
	}

	template <> bool WriteData<F4EEFixedString>(const F4SE::SerializationInterface * intfc, const F4EEFixedString * data);
	template <> bool ReadData<F4EEFixedString>(const F4SE::SerializationInterface * intfc, F4EEFixedString * data);
};

namespace std
{
	std::string &ltrim(std::string &s);// trim from end
	std::string &rtrim(std::string &s);
	std::string &trim(std::string &s);// trim from both ends
	std::vector<std::string> explode(const std::string& str, const char& ch);
}

std::string bytes_to_string(std::size_t size);
TESRace * GetRaceByName(const std::string & name);
TESRace * GetActorRace(Actor * actor);
std::string GetFormIdentifier(TESForm * form);
TESForm * GetFormFromIdentifier(const std::string & formIdentifier);
TESForm * LookupFormByID(std::uint32_t id);

template<int MaxBuf>
class BSResourceTextFile
{
public:
	BSResourceTextFile(BSResourceNiBinaryStream* file) : fin(file) { }

	bool ReadLine(std::string* str)
	{
		UInt32 ret = fin->ReadLine((char*)buf, MaxBuf, '\n');
		if (ret > 0) {
			*str = buf;
			return true;
		}
		return false;
	}

protected:
	BSResourceNiBinaryStream* fin;
	char buf[MaxBuf];
};

template<typename T>
static bool AreEqual(T f1, T f2) { 
	return (std::fabs(f1 - f2) <= std::numeric_limits<T>::epsilon() * (std::max)(fabs(f1), fabs(f2)));
}

void BSReadAll(BSResourceNiBinaryStream* fin, std::string* str);
bool VisitObjects(NiPointer<NiAVObject> parent, std::function<bool(NiPointer<NiAVObject>)> functor);
void VisitLeveledCharacter(TESLevCharacter * character, std::function<void(TESNPC*)> functor);

// Travels up a node tree to determine the right node
NiNode * GetRootNode(Actor * actor, NiPointer<NiAVObject> object);

void ForEachMod(std::function<void(const ModInfo*)> functor);

void * Heap_Allocate(size_t size);
void Heap_Free(void * ptr);

typedef std::map <const std::type_info *, GFx::FunctionHandler *>	FunctionHandlerCache;
extern FunctionHandlerCache g_functionHandlerCache;

template <typename T>
void CreateFunction(GFx::Value * dst, GFx::Movie * movie)
{
	// either allocate the object or retrieve an existing instance from the cache
	GFx::FunctionHandler	* fn = nullptr;

	// check the cache
	FunctionHandlerCache::iterator iter = g_functionHandlerCache.find(&typeid(T));
	if(iter != g_functionHandlerCache.end())
		fn = iter->second;

	if(!fn)
	{
		// not found, allocate a new one
		fn = new T;

		// add it to the cache
		// cache now owns the object as far as refcounting goes
		g_functionHandlerCache[&typeid(T)] = fn;
	}

	// create the function object
	movie->CreateFunction(dst, fn);
}

template <typename T>
void RegisterFunction(GFx::Value * dst, GFx::Movie * movie, const char * name)
{
	// either allocate the object or retrieve an existing instance from the cache
	GFx::Value fnValue;
	CreateFunction<T>(&fnValue, movie);
	dst->SetMember(name, &fnValue);
}

namespace PapyrusVM
{
UInt64 GetHandleFromObject(void * src, ENUM_FORM_ID formID);
void * GetObjectFromHandle(UInt64 handle, ENUM_FORM_ID formID);
};

class SimpleLock
{
	std::recursive_mutex mutex;

public:
	SimpleLock() : mutex() {}

	void Lock(void) { mutex.lock(); }
	void Release(void) { mutex.unlock(); }
};

class SimpleLocker
{
public:
	SimpleLocker(SimpleLock * dataHolder) { m_lock = dataHolder; m_lock->Lock(); }
	~SimpleLocker() { m_lock->Release(); }

protected:
	SimpleLock	* m_lock;
};

template <typename T>
class SafeDataHolder
{
protected:
	SimpleLock	m_lock;
public:
	T			m_data;

	void	Lock(void) { m_lock.Lock(); }
	void	Release(void) { m_lock.Release(); }
};
