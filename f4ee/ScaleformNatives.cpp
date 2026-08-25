#include "ScaleformNatives.h"


#include "CharGenInterface.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "SkinInterface.h"
#include "StringTable.h"

#include <set>

using namespace REX::W32;
#include <common/IDirectoryIterator.h>

extern std::set<UInt32> g_presetNPCs;

extern CharGenInterface g_charGenInterface;
extern BodyMorphInterface g_bodyMorphInterface;
extern OverlayInterface g_overlayInterface;
extern SkinInterface	g_skinInterface;

extern StringTable g_stringTable;
extern bool g_bEnableBodyMorphs;
extern bool g_bEnableOverlays;
extern bool g_bEnableSkinOverrides;

extern const F4SE::TaskInterface * g_task;

template<typename T>
inline void Register(GFx::Value * dst, const char * name, T value)
{
	GFx::Value	fxValue(value);
	dst->SetMember(name, fxValue);
}

inline void RegisterUnmanagedString(GFx::Value * dst, const char * name, const char * str)
{
	GFx::Value	fxValue(str);
	dst->SetMember(name, &fxValue);
}

inline void RegisterString(GFx::Value * dst,  GFx::Movie * movie, const char * name, const char * str)
{
	GFx::Value	fxValue;
	movie->asMovieRoot->CreateString(&fxValue, str);
	dst->SetMember(name, &fxValue);
}

void F4EEScaleform_LoadPreset::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kString);

	*params.retVal = g_charGenInterface.LoadPreset(params.args[0].GetString());

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		characterCreation->needsBodyUpdate = true;
		characterCreation->needsMorphUpdate = true;

		characterCreation->needsFullUpdate = true;
	}

	RE::IMenu * menu = UI::GetSingleton()->GetMenuByMovie(params.movie).get();
	if(menu) {
		LooksMenu * looksMenu = static_cast<LooksMenu*>(menu);
		looksMenu->LoadCharacterParameters();
	}
}

void F4EEScaleform_SetCurrentBoneRegionID::Call(const Params& params)
{
	ASSERT(params.argCount >= 2);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kUInt || params.args[0].GetType() == GFx::Value::ValueType::kInt);
	ASSERT(params.args[1].GetType() == GFx::Value::ValueType::kUInt || params.args[1].GetType() == GFx::Value::ValueType::kInt);

	RE::IMenu * menu = UI::GetSingleton()->GetMenuByMovie(params.movie).get();
	if(menu) {
		LooksMenu * looksMenu = static_cast<LooksMenu*>(menu);
		looksMenu->currentRegionID = params.args[0].GetUInt();
		looksMenu->lastRegionID = params.args[1].GetUInt();
	}
}

void F4EEScaleform_SavePreset::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kString);

	*params.retVal = g_charGenInterface.SavePreset(params.args[0].GetString());
}

void F4EEScaleform_ReadPreset::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kString);

	const char	* strPath = params.args[0].GetString();
}

void F4EEScaleform_AllowTextInput::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kBoolean);
	
	ControlMap::GetSingleton()->AllowTextInput(params.args[0].GetBoolean());
}

void F4EEScaleform_GetBodySliders::Call(const Params& params)
{	
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();

			params.movie->asMovieRoot->CreateArray(params.retVal);

			std::vector<BodySliderPtr> sliders;
			g_bodyMorphInterface.ForEachSlider(std::to_underlying(gender), [&sliders](const BodySliderPtr & slider) {
				sliders.push_back(slider);
			});

			std::sort(sliders.begin(), sliders.end(), [&](const BodySliderPtr & a, const BodySliderPtr & b)
			{
				if(a->sort == b->sort)
					return std::string(a->name.c_str()) < std::string(b->name.c_str());
				else
					return a->sort < b->sort;
			});

			for(const BodySliderPtr & slider : sliders) {
				GFx::Value sliderInfo;
				params.movie->asMovieRoot->CreateObject(&sliderInfo);
				RegisterString(&sliderInfo, params.movie, "name", slider->name);
				RegisterString(&sliderInfo, params.movie, "morph", slider->morph);
				Register<double>(&sliderInfo, "value", g_bodyMorphInterface.GetMorph(actor, gender == SEX::kFemale ? true : false, slider->morph, nullptr));
				Register<double>(&sliderInfo, "minimum", slider->minimum);
				Register<double>(&sliderInfo, "maximum", slider->maximum);
				Register<double>(&sliderInfo, "interval", slider->interval);
				params.retVal->PushBack(&sliderInfo);
			}
		}
	}
}

void F4EEScaleform_SetBodyMorph::Call(const Params& params)
{
	ASSERT(params.argCount >= 2);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kString);
	ASSERT(params.args[1].GetType() == GFx::Value::ValueType::kNumber);

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			g_bodyMorphInterface.SetMorph(actor, gender == SEX::kFemale ? true : false, params.args[0].GetString(), nullptr, (float)params.args[1].GetNumber());
		}
		
	}
}

static REL::Relocation<TESNPC**> g_customizationDummy1{ REL::ID{ 4799415 } };
static REL::Relocation<TESNPC**> g_customizationDummy2{ REL::ID{ 4799413 } };

void F4EEScaleform_CloneBodyMorphs::Call(const Params& params)
{
	bool bVerify = true;
	if(params.argCount >= 1) {
		bVerify = params.args[0].GetBoolean(); // Used to verify whether the target is the starting character's dummy actor
	}
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);

		if(actor != PlayerCharacter::GetPlayer() && ((bVerify && (npc == (*g_customizationDummy1) || npc == (*g_customizationDummy2))) || !bVerify)) {
			if(g_bEnableBodyMorphs) {
				g_bodyMorphInterface.CloneMorphs(actor, PlayerCharacter::GetPlayer());
				g_bodyMorphInterface.UpdateMorphs(PlayerCharacter::GetPlayer());
			}
		}
	}
}

void F4EEScaleform_UpdateBodyMorphs::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation && g_bEnableBodyMorphs) {
		g_bodyMorphInterface.UpdateMorphs(characterCreation->targetActor);
	}
}

#include <functional>

void ReadFileDirectory(const char * lpFolder, const char ** lpFilePattern, UInt32 numPatterns, bool recursive, std::function<void(const char*, WIN32_FIND_DATAA *, bool, DateTime)> file)
{
	if(recursive)
	{
		// first we are going to process any subdirectories
		IDirectoryIterator it(lpFolder, "*");
		while (!it.Done())
		{
			if (it.Get()->fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// found a subdirectory; recurse into it
				if (it.Get()->fileName[0] == '.')
					continue;

				std::string szFullPattern = it.GetFullPath();
				file(szFullPattern.c_str(), it.Get(), true, it.GetModifiedTime());
			}
		}
	}
	// now we are going to look for the matching files
	for (UInt32 i = 0; i < numPatterns; i++)
	{
		IDirectoryIterator it(lpFolder, lpFilePattern[i]);
		while (!it.Done())
		{
			if (!(it.Get()->fileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				// found a file; do something with it
				std::string szFullPattern = it.GetFullPath();
				file(szFullPattern.c_str(), it.Get(), false, it.GetModifiedTime());
			}
		}
	}
}

void F4EEScaleform_GetExternalFiles::Call(const Params& params)
{
	ASSERT(params.argCount >= 3);
	ASSERT(params.args[0].GetType() == GFx::Value::ValueType::kString);
	ASSERT(params.args[1].GetType() == GFx::Value::ValueType::kArray);
	ASSERT(params.args[2].GetType() == GFx::Value::ValueType::kBoolean);

	const char * path = params.args[0].GetString();
	
	UInt32 numPatterns = params.args[1].GetArraySize();

	const char ** patterns = (const char **)Memory::Alloc(numPatterns * sizeof(const char*));
	for (UInt32 i = 0; i < numPatterns; i++) {
		GFx::Value str;
		params.args[1].GetElement(i, &str);
		patterns[i] = str.GetString();
	}

	params.movie->CreateArray(params.retVal);

	ReadFileDirectory(path, patterns, numPatterns, params.args[2].GetBoolean(), [params](const char* dirPath, WIN32_FIND_DATAA * fileData, bool dir, DateTime mtime)
	{
		GFx::Value fileInfo;
		params.movie->CreateObject(&fileInfo);
		RegisterString(&fileInfo, params.movie, "path", dirPath);
		RegisterString(&fileInfo, params.movie, "name", fileData->fileName);
		Register<std::uint32_t>(&fileInfo, "size", fileData->fileSizeLo);
		GFx::Value date;
		GFx::Value members[7];
		members[0] = mtime.GetYear();
		members[1] = mtime.GetMonth() - 1; // Flash Month is 0-11, System time is 1-12
		members[2] = mtime.GetDay();
		members[3] = mtime.GetHours();
		members[4] = mtime.GetMinutes();
		members[5] = mtime.GetSeconds();
		members[6] = mtime.GetMS();
		params.movie->CreateObject(&date, "Date", members, 7);
		fileInfo.SetMember("lastModified", &date);
		Register<bool>(&fileInfo, "directory", dir);
		params.retVal->PushBack(&fileInfo);
	});

	Memory::Free(patterns);
}



void F4EEScaleform_GetOverlays::Call(const Params& params)
{	
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			params.movie->CreateArray(params.retVal);

			g_overlayInterface.ForEachOverlay(actor, isFemale, [&](SInt32 priority, const OverlayInterface::OverlayDataPtr & slider) {
				GFx::Value sliderInfo;
				params.movie->CreateObject(&sliderInfo);
				Register<std::int32_t>(&sliderInfo, "priority", priority);
				Register<std::uint32_t>(&sliderInfo, "uid", slider->uid);
				RegisterString(&sliderInfo, params.movie, "id", slider->templateName->c_str());
				Register<double>(&sliderInfo, "red", slider->tintColor.r);
				Register<double>(&sliderInfo, "green", slider->tintColor.g);
				Register<double>(&sliderInfo, "blue", slider->tintColor.b);
				Register<double>(&sliderInfo, "alpha", slider->tintColor.a);
				Register<double>(&sliderInfo, "offsetU", slider->offsetUV.x);
				Register<double>(&sliderInfo, "offsetV", slider->offsetUV.y);
				Register<double>(&sliderInfo, "scaleU", slider->scaleUV.x);
				Register<double>(&sliderInfo, "scaleV", slider->scaleUV.y);

				auto pTemplate = g_overlayInterface.GetTemplateByName(isFemale, *slider->templateName);
				if(pTemplate) {
					RegisterString(&sliderInfo, params.movie, "name", pTemplate->displayName);
					Register<bool>(&sliderInfo, "playable", pTemplate->playable);
				}

				params.retVal->PushBack(&sliderInfo);
			});
		}
	}
}

void F4EEScaleform_GetOverlayTemplates::Call(const Params& params)
{	
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			params.movie->CreateArray(params.retVal);

			std::vector<std::pair<F4EEFixedString, OverlayInterface::OverlayTemplatePtr>> overlays;
			g_overlayInterface.ForEachOverlayTemplate(gender == SEX::kFemale ? true : false, [&overlays](const F4EEFixedString & name, const OverlayInterface::OverlayTemplatePtr & slider) {
				overlays.push_back(std::make_pair(name, slider));
			});

			std::sort(overlays.begin(), overlays.end(), [&](const std::pair<F4EEFixedString, OverlayInterface::OverlayTemplatePtr> & a, const std::pair<F4EEFixedString, OverlayInterface::OverlayTemplatePtr> & b)
			{
				if(a.second->sort == b.second->sort)
					return std::string(a.second->displayName.c_str()) < std::string(b.second->displayName.c_str());
				else
					return a.second->sort < b.second->sort;
			});

			for(auto & slider : overlays)
			{
				if(slider.second->playable) {
					GFx::Value sliderInfo;
					params.movie->CreateObject(&sliderInfo);
					RegisterString(&sliderInfo, params.movie, "id", slider.first);
					RegisterString(&sliderInfo, params.movie, "name", slider.second->displayName.c_str());
					Register<bool>(&sliderInfo, "transformable", slider.second->transformable);
					Register<bool>(&sliderInfo, "tintable", slider.second->tintable);
					params.retVal->PushBack(&sliderInfo);
				}
			}
		}
	}
}



void F4EEScaleform_CreateOverlay::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			SInt32 priority = params.args[0].GetInt();
			const char * templateName = params.args[1].GetString();

			NiColorA color;
			color.r = 0.0f;
			color.g = 0.0f;
			color.b = 0.0f;
			color.a = 0.0f;
			NiPoint2 offsetUV;
			offsetUV.x = 0.0f;
			offsetUV.y = 0.0f;
			NiPoint2 scaleUV;
			scaleUV.x = 1.0f;
			scaleUV.y = 1.0f;

			std::uint32_t uid = g_overlayInterface.AddOverlay(actor, isFemale, priority, templateName, color, offsetUV, scaleUV);
			*params.retVal = uid;
		}
	}
}

void F4EEScaleform_DeleteOverlay::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			UInt32 uid = params.args[0].GetUInt();
			bool result = g_overlayInterface.RemoveOverlay(actor, isFemale, uid);
			*params.retVal = result;
		}
	}
}

void F4EEScaleform_SetOverlayData::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			UInt32 uid = params.args[0].GetUInt();

			bool setParam = false;
			auto pOverlay = g_overlayInterface.GetActorOverlayByUID(actor, isFemale, uid);
			if(pOverlay.second) {
				GFx::Value memberData;
				if(params.args[1].HasMember("template")) {
					params.args[1].GetMember("template", &memberData);
					pOverlay.second->templateName = g_stringTable.GetString(memberData.GetString());
					setParam = true;
				}
				if(params.args[1].HasMember("offsetU")) {
					params.args[1].GetMember("offsetU", &memberData);
					pOverlay.second->offsetUV.x = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("offsetV")) {
					params.args[1].GetMember("offsetV", &memberData);
					pOverlay.second->offsetUV.y = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("scaleU")) {
					params.args[1].GetMember("scaleU", &memberData);
					pOverlay.second->scaleUV.x = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("scaleV")) {
					params.args[1].GetMember("scaleV", &memberData);
					pOverlay.second->scaleUV.y = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("red")) {
					params.args[1].GetMember("red", &memberData);
					pOverlay.second->tintColor.r = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("green")) {
					params.args[1].GetMember("green", &memberData);
					pOverlay.second->tintColor.g = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("blue")) {
					params.args[1].GetMember("blue", &memberData);
					pOverlay.second->tintColor.b = (float)memberData.GetNumber();
					setParam = true;
				}
				if(params.args[1].HasMember("alpha")) {
					params.args[1].GetMember("alpha", &memberData);
					pOverlay.second->tintColor.a = (float)memberData.GetNumber();
					setParam = true;
				}

				pOverlay.second->UpdateFlags();
			}

			*params.retVal = setParam;
		}
	}
}

void F4EEScaleform_ReorderOverlay::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			UInt32 uid = params.args[0].GetUInt();
			SInt32 priority = params.args[1].GetInt();

			bool result = g_overlayInterface.ReorderOverlay(actor, isFemale, uid, priority);
			*params.retVal = result;
		}
	}
}

void F4EEScaleform_UpdateOverlays::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation && g_bEnableOverlays) {
		g_overlayInterface.UpdateOverlays(characterCreation->targetActor);
	}
}

void F4EEScaleform_CloneOverlays::Call(const Params& params)
{
	bool bVerify = true;
	if(params.argCount >= 1) {
		bVerify = params.args[0].GetBoolean(); // Used to verify whether the target is the starting character's dummy actor
	}
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);

		if(actor != PlayerCharacter::GetPlayer() && ((bVerify && (npc == (*g_customizationDummy1) || npc == (*g_customizationDummy2))) || !bVerify)) {
			if(g_bEnableOverlays) {
				g_overlayInterface.CloneOverlays(actor, PlayerCharacter::GetPlayer());
				g_overlayInterface.UpdateOverlays(actor);
			}
		}
	}
}

void F4EEScaleform_GetEquippedItems::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;

		std::unordered_map<TESBoundObject*, std::pair<SInt32, UInt16>> stackList;
		auto inventory = actor->inventoryList;
		if(inventory)
		{
			inventory->rwLock.lock_read();

			for(BGSInventoryItem& item : inventory->data)
			{
				SInt32 s = 0;
				item.stackData->Visit([&](BGSInventoryItem::Stack * stack)
				{
					if(stack->IsEquipped()) {
						stackList.emplace(item.object, std::make_pair(s, (stack->flags & BGSInventoryItem::Stack::Flag::kSlotMask).underlying()));
					}
					s++;
					return true;
				});
			}
			inventory->rwLock.unlock_read();
		}
		if(!stackList.empty())
		{
			params.movie->CreateArray(params.retVal);

			for(auto & stackItem : stackList)
			{
				GFx::Value equippedItem;
				params.movie->CreateObject(&equippedItem);
				Register<std::uint32_t>(&equippedItem, "formId", stackItem.first->formID);
				Register<std::uint32_t>(&equippedItem, "stackIndex", stackItem.second.first);
				Register<std::uint32_t>(&equippedItem, "flags", stackItem.second.second);
				params.retVal->PushBack(&equippedItem);
			}	
		}
		else
			*params.retVal = nullptr;
	}
}

void F4EEScaleform_UnequipItems::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;

		// Convert the GFX data to a stack mapping
		std::unordered_map<TESForm*, std::pair<SInt32, UInt8>> stackList;
		UInt32 numItems = params.args[0].GetArraySize();
		for (UInt32 i = 0; i < numItems; i++) {
			GFx::Value element, formId, stackIndex, flags;
			params.args[0].GetElement(i, &element);

			element.GetMember("formId", &formId);
			element.GetMember("stackIndex", &stackIndex);
			element.GetMember("flags", &flags);			

			TESForm * form = LookupFormByID(formId.GetUInt());
			if(!form)
				continue;

			stackList.emplace(form, std::make_pair(stackIndex.GetUInt(), flags.GetUInt()));
		}

		// Traverse the inventory to unequip the specific stacks
		auto inventory = actor->inventoryList;
		if(inventory)
		{
			inventory->rwLock.lock_write();

			for(BGSInventoryItem& item : inventory->data)
			{
				SInt32 s = 0;
				auto it = stackList.find(item.object);
				if(it != stackList.end())
				{
					item.stackData->Visit([&](BGSInventoryItem::Stack * stack)
					{
						if(it->second.first == s) {
							stack->flags &= ~BGSInventoryItem::Stack::Flag::kSlotMask;
							return false;
						}

						s++;
						return true;
					});
				}
			}

			inventory->rwLock.unlock_write();
		}

		if(!stackList.empty()) {
			params.movie->CreateArray(params.retVal);

			for(auto & stackItem : stackList)
			{
				GFx::Value equippedItem;
				params.movie->CreateObject(&equippedItem);
				Register<std::uint32_t>(&equippedItem, "formId", stackItem.first->formID);
				Register<std::uint32_t>(&equippedItem, "stackIndex", stackItem.second.first);
				Register<std::uint32_t>(&equippedItem, "flags", stackItem.second.second);
				params.retVal->PushBack(&equippedItem);
			}

			g_task->AddTask(new F4EEBodyGenUpdate(actor, false));
		}
		else
			*params.retVal = nullptr;
	}
}

void F4EEScaleform_EquipItems::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;

		// Convert the GFX data
		std::unordered_map<TESForm*, std::pair<SInt32, UInt8>> stackList;
		UInt32 numItems = params.args[0].GetArraySize();
		for (UInt32 i = 0; i < numItems; i++) {
			GFx::Value element, formId, stackIndex, flags;
			params.args[0].GetElement(i, &element);
			
			element.GetMember("formId", &formId);
			element.GetMember("stackIndex", &stackIndex);
			element.GetMember("flags", &flags);			

			TESForm * form = LookupFormByID(formId.GetUInt());
			if(!form)
				continue;

			stackList.emplace(form, std::make_pair(stackIndex.GetUInt(), flags.GetUInt()));
		}

		// Re-equip the specified stacks
		auto inventory = actor->inventoryList;
		if(inventory)
		{
			inventory->rwLock.lock_write();

			for(BGSInventoryItem& item : inventory->data)
			{
				SInt32 s = 0;
				auto it = stackList.find(item.object);
				if(it != stackList.end())
				{
					item.stackData->Visit([&](BGSInventoryItem::Stack * stack)
					{
						if(it->second.first == s) {
							stack->flags |= (BGSInventoryItem::Stack::Flag)it->second.second & BGSInventoryItem::Stack::Flag::kSlotMask;
							return false;
						}

						s++;
						return true;
					});
				}
			}

			inventory->rwLock.unlock_write();
		}

		if(!stackList.empty()) {
			g_task->AddTask(new F4EEBodyGenUpdate(actor, false));
			*params.retVal = true;
		}
		else
			*params.retVal = false;
	}
}

void F4EEScaleform_GetSkinOverrides::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;

		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();

			params.movie->CreateArray(params.retVal);

			std::vector<std::pair<F4EEFixedString, SkinTemplatePtr>> skins;

			g_skinInterface.ForEachSkinTemplate([&](const F4EEFixedString & name, const SkinTemplatePtr & pTemplate) {
				if(pTemplate->gender == 2 || pTemplate->gender == std::to_underlying(gender))
				{
					skins.push_back(std::make_pair(name, pTemplate));
				}
			});

			std::sort(skins.begin(), skins.end(), [&](const std::pair<F4EEFixedString, SkinTemplatePtr> & a, const std::pair<F4EEFixedString, SkinTemplatePtr> & b)
			{
				if(a.second->sort == b.second->sort)
					return std::string(a.second->name.c_str()) < std::string(b.second->name.c_str());
				else
					return a.second->sort < b.second->sort;
			});

			for(auto & skin : skins)
			{
				GFx::Value templateEntry;
				params.movie->CreateObject(&templateEntry);
				RegisterString(&templateEntry, params.movie, "id", skin.first.c_str());
				RegisterString(&templateEntry, params.movie, "name", skin.second->name.c_str());
				params.retVal->PushBack(&templateEntry);
			}
		}
	}
}

void F4EEScaleform_GetSkinOverride::Call(const Params& params)
{
	F4EEFixedString res;
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;

		res = g_skinInterface.GetSkinOverride(actor);
	}

	params.movie->asMovieRoot->CreateString(params.retVal, res.c_str());
}

void F4EEScaleform_SetSkinOverride::Call(const Params& params)
{
	bool res = false;
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(npc) {
			SEX gender = CALL_MEMBER_FN(npc, GetSex)();
			bool isFemale = gender == SEX::kFemale ? true : false;

			std::string skinOverride = params.args[0].GetString();
			if(skinOverride.length() == 0)
				res = g_skinInterface.RemoveSkinOverride(actor);
			else
				res = g_skinInterface.AddSkinOverride(actor, skinOverride, isFemale);
		}
	}
	*params.retVal = res;
}

void F4EEScaleform_UpdateSkinOverride::Call(const Params& params)
{
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation && g_bEnableSkinOverrides) {
		g_skinInterface.UpdateSkinOverride(characterCreation->targetActor, true);
	}
}

void F4EEScaleform_CloneSkinOverride::Call(const Params& params)
{
	bool bVerify = true;
	if(params.argCount >= 1) {
		bVerify = params.args[0].GetBoolean(); // Used to verify whether the target is the starting character's dummy actor
	}
	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation) {
		Actor * actor = characterCreation->targetActor;
		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);

		if(actor != PlayerCharacter::GetPlayer() && ((bVerify && (npc == (*g_customizationDummy1) || npc == (*g_customizationDummy2))) || !bVerify)) {
			if(g_bEnableSkinOverrides) {
				g_skinInterface.CloneSkinOverride(actor, PlayerCharacter::GetPlayer());
				g_skinInterface.UpdateSkinOverride(actor, true);
			}
		}
	}
}


void F4EEScaleform_GetSkinColor::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);

	UInt32 currentIndex = params.args[0].GetUInt();

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation)
	{
		TESNPC * npc = characterCreation->targetNPC;
		if(npc)
		{
			auto skinTemplate = characterCreation->skinToneCustomizationPalette;
			if(skinTemplate) {
				const BGSCharacterTint::Template::Palette::ColorValue * colorData = nullptr;
				if(currentIndex < skinTemplate->colorValues.size()) {
					colorData = &skinTemplate->colorValues[currentIndex];
				}

				BGSCharacterTint::Entries * charTints = nullptr;
				if(characterCreation->targetActor == PlayerCharacter::GetPlayer())
					charTints = PlayerCharacter::GetPlayer()->tintingData;
				else
					charTints = npc->tintingData;

				BGSCharacterTint::PaletteEntry * skinEntry = nullptr;
				if(charTints) {
					for(BGSCharacterTint::Entry * tintEntry : charTints->entriesA)
					{
						if(tintEntry->idLink == skinTemplate->uniqueID) {
							skinEntry = (BGSCharacterTint::PaletteEntry *)Runtime_DynamicCast(tintEntry, RTTI::BGSCharacterTint__Entry, RTTI::BGSCharacterTint__PaletteEntry);
							break;
						}
					}
				}

				double red = 0.0, green = 0.0, blue = 0.0, alpha = 100.0;
				if(skinEntry) {
					red = (double)skinEntry->tintingChannels.r / 255.0;
					green = (double)skinEntry->tintingChannels.g / 255.0;
					blue = (double)skinEntry->tintingChannels.b / 255.0;
					alpha = (double)skinEntry->tingingValue / 100.0;
				} else if(colorData) {
					BGSColorForm * colorForm = colorData->color;
					if(colorForm) {
						red = (double)colorForm->channels.r / 255.0;
						green = (double)colorForm->channels.g / 255.0;
						blue = (double)colorForm->channels.b / 255.0;
					}
					alpha = colorData->value;
				}

				params.movie->CreateObject(params.retVal);
				Register<double>(params.retVal, "red", red);
				Register<double>(params.retVal, "green", green);
				Register<double>(params.retVal, "blue", blue);
				Register<double>(params.retVal, "alpha", alpha);
			}
		}
	}
}


void F4EEScaleform_SetSkinColor::Call(const Params& params)
{
	ASSERT(params.argCount >= 1);

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation)
	{
		TESNPC * npc = characterCreation->targetNPC;
		if(npc)
		{
			auto skinTemplate = characterCreation->skinToneCustomizationPalette;
			if(skinTemplate) {
				BGSCharacterTint::Entries * charTints = nullptr;
				if(characterCreation->targetActor == PlayerCharacter::GetPlayer())
					charTints = PlayerCharacter::GetPlayer()->tintingData;
				else
					charTints = npc->tintingData;

				BGSCharacterTint::PaletteEntry * skinEntry = nullptr;
				if(charTints) {
					for(BGSCharacterTint::Entry * tintEntry : charTints->entriesA)
					{
						if(tintEntry->idLink == skinTemplate->uniqueID) {
							skinEntry = (BGSCharacterTint::PaletteEntry *)Runtime_DynamicCast(tintEntry, RTTI::BGSCharacterTint__Entry, RTTI::BGSCharacterTint__PaletteEntry);
							break;
						}
					}

					if(!skinEntry) {
						skinEntry = static_cast<BGSCharacterTint::PaletteEntry *>(BGSCharacterTint::Entry::CreateCharacterTintEntry(((UInt32)skinTemplate->uniqueID << 16) | std::to_underlying(BGSCharacterTint::EntryType::kPalette)));
						charTints->entriesA.push_back(skinEntry);
					}
				}

				

				if(skinEntry) {
					GFx::Value red, green, blue, alpha;
					if(params.args[0].HasMember("red")) {
						params.args[0].GetMember("red", &red);

						skinEntry->tintingChannels.r = (UInt8)std::max(0.0, std::min(red.GetNumber() * 255.0, 255.0));
					}
					if(params.args[0].HasMember("green")) {
						params.args[0].GetMember("green", &green);

						skinEntry->tintingChannels.g = (UInt8)std::max(0.0, std::min(green.GetNumber() * 255.0, 255.0));
					}
					if(params.args[0].HasMember("blue")) {
						params.args[0].GetMember("blue", &blue);

						skinEntry->tintingChannels.b = (UInt8)std::max(0.0, std::min(blue.GetNumber() * 255.0, 255.0));
					}
					if(params.args[0].HasMember("alpha")) {
						params.args[0].GetMember("alpha", &alpha);

						skinEntry->tingingValue = (UInt8)std::max(0.0, std::min(alpha.GetNumber() * 100.0, 100.0));
					}

					npc->AddChange(0x800);

					characterCreation->needsMorphUpdate = true;
					characterCreation->needsFullUpdate = true;
				}
			}
		}
	}
}

void F4EEScaleform_GetExtraColor::Call(const Params& params)
{
	ASSERT(params.argCount >= 2);

	UInt32 extraGroup = params.args[0].GetUInt();
	UInt32 selectedExtra = params.args[1].GetUInt();

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation)
	{
		TESNPC * npc = characterCreation->targetNPC;
		if(npc)
		{
			BSTArray<const BGSCharacterTint::Template::Entry*> * templates = characterCreation->faceExtraEntries[extraGroup];

			const BGSCharacterTint::Template::Entry* pTemplate = nullptr;
			if(templates)
				pTemplate = (*templates)[selectedExtra];

			const BGSCharacterTint::Template::Palette* pPaletteTemplate = (const BGSCharacterTint::Template::Palette*)Runtime_DynamicCast(pTemplate, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__Palette);
			if(pPaletteTemplate)
			{
				const BGSCharacterTint::Template::Palette::ColorValue * colorData = pPaletteTemplate->colorValues.size() > 1 ? &pPaletteTemplate->colorValues[1] : nullptr;

				BGSCharacterTint::Entries * charTints = nullptr;
				if(characterCreation->targetActor == PlayerCharacter::GetPlayer())
					charTints = PlayerCharacter::GetPlayer()->tintingData;
				else
					charTints = npc->tintingData;

				BGSCharacterTint::PaletteEntry * extraEntry = nullptr;
				if(charTints) {
					for(BGSCharacterTint::Entry * tintEntry : charTints->entriesA)
					{
						if(tintEntry->idLink == pPaletteTemplate->uniqueID) {
							extraEntry = (BGSCharacterTint::PaletteEntry *)Runtime_DynamicCast(tintEntry, RTTI::BGSCharacterTint__Entry, RTTI::BGSCharacterTint__PaletteEntry);
							break;
						}
					}
				}

				double red = 0.0, green = 0.0, blue = 0.0, alpha = 100.0;
				if(extraEntry) {
					red = (double)extraEntry->tintingChannels.r / 255.0;
					green = (double)extraEntry->tintingChannels.g / 255.0;
					blue = (double)extraEntry->tintingChannels.b / 255.0;
					alpha = (double)extraEntry->tingingValue / 100.0;
				} else if(colorData) {
					BGSColorForm * colorForm = colorData->color;
					if(colorForm) {
						red = (double)colorForm->channels.r / 255.0;
						green = (double)colorForm->channels.g / 255.0;
						blue = (double)colorForm->channels.b / 255.0;
					}
					alpha = colorData->value;
				}

				params.movie->CreateObject(params.retVal);
				Register<double>(params.retVal, "red", red);
				Register<double>(params.retVal, "green", green);
				Register<double>(params.retVal, "blue", blue);
				Register<double>(params.retVal, "alpha", alpha);
			}
		}
	}
}

void F4EEScaleform_SetExtraColor::Call(const Params& params)
{
	ASSERT(params.argCount >= 2);

	UInt32 extraGroup = params.args[0].GetUInt();
	UInt32 selectedExtra = params.args[1].GetUInt();

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(characterCreation)
	{
		TESNPC * npc = characterCreation->targetNPC;
		if(npc)
		{
			auto pTemplate = (*characterCreation->faceExtraEntries[extraGroup])[selectedExtra];
			if(pTemplate)
			{
				BGSCharacterTint::Entries * charTints = nullptr;
				if(characterCreation->targetActor == PlayerCharacter::GetPlayer())
					charTints = PlayerCharacter::GetPlayer()->tintingData;
				else
					charTints = npc->tintingData;

				BGSCharacterTint::PaletteEntry * extraEntry = nullptr;
				if(charTints) {
					for(BGSCharacterTint::Entry * tintEntry : charTints->entriesA)
					{
						if(tintEntry->idLink == pTemplate->uniqueID) {
							extraEntry = (BGSCharacterTint::PaletteEntry *)Runtime_DynamicCast(tintEntry, RTTI::BGSCharacterTint__Entry, RTTI::BGSCharacterTint__PaletteEntry);
							break;
						}
					}

					if(!extraEntry) {
						extraEntry = static_cast<BGSCharacterTint::PaletteEntry *>(BGSCharacterTint::Entry::CreateCharacterTintEntry(((UInt32)pTemplate->uniqueID << 16) | std::to_underlying(BGSCharacterTint::EntryType::kPalette)));
						charTints->entriesA.push_back(extraEntry);
					}
				}

				if(extraEntry) {
					GFx::Value red, green, blue, alpha;
					if(params.args[2].HasMember("red")) {
						params.args[2].GetMember("red", &red);

						extraEntry->tintingChannels.r = (UInt8)std::max(0.0, std::min(red.GetNumber() * 255.0, 255.0));
					}
					if(params.args[2].HasMember("green")) {
						params.args[2].GetMember("green", &green);

						extraEntry->tintingChannels.g = (UInt8)std::max(0.0, std::min(green.GetNumber() * 255.0, 255.0));
					}
					if(params.args[2].HasMember("blue")) {
						params.args[2].GetMember("blue", &blue);

						extraEntry->tintingChannels.b = (UInt8)std::max(0.0, std::min(blue.GetNumber() * 255.0, 255.0));
					}
					if(params.args[2].HasMember("alpha")) {
						params.args[2].GetMember("alpha", &alpha);

						extraEntry->tingingValue = (UInt8)std::max(0.0, std::min(alpha.GetNumber() * 100.0, 100.0));
					}

					npc->AddChange(0x800);

					characterCreation->needsMorphUpdate = true;
					characterCreation->needsFullUpdate = true;
				}
			}
		}
	}
}
