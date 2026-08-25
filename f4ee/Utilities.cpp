#include "Utilities.h"
#include "StringTable.h"

#include <iomanip>
#include <sstream>
#include <cctype>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <functional>

FunctionHandlerCache g_functionHandlerCache;

template <>
bool Serialization::WriteData<F4EEFixedString>(const F4SE::SerializationInterface * intfc, const F4EEFixedString * str)
{
	UInt16 len = (UInt16)strlen(str->c_str());
	if (len > SHRT_MAX)
		return false;
	if (! intfc->WriteRecordData(&len, sizeof(len)))
		return false;
	if (len == 0)
		return true;
	if (! intfc->WriteRecordData(str->c_str(), len))
		return false;
	return true;
}

template <>
bool Serialization::ReadData<F4EEFixedString>(const F4SE::SerializationInterface * intfc, F4EEFixedString * str)
{
	UInt16 len = 0;

	if (! intfc->ReadRecordData(&len, sizeof(len)))
		return false;
	if(len == 0)
		return true;
	if (len > SHRT_MAX)
		return false;

	char * buf = new char[len + 1];
	buf[0] = 0;

	if (! intfc->ReadRecordData(buf, len)) {
		delete [] buf;
		return false;
	}
	buf[len] = 0;

	*str = F4EEFixedString(buf);
	delete [] buf;
	return true;
}

std::string bytes_to_string(std::size_t size) {               
	static const char *SIZES[] = { "B", "KB", "MB", "GB" };

	int div = 0;
	size_t rem = 0;
	while (size >= 1024 && div < (sizeof SIZES / sizeof *SIZES)) {
		rem = (size % 1024);
		div++;
		size /= 1024;
	}

	double size_d = (float)size + (float)rem / 1024.0;

	std::stringstream ss;
	ss << std::fixed << std::setprecision(2) << size_d << SIZES[div];
	return ss.str();
}

void BSReadAll(BSResourceNiBinaryStream* fin, std::string* str)
{
	char ch;
	size_t ret = fin->DoRead(&ch, 1);
	while (ret > 0) {
		str->push_back(ch);
		ret = fin->DoRead(&ch, 1);
	}
}

bool VisitObjects(NiPointer<NiAVObject> parent, std::function<bool(NiPointer<NiAVObject>)> functor)
{
	if (functor(parent))
		return true;

	NiPointer<NiNode> node(parent->IsNode());
	if(node) {
		for(NiPointer<NiAVObject> object : node->children) {
			if(object) {
				if (VisitObjects(object, functor))
					return true;
			}
		}
	}

	return false;
}


std::string GetFormIdentifier(TESForm * form)
{
	char formName[256];
	UInt8 modIndex = form->formID >> 24;
	UInt32 modForm = form->formID & 0xFFFFFF;

	ModInfo* modInfo = nullptr;
	if(modIndex == 0xFE)
	{
		UInt16 lightIndex = (form->formID >> 12) & 0xFFF;
		if(lightIndex < TESDataHandler::GetSingleton()->compiledFileCollection.smallFiles.size())
			modInfo = TESDataHandler::GetSingleton()->compiledFileCollection.smallFiles[lightIndex];
	}
	else
	{
		modInfo = TESDataHandler::GetSingleton()->compiledFileCollection.files[modIndex];
	}
	
	if (modInfo) {
		sprintf_s(formName, "%s|%06X", modInfo->filename, modForm);
	}

	return formName;
}

TESForm * GetFormFromIdentifier(const std::string & formIdentifier)
{
	std::size_t pos = formIdentifier.find_first_of('|');
	std::string modName = formIdentifier.substr(0, pos);
	std::string modForm = formIdentifier.substr(pos+1);

	UInt32 formId = 0;
	sscanf_s(modForm.c_str(), "%X", &formId);

	std::optional<uint8_t> modIndex = TESDataHandler::GetSingleton()->GetLoadedModIndex(modName.c_str());
	if(modIndex) {
		formId |= ((UInt32)*modIndex) << 24;
	}
	else
	{
		std::optional<uint16_t> lightModIndex = TESDataHandler::GetSingleton()->GetLoadedLightModIndex(modName.c_str());
		if(lightModIndex) {
			formId |= 0xFE000000 | (UInt32(*lightModIndex) << 12);
		}
	}

	return LookupFormByID(formId);
}

TESForm * LookupFormByID(std::uint32_t id)
{
	return TESForm::GetFormByNumericID(id);
}

TESRace * GetActorRace(Actor * actor)
{
	TESRace * race = actor->race;
	if(!race) {
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc)
			race = npc->formRace;
	}

	return race;
}

TESRace * GetRaceByName(const std::string & raceName)
{
	F4EEFixedString lower(raceName.c_str());
	for(TESForm * form : TESDataHandler::GetSingleton()->formArrays[std::to_underlying(ENUM_FORM_ID::kRACE)])
	{
		TESRace * race = (TESRace *)form;
		F4EEFixedString raceName(race->formEditorID.c_str());
		if(raceName == lower)
		{
			return race;
		}
	}

	return nullptr;
}

namespace std {
std::string& ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }));
	return s;
}

// trim from end
std::string& rtrim(std::string& s) {
	s.erase((std::find_if_not(s.rbegin(), s.rend(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); })).base(), s.end());
	return s;
}

// trim from both ends
std::string &std::trim(std::string &s) {
	return std::ltrim(std::rtrim(s));
}
}

std::vector<std::string> std::explode(const std::string& str, const char& ch) {
	std::string next;
	std::vector<std::string> result;

	for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
		if (*it == ch) {
			if (!next.empty()) {
				result.push_back(next);
				next.clear();
			}
		}
		else {
			next += *it;
		}
	}
	if (!next.empty())
		result.push_back(next);
	return result;
}

void VisitLeveledCharacter(TESLevCharacter * character, std::function<void(TESNPC*)> functor)
{
	std::unordered_set<TESLevCharacter*> visited;
	std::queue<TESLevCharacter*> visit;

	visit.push(character);

	while(!visit.empty())
	{
		character = visit.front();
		visit.pop();

		if(character)
		{
			for(std::int8_t i = 0; i < character->baseListCount; i++)
			{
				TESForm * form = character->leveledLists[i].form;
				if(form) {
					TESLevCharacter * levCharacter = DYNAMIC_CAST(form, TESForm, TESLevCharacter);
					if(levCharacter && visited.find(levCharacter) == visited.end())
						visit.push(levCharacter);

					TESNPC * npc = DYNAMIC_CAST(form, TESForm, TESNPC);
					if(npc)
						functor(npc);
				}
			}

			visited.insert(character);
		}
	}
}

NiNode * GetRootNode(Actor * actor, NiPointer<NiAVObject> object)
{
	NiAVObject * rootNode = actor->Get3D(false);

	bool isFirstPerson = false;

	// Only the player will have a first person skeleton
	if(actor == PlayerCharacter::GetPlayer()) {
		NiAVObject * node1P = actor->Get3D(true);

		// Go up to the root and see if it is the first person one
		NiAVObject * foundNode = nullptr;
		NiNode * parent = object->parent;
		while(parent)
		{
			if (parent == node1P) {
				foundNode = node1P;
				break;
			}
			parent = parent->parent;
		}

		isFirstPerson = (foundNode == node1P);
		if(isFirstPerson)
			rootNode = node1P;
	}

	return (NiNode *)rootNode;
}

void ForEachMod(std::function<void(const ModInfo*)> functor)
{
	const TESFileCollection& coll = TESDataHandler::GetSingleton()->compiledFileCollection;
	std::for_each(coll.files.begin(), coll.files.end(), functor);
	std::for_each(coll.smallFiles.begin(), coll.smallFiles.end(), functor);
}

void * Heap_Allocate(size_t size)
{
    MemoryManager& mm = MemoryManager::GetSingleton();
	return mm.Allocate(size, 0, false);
}

void Heap_Free(void * ptr)
{
    MemoryManager& mm = MemoryManager::GetSingleton();
	mm.Deallocate(ptr, false);
}

UInt64 PapyrusVM::GetHandleFromObject(void * src, ENUM_FORM_ID formID)
{
	BSScript::IVirtualMachine		* registry =	GameVM::GetSingleton()->impl.get();
	BSScript::IObjectHandlePolicy	& policy =		registry->GetObjectHandlePolicy();

	return policy.GetHandleForObject((std::uint32_t)formID, (void*)src);
}

void * PapyrusVM::GetObjectFromHandle(UInt64 handle, ENUM_FORM_ID formID)
{
	BSScript::IVirtualMachine		* registry =	GameVM::GetSingleton()->impl.get();
	BSScript::IObjectHandlePolicy	& policy =		registry->GetObjectHandlePolicy();

	if(handle == policy.EmptyHandle()) {
		return NULL;
	}

	return policy.GetObjectForHandle((std::uint32_t)formID, (std::size_t)handle);
}
