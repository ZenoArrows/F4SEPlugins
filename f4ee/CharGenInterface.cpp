#include "CharGenInterface.h"

#include <fstream>
#include <memory>

#include <atomic>
#include <chrono>
#include <regex>

#include <algorithm>
#include <execution>

#include "CharGenTint.h"
#include "BodyMorphInterface.h"
#include "OverlayInterface.h"
#include "SkinInterface.h"
#include "Utilities.h"

using namespace REX::W32;
#include "common/IFileStream.h"

extern bool g_bExportRace;
extern std::string g_strExportRace;
extern UInt32 g_uExportIdMin;
extern UInt32 g_uExportIdMax;

extern BodyMorphInterface g_bodyMorphInterface;
extern OverlayInterface g_overlayInterface;
extern SkinInterface	g_skinInterface;

extern bool g_bIgnoreTintPalettes;
extern bool g_bIgnoreTintTextures;
extern bool g_bIgnoreTintMasks;

extern const std::string & F4EEGetRuntimeDirectory(void);

static const char * HairGradientPalette = "actors\\character\\hair\\haircolor_lgrad_d.dds";

#define ERROR_SUCCESS         0
#define ERROR_INVALID_TOKEN   315
#define ERROR_INVALID_ADDRESS 487

std::uint32_t CharGenInterface::SavePreset(const std::string & filePath)
{
	TESDataHandler * dataHandler = TESDataHandler::GetSingleton();
	if(!dataHandler)
		return ERROR_INVALID_ADDRESS;

	Actor * actor = GetCurrentActor();
	if(!actor)
		return ERROR_INVALID_ADDRESS;

	TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
	if(!npc)
		return ERROR_INVALID_ADDRESS;

	TESRace * race = GetActorRace(actor);
	if(!race)
		return ERROR_INVALID_ADDRESS;

	SEX gender = CALL_MEMBER_FN(npc, GetSex)();

	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath.c_str());
	if (!currentFile.Create(filePath.c_str()))
	{
		return GetLastError();
	}

	Json::StyledWriter writer;
	Json::Value root;
	
	UInt32 numHeadParts = 0;
	BGSHeadPart ** headParts = NULL;
	if (CALL_MEMBER_FN(npc, HasOverlays)()) {
		numHeadParts = CALL_MEMBER_FN(npc, GetNumOverlayHeadParts)();
		headParts = CALL_MEMBER_FN(npc, GetOverlayHeadParts)();
	}
	else {
		numHeadParts = npc->numHeadParts;
		headParts = npc->headParts;
	}

	root["Gender"] = (Json::UInt)gender;

	Json::Value headPartInfo;
	for (UInt32 i = 0; i < numHeadParts; i++) // Acquire all unique parts
	{
		BGSHeadPart * headPart = headParts[i];
		if (headPart && !headPart->IsExtraPart()) {
			std::string formIdentifier = GetFormIdentifier(headPart);
			if (!formIdentifier.empty()) {
				headPartInfo.append(formIdentifier);
			}
		}
	}
	root["HeadParts"] = headPartInfo;

	auto headData = npc->headRelatedData;
	if(headData) {
		auto hairColor = headData->hairColor;
		if(hairColor) {
			root["HairColor"] = GetFormIdentifier(hairColor);
		}
	}

	root["Weight"][0] = npc->morphWeight.x;
	root["Weight"][1] = npc->morphWeight.y;
	root["Weight"][2] = npc->morphWeight.z;

	Json::Value morphSliderValues;
	Json::Value facialBoneRegionSliderValues;

	if(npc->morphRegionSliderValues)
	{
		for(int i = 0; i < (int)npc->morphRegionSliderValues->size(); i++)
		{
			root["Morphs"]["Values"][i] = (*npc->morphRegionSliderValues)[i];
		}
	}

	char keyName[256];
	if(npc->morphSliderValues)
	{
		for (const BSTTuple<std::uint32_t, float>& region : *npc->morphSliderValues)
		{
			sprintf_s(keyName, "%X", region.first);
			morphSliderValues[keyName] = region.second;
		}

		root["Morphs"]["Presets"] = morphSliderValues;
	}
		

	if(npc->facialBoneRegionSliderValues)
	{
		for (const BSTTuple<std::uint32_t, BGSCharacterMorph::Transform>& region : *npc->facialBoneRegionSliderValues)
		{
			Json::Value values;
			for(UInt32 f = 0; f < 8; f++)
			{
				values.append(reinterpret_cast<const float *>(&region.second)[f]);
			}

			sprintf_s(keyName, "%X", region.first);
			facialBoneRegionSliderValues[keyName] = values;
		}

		root["Morphs"]["Regions"] = facialBoneRegionSliderValues;
	}

	float intensity = npc->GetFacialBoneMorphIntensity();
	if(intensity != 1.0f) {
		root["Morphs"]["Intensity"] = intensity;
	}

	Json::Value tintData;

	BGSCharacterTint::Entries * tints = npc->tintingData;

	PlayerCharacter * pPC = DYNAMIC_CAST(actor, Actor, PlayerCharacter);
	if(pPC && pPC->tintingData)
		tints = pPC->tintingData;

	if(tints)
	{
		for(BGSCharacterTint::Entry * entry : tints->entriesA)
		{
			if(entry->tingingValue == 0)
				continue;

			sprintf_s(keyName, "%X", entry->idLink);

			BGSCharacterTint::EntryType type = entry->GetType();
			tintData[keyName]["Type"] = (Json::Int)type;
			tintData[keyName]["Percent"] = (Json::Int)entry->tingingValue;

			switch(type)
			{
			case BGSCharacterTint::EntryType::kPalette:
				BGSCharacterTint::PaletteEntry * palette = static_cast<BGSCharacterTint::PaletteEntry*>(entry);
				tintData[keyName]["Color"] = palette->tintingColor;
				tintData[keyName]["ColorID"] = palette->swatchID;
				break;
			}

			root["TintOrder"].append(keyName);
		}

		root["Tints"] = tintData;
	}

	Json::Value morphData;
	auto morphMap = g_bodyMorphInterface.GetMorphMap(actor, gender == SEX::kFemale ? true : false);
	if(morphMap) {
		for(auto & morph : *morphMap) {
			auto it = morph.second->find(0);
			if(it != morph.second->end()) {
				morphData[morph.first->c_str()] = it->second;
			}
		}

		root["BodyMorphs"] = morphData;
	}

	Json::Value overlayData;
	if(g_overlayInterface.ForEachOverlay(actor, gender == SEX::kFemale ? true : false, [&](SInt32 priority, const OverlayInterface::OverlayDataPtr & pOverlay)
	{
		Json::Value overlay;
		overlay["template"] = pOverlay->templateName ? pOverlay->templateName->c_str() : "";
		overlay["priority"] = (Json::Int)priority;

		if((pOverlay->flags & OverlayInterface::OverlayData::kHasTintColor) == OverlayInterface::OverlayData::kHasTintColor)
		{
			overlay["tint"].append(pOverlay->tintColor.r);
			overlay["tint"].append(pOverlay->tintColor.g);
			overlay["tint"].append(pOverlay->tintColor.b);
			overlay["tint"].append(pOverlay->tintColor.a);
		}
		if((pOverlay->flags & OverlayInterface::OverlayData::kHasOffsetUV) == OverlayInterface::OverlayData::kHasOffsetUV)
		{
			overlay["offsetUV"].append(pOverlay->offsetUV.x);
			overlay["offsetUV"].append(pOverlay->offsetUV.y);
		}
		if((pOverlay->flags & OverlayInterface::OverlayData::kHasScaleUV) == OverlayInterface::OverlayData::kHasScaleUV)
		{
			overlay["scaleUV"].append(pOverlay->offsetUV.x);
			overlay["scaleUV"].append(pOverlay->offsetUV.y);
		}
		
		overlayData.append(overlay);
	})) {
		root["Overlays"] = overlayData;
	}

	auto skin = g_skinInterface.GetSkinOverride(actor);
	if(skin.length() > 0) {
		root["Skin"] = skin.c_str();
	}
		

	std::string data = writer.write(root);
	currentFile.WriteBuf(data.c_str(), (UInt32)data.length());
	currentFile.Close();
	return ERROR_SUCCESS;
}

std::uint32_t CharGenInterface::LoadPreset(const std::string & filePath)
{
	Json::Reader reader;
	Json::Value root;

	std::ifstream in(filePath);
	if(!in) {
		return GetLastError();
	}

	if(!reader.parse(in, root)) {
		return ERROR_INVALID_TOKEN;
	}

	TESDataHandler * dataHandler = TESDataHandler::GetSingleton();
	if(!dataHandler)
		return ERROR_INVALID_ADDRESS;

	Actor * actor = GetCurrentActor();
	if(!actor)
		return ERROR_INVALID_ADDRESS;

	TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
	if(!npc)
		return ERROR_INVALID_ADDRESS;

	TESRace * race = GetActorRace(actor);
	if(!race)
		return ERROR_INVALID_ADDRESS;

	SEX gender = CALL_MEMBER_FN(npc, GetSex)();

	SEX loadedGender = SEX::kMale;
	try
	{
		loadedGender = (SEX)root["Gender"].asUInt();
	}
	catch( ... )
	{
		loadedGender = gender;
	}

	if(gender != loadedGender)
	{
		return ERROR_INVALID_TOKEN;
	}

	bool isFemale = gender == SEX::kFemale ? true : false;

	// Wipe the HeadPart list and replace it with the default race list
	auto chargenData = race->faceRelatedData[isFemale];
	if (chargenData) {
		BGSHeadPart ** headParts = npc->headParts;
		BSTArray<BGSHeadPart*> * headPartList = race->faceRelatedData[isFemale]->headParts;
		if (headParts && headPartList) {
			Heap_Free(headParts);
			npc->numHeadParts = headPartList->size();
			headParts = (BGSHeadPart **)Heap_Allocate(npc->numHeadParts * sizeof(BGSHeadPart*));
			for (UInt32 i = 0; i < headPartList->size(); i++)
				headParts[i] = (*headPartList)[i];
			npc->headParts = headParts;
		}
	}

	try
	{
		Json::Value parts = root["HeadParts"];
		for(auto & part : parts)
		{
			TESForm * form = GetFormFromIdentifier(part.asString());
			if(!form) // Not a valid form
				continue;

			BGSHeadPart * newPart = DYNAMIC_CAST(form, TESForm, BGSHeadPart);
			if(!newPart) // Not a head part type
				continue;

			npc->ChangeHeadPart(newPart);
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}
	
	try
	{
		TESForm * form = GetFormFromIdentifier(root["HairColor"].asString());
		if(form) {
			BGSColorForm * colorForm = DYNAMIC_CAST(form, TESForm, BGSColorForm);
			if(colorForm) {
				if(!npc->headRelatedData)
					npc->headRelatedData = new TESNPC::HeadRelatedData();
				npc->headRelatedData->hairColor = colorForm;
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}
	
	try
	{
		Json::Value values = root["Morphs"]["Values"];
		std::vector<float> fValues;
		for(auto & value : values)
		{
			fValues.push_back(value.asFloat());
		}

		if(!npc->morphRegionSliderValues && fValues.size() > 0)
			npc->morphRegionSliderValues = new BSTArray<float>();
		if(npc->morphRegionSliderValues) {
			npc->morphRegionSliderValues->clear();
			npc->morphRegionSliderValues->resize(5);
			UInt32 elements = (std::min<UInt32>)(5, (UInt32)fValues.size());
			for(UInt32 i = 0; i < elements; i++)
			{
				(*npc->morphRegionSliderValues)[i] = fValues.at(i);
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}
	
	try
	{
		Json::Value regions = root["Morphs"]["Regions"];
		auto members = regions.getMemberNames();

		bool bClear = true;
		if(!npc->facialBoneRegionSliderValues && members.size() > 0) {
			npc->facialBoneRegionSliderValues = new BSTHashMap<std::uint32_t, BGSCharacterMorph::Transform>();
			bClear = false;
		}
		if(npc->facialBoneRegionSliderValues) {
			if(bClear)
				npc->facialBoneRegionSliderValues->clear();
			
			for(auto key : members)
			{
				std::uint32_t keyValue = 0;
				sscanf_s(key.c_str(), "%X", &keyValue);

				RE::BSTTuple<std::uint32_t, BGSCharacterMorph::Transform> regionData;
				regionData.first = keyValue;
				for(int i = 0; i < 8; i++)
				{
					reinterpret_cast<float *>(&regionData.second)[i] = regions[key][i].asFloat();
				}
				npc->facialBoneRegionSliderValues->insert(regionData);
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}
	
	try
	{
		Json::Value presets = root["Morphs"]["Presets"];
		auto members = presets.getMemberNames();

		bool bClear = true;
		if(!npc->morphSliderValues && members.size() > 0) {
			npc->morphSliderValues = new BSTHashMap<std::uint32_t, float>();
			bClear = false;
		}
		if(npc->morphSliderValues) {
			if(bClear)
				npc->morphSliderValues->clear();

			for(auto key : members)
			{
				UInt32 keyValue = 0;
				sscanf_s(key.c_str(), "%X", &keyValue);

				BSTTuple<std::uint32_t, float> morphSet;
				morphSet.first = keyValue;
				morphSet.second = presets[key].asFloat();
				npc->morphSliderValues->insert(morphSet);
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	if(actor != PlayerCharacter::GetPlayer()) // Don't apply morph intensity to the player.
	{
		try
		{
			bool hasIntensity = root["Morphs"].isMember("Intensity");
			float intensity = hasIntensity ? root["Morphs"]["Intensity"].asFloat() : 1.0f;
			npc->SetFacialBoneMorphIntensity(intensity);
		}
		catch(const std::exception& e)
		{
			_ERROR(e.what());
		}
	}
	
	try
	{
		npc->morphWeight.x = root["Weight"][0].asFloat();
		npc->morphWeight.y = root["Weight"][1].asFloat();
		npc->morphWeight.z = root["Weight"][2].asFloat();
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	try
	{
		Json::Value tints = root["Tints"];
		auto members = tints.getMemberNames();
		bool bClear = true;
		if(!npc->tintingData && members.size() > 0) {
			npc->tintingData = new BGSCharacterTint::Entries();
			bClear = false;
		}
		if(npc->tintingData) {
			if(bClear)
				BGSCharacterTint::Entry::ClearCharacterTints(npc->tintingData);

			std::map<UInt32, BGSCharacterTint::Entry*> tintMap;
			for(auto key : members)
			{
				UInt32 keyValue = 0;
				sscanf_s(key.c_str(), "%X", &keyValue);

				BGSCharacterTint::EntryType type = (BGSCharacterTint::EntryType)tints[key]["Type"].asInt();
				if((type == BGSCharacterTint::EntryType::kPalette && g_bIgnoreTintPalettes) ||
					(type == BGSCharacterTint::EntryType::kTexture && g_bIgnoreTintTextures) ||
					(type == BGSCharacterTint::EntryType::kMask && g_bIgnoreTintMasks))
					continue;

				BGSCharacterTint::Template::Entry * templateEntry = chargenData->tintingTemplate->GetTemplateByUniqueID((std::uint16_t)keyValue); // Validate the tint index
				if(templateEntry) {
					BGSCharacterTint::Entry* newEntry = BGSCharacterTint::Entry::CreateCharacterTintEntry((keyValue << 16) | (UInt32)type);
					if(newEntry) {
						if(newEntry->GetType() == BGSCharacterTint::EntryType::kPalette) {
							BGSCharacterTint::PaletteEntry * palette = static_cast<BGSCharacterTint::PaletteEntry*>(newEntry);
							BGSCharacterTint::Template::Palette * paletteTemplate = static_cast<BGSCharacterTint::Template::Palette*>(templateEntry);
							palette->tintingColor = tints[key]["Color"].asUInt();
							SInt16 colorID = tints[key]["ColorID"].asInt();
							auto colorData = paletteTemplate->GetColorDataBySwatchID(colorID); // Validate the color index
							if(colorData)
								palette->swatchID = colorID;
							else if(!paletteTemplate->colorValues.empty())
								palette->swatchID = paletteTemplate->colorValues[0].swatchID;
							else
								palette->swatchID = 0;
						}
						newEntry->tingingValue = tints[key]["Percent"].asInt();

						tintMap.emplace(keyValue, newEntry);
					}
				}
			}

			// We have a defined ordering, find them in the map and push them in and erase them
			Json::Value tintOrder = root["TintOrder"];
			if(root.isMember("TintOrder"))
			{
				for(auto & tintEntry : tintOrder)
				{
					UInt32 keyValue = 0;
					sscanf_s(tintEntry.asCString(), "%X", &keyValue);

					auto it = tintMap.find(keyValue);
					if(it != tintMap.end()) {
						npc->tintingData->entriesA.push_back(it->second);
						tintMap.erase(it);
					}
				}

			}

			// We either have remaining tints not part of the ordering, or we didn't have an ordering at all
			for(auto & tintEntry : tintMap)
				npc->tintingData->entriesA.push_back(tintEntry.second);

			if(actor == PlayerCharacter::GetPlayer()) {
				BGSCharacterTint::Entry::CopyCharacterTints(PlayerCharacter::GetPlayer()->tintingData, npc->tintingData);
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	try
	{
		Json::Value morphData = root["BodyMorphs"];
		auto members = morphData.getMemberNames();
		if(members.size() > 0 || (root.isMember("BodyMorphs") && morphData.isNull())) {
			g_bodyMorphInterface.RemoveMorphsByKeyword(actor, isFemale, nullptr);
		}

		for(auto key : members)
		{
			float value = morphData[key].asFloat();
			g_bodyMorphInterface.SetMorph(actor, isFemale, key.c_str(), nullptr, value);
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	g_overlayInterface.RemoveAll(actor, isFemale);
	if(root.isMember("Overlays"))
	{
		Json::Value overlays = root["Overlays"];
		for(auto & overlay : overlays)
		{
			try
			{
				SInt32 priority = overlay["priority"].asInt();
				const char * templateName = overlay["template"].asCString();
				NiColorA color;
				color.r = 0.0f;
				color.g = 0.0f;
				color.b = 0.0f;
				color.a = 0.0f;
				if(overlay.isMember("tint")) {
					color.r = overlay["tint"][0].asFloat();
					color.g = overlay["tint"][1].asFloat();
					color.b = overlay["tint"][2].asFloat();
					color.a = overlay["tint"][3].asFloat();
				}
				NiPoint2 offsetUV;
				offsetUV.x = 0.0f;
				offsetUV.y = 0.0f;
				if(overlay.isMember("offsetUV")) {
					offsetUV.x = overlay["offsetUV"][0].asFloat();
					offsetUV.y = overlay["offsetUV"][1].asFloat();
				}
				NiPoint2 scaleUV;
				scaleUV.x = 1.0f;
				scaleUV.y = 1.0f;
				if(overlay.isMember("scaleUV")) {
					scaleUV.x = overlay["scaleUV"][0].asFloat();
					scaleUV.y = overlay["scaleUV"][1].asFloat();
				}

				g_overlayInterface.AddOverlay(actor, gender == SEX::kFemale ? true : false, priority, templateName, color, offsetUV, scaleUV);
			}
			catch(const std::exception& e)
			{
				_ERROR(e.what());
			}
		}
	}

	g_skinInterface.RevertOverride(actor, npc);
	g_skinInterface.RemoveSkinOverride(actor);
	if(root.isMember("Skin"))
	{
		g_skinInterface.AddSkinOverride(actor, root["Skin"].asString(), gender == SEX::kFemale ? true : false);
		g_skinInterface.ApplyOverride(actor, npc, true);
	}
	

	npc->AddChange(0x800); // Save FaceData
	npc->AddChange(0x4000); // Save weights

	return ERROR_SUCCESS;
}

void CharGenInterface::LoadHairColorMods()
{
	// Load all hair color mods
	ForEachMod([&](const ModInfo * modInfo)
	{
		std::string templatesPath = std::string("F4SE\\Plugins\\F4EE\\LUTs\\") + std::string(modInfo->filename) + "\\haircolors.json";
		LoadHairColorData(templatesPath, modInfo);
	});
}

void CharGenInterface::LoadTintTemplateMods()
{
	// Load all categories first
	ForEachMod([&](const ModInfo * modInfo)
	{
		std::string templatesPath = std::string("F4SE\\Plugins\\F4EE\\Tints\\") + std::string(modInfo->filename) + "\\categories.json";
		LoadTintCategories(templatesPath);
	});

	ForEachMod([&](const ModInfo * modInfo)
	{
		std::string templatesPath = std::string("F4SE\\Plugins\\F4EE\\Tints\\") + std::string(modInfo->filename) + "\\templates.json";
		LoadTintTemplates(templatesPath);
	});

	if(g_bExportRace)
	{
		TESRace * race = GetRaceByName(g_strExportRace);
		if(race)
		{
			std::string exportPath = "Data\\F4SE\\Plugins\\F4EE\\Exported\\Tints";
			std::string categories = F4EEGetRuntimeDirectory() + exportPath + std::string("\\categories.json");
			SaveTintCategories(race, categories);
			std::string templatesPath = F4EEGetRuntimeDirectory() + exportPath + std::string("\\templates.json");
			SaveTintTemplates(race, templatesPath);
		}
	}
}

bool CharGenInterface::LoadTintCategories(const std::string & filePath)
{
	BSResourceNiBinaryStream binaryStream(filePath.c_str());
	if(!binaryStream)
		return false;

	std::string strFile;
	BSReadAll(&binaryStream, &strFile);

	Json::Reader reader;
	Json::Value root;

	UInt32 loadedCategories = 0;

	if(!reader.parse(strFile, root)) {
		return false;
	}

	try
	{
		// traverse instances
		for(auto & item : root)
		{
			std::string raceName = item["Race"].asString();

			TESRace * race = GetRaceByName(raceName);
			if(race)
			{
				// First pass for category registration
				auto entries = item["Entries"];
				for(auto & entry : entries)
				{
					std::string type = entry["Type"].asString();
					UInt8 gender = entry["Gender"].asUInt();
					UInt32 identifier = entry["Id"].asUInt();
					switch(gender)
					{
					case 0:
					case 1:
					case 2:
						break;
					default:
						throw std::exception("Invalid gender specified");
						break;
					}

					if(_strnicmp(type.c_str(), "Category", 8) == 0)
					{
						std::shared_ptr<CharGenTintObject> pCategory = std::make_shared<CharGenTintObject>(identifier);
						if(pCategory->Parse(entry)) {
							pCategory->Apply(race, gender);
							loadedCategories++;
						}
					}
				}
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
		return false;
	}

	_MESSAGE("%s - Info - Loaded %d tint category(s).\t[%s]", __FUNCTION__, loadedCategories, filePath.c_str());
	return true;
}

bool CharGenInterface::LoadTintTemplates(const std::string & filePath)
{
	BSResourceNiBinaryStream binaryStream(filePath.c_str());
	if(!binaryStream)
		return false;

	std::string strFile;
	BSReadAll(&binaryStream, &strFile);

	Json::Reader reader;
	Json::Value root;

	UInt32 loadedTemplates = 0;

	if(!reader.parse(strFile, root)) {
		return false;
	}

	try
	{
		// traverse instances
		for(auto & item : root)
		{
			std::string raceName = item["Race"].asString();

			TESRace * race = GetRaceByName(raceName);
			if(race)
			{
				auto entries = item["Entries"];
				for(auto & entry : entries)
				{
					std::string type = entry["Type"].asString();
					UInt8 gender = entry["Gender"].asUInt();
					UInt32 identifier = entry["Id"].asUInt();

					switch(gender)
					{
					case 0:
					case 1:
					case 2:
						break;
					default:
						throw std::exception("Invalid gender specified");
						break;
					}

					if(_strnicmp(type.c_str(), "Mask", 4) == 0)
					{
						std::shared_ptr<CharGenTintMask> pMask = std::make_shared<CharGenTintMask>((UInt16)identifier);
						if(pMask->Parse(entry)) {
							pMask->Apply(race, gender);
							loadedTemplates++;
						}
					}
					else if(_strnicmp(type.c_str(), "Palette", 7) == 0)
					{
						std::shared_ptr<CharGenTintPalette> pPalette = std::make_shared<CharGenTintPalette>((UInt16)identifier);
						if(pPalette->Parse(entry)) {
							pPalette->Apply(race, gender);
							loadedTemplates++;
						}
					}
					else if(_strnicmp(type.c_str(), "TextureSet", 10) == 0)
					{
						std::shared_ptr<CharGenTintTextureSet> pTextureSet = std::make_shared<CharGenTintTextureSet>((UInt16)identifier);
						if(pTextureSet->Parse(entry)) {
							pTextureSet->Apply(race, gender);
							loadedTemplates++;
						}
					}
				}
			}
		}
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
		return false;
	}

	_MESSAGE("%s - Info - Loaded %d tint template(s).\t[%s]", __FUNCTION__, loadedTemplates, filePath.c_str());
	return true;
}


bool CharGenInterface::SaveTintCategories(const TESRace * race, const std::string & filePath)
{
	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath.c_str());
	if (!currentFile.Create(filePath.c_str()))
	{
		return false;
	}

	Json::StyledWriter writer;
	Json::Value root;

	Json::Value rootEntry;
	rootEntry["Race"] = race->formEditorID.c_str();

	// Find the last index first by going over all looking for the max
	UInt32 lastIndex = 0;
	CharGenTintObject::ForEachGender(race, 2, [&](BGSCharacterTint::Template::Groups * tints, UInt8 genderId)
	{
		for(BGSCharacterTint::Template::Group * category : tints->groups)
		{
			if(category->chargenIndex > lastIndex)
				lastIndex = category->chargenIndex;
		}
	});

	CharGenTintObject::ForEachGender(race, 2, [&](BGSCharacterTint::Template::Groups * tints, UInt8 genderId)
	{
		for(BGSCharacterTint::Template::Group * category : tints->groups)
		{
			// Correct broken categories
			if(category->chargenIndex == 0 && category->name == BSFixedString("SkinTints") && category->name != BSFixedString("FaceRegions") && category->name != BSFixedString("Brows")) {
				lastIndex++;
				category->chargenIndex = lastIndex;
			}

			Json::Value jcategory;
			jcategory["Id"] = (Json::UInt)category->chargenIndex;
			jcategory["Type"] = "Category";
			jcategory["Gender"] = genderId;
			jcategory["Name"] = category->name.c_str();

			rootEntry["Entries"].append(jcategory);
		}
	});

	root.append(rootEntry);

	std::string data = writer.write(root);
	currentFile.WriteBuf(data.c_str(), (UInt32)data.length());
	currentFile.Close();
	return true;
}

bool CharGenInterface::SaveTintTemplates(const TESRace * race, const std::string & filePath)
{
	IFileStream		currentFile;
	IFileStream::MakeAllDirs(filePath.c_str());
	if (!currentFile.Create(filePath.c_str()))
	{
		return false;
	}

	Json::StyledWriter writer;
	Json::Value root;

	Json::Value rootEntry;
	rootEntry["Race"] = race->formEditorID.c_str();

	CharGenTintObject::ForEachGender(race, 2, [&](BGSCharacterTint::Template::Groups * tints, UInt8 genderId)
	{
		for(BGSCharacterTint::Template::Group * category : tints->groups)
		{
			for(BGSCharacterTint::Template::Entry* entry : category->entries)
			{
				BGSCharacterTint::Template::Mask* mask = (BGSCharacterTint::Template::Mask*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__Mask);
				BGSCharacterTint::Template::Palette* palette = (BGSCharacterTint::Template::Palette*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__Palette);
				BGSCharacterTint::Template::TextureSet* textureSet = (BGSCharacterTint::Template::TextureSet*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__TextureSet);

				if(entry->uniqueID < g_uExportIdMin || entry->uniqueID > g_uExportIdMax)
					continue;

				if(mask)
				{
					Json::Value jentry;
					jentry["Id"] = (Json::UInt)entry->uniqueID;
					jentry["Type"] = "Mask";
					jentry["Gender"] = genderId;
					jentry["Slot"] =  CharGenTintObject::WriteSlot(*mask->slot);
					CharGenTintObject::WriteFlags(mask->flags, jentry["Flags"]);
					jentry["Name"] = mask->name.c_str();

					if(category->chargenIndex == 0 || jentry["Slot"].asString().compare("Brows") == 0)
					{
						jentry["FixedCategory"] = category->name.c_str();
					}
					else
					{
						jentry["Category"] = (Json::UInt)category->chargenIndex;
					}
					

					std::string texture = mask->maskTextureName.c_str();
					if(!texture.empty())
						jentry["Texture"] = texture;

					if(mask->blendOp != BGSCharacterTint::BlendOp::kDefault)
						jentry["BlendOp"] = CharGenTintObject::WriteBlendOp(mask->blendOp);

					rootEntry["Entries"].append(jentry);
				}
				else if(palette)
				{
					Json::Value jentry;
					jentry["Id"] = (Json::UInt)entry->uniqueID;
					jentry["Type"] = "Palette";
					jentry["Gender"] = genderId;
					jentry["Slot"] =  CharGenTintObject::WriteSlot(*palette->slot);
					CharGenTintObject::WriteFlags(palette->flags, jentry["Flags"]);
					jentry["Name"] = palette->name.c_str();
					if(category->chargenIndex == 0)
					{
						jentry["FixedCategory"] = category->name.c_str();
					}
					else
					{
						jentry["Category"] = (Json::UInt)category->chargenIndex;
					}
					jentry["Texture"] = palette->maskTextureName.c_str();
					Json::Value colors;
					for(BGSCharacterTint::Template::Palette::ColorValue colorData : palette->colorValues)
					{
						Json::Value color;
						color["Id"] = (Json::UInt)colorData.swatchID;
						color["Form"] = GetFormIdentifier(colorData.color);
						color["Alpha"] = colorData.value;
						if(colorData.blendOp != BGSCharacterTint::BlendOp::kDefault)
							color["BlendOp"] = CharGenTintObject::WriteBlendOp(colorData.blendOp);
						colors.append(color);
					}
					jentry["Colors"] = colors;
					rootEntry["Entries"].append(jentry);
				}
				else if(textureSet)
				{
					Json::Value jentry;
					jentry["Id"] = (Json::UInt)entry->uniqueID;
					jentry["Type"] = "TextureSet";
					jentry["Gender"] = genderId;
					jentry["Slot"] =  CharGenTintObject::WriteSlot(*textureSet->slot);
					CharGenTintObject::WriteFlags(textureSet->flags, jentry["Flags"]);
					jentry["Name"] = textureSet->name.c_str();
					if(category->chargenIndex == 0)
					{
						jentry["FixedCategory"] = category->name.c_str();
					}
					else
					{
						jentry["Category"] = (Json::UInt)category->chargenIndex;
					}

					std::string diffuse = textureSet->diffuse.c_str();
					std::string normal = textureSet->normal.c_str();
					std::string specular = textureSet->specular.c_str();

					if(!diffuse.empty())
						jentry["Diffuse"] = diffuse;
					if(!normal.empty())
						jentry["Normal"] = normal;
					if(!specular.empty())
						jentry["Specular"] = specular;

					if(textureSet->blendOp != BGSCharacterTint::BlendOp::kDefault)
						jentry["BlendOp"] = CharGenTintObject::WriteBlendOp(textureSet->blendOp);

					if(textureSet->defaultValue != 0.0f)
						jentry["Default"] = textureSet->defaultValue;

					rootEntry["Entries"].append(jentry);
				}
			}
		}
	});

	root.append(rootEntry);

	std::string data = writer.write(root);
	currentFile.WriteBuf(data.c_str(), (UInt32)data.length());
	currentFile.Close();
	return true;
}

Actor * CharGenInterface::GetCurrentActor()
{
	if(!BGSChargenUtils::GetSingleton())
		return nullptr;

	BGSChargenUtils * characterCreation = BGSChargenUtils::GetSingleton();
	if(!characterCreation)
		return nullptr;

	return characterCreation->targetActor;
}


void CharGenInterface::UnlockHeadParts()
{
	std::atomic<int> total;
	std::chrono::time_point<std::chrono::system_clock> start, end;

	start = std::chrono::system_clock::now();
	BSTArray<TESForm *>& forms = TESDataHandler::GetSingleton()->formArrays[std::to_underlying(ENUM_FORM_ID::kHDPT)];
	std::for_each(std::execution::par, forms.begin(), forms.end(), [&](TESForm * form)
	{
		BGSHeadPart * hdpt = (BGSHeadPart *)form;
		if(hdpt->chargenConditions) {
			hdpt->chargenConditions.head = nullptr;
			total++;
		}
	});
	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	_MESSAGE("Completed head part unlock of %d parts in %f seconds", total.load(), elapsed_seconds.count());
}

void CharGenInterface::UnlockTints()
{
	std::atomic<int> total;
	std::chrono::time_point<std::chrono::system_clock> start, end;

	start = std::chrono::system_clock::now();
	BSTArray<TESForm *>& forms = TESDataHandler::GetSingleton()->formArrays[std::to_underlying(ENUM_FORM_ID::kRACE)];
	std::for_each(std::execution::par, forms.begin(), forms.end(), [&](TESForm * form)
	{
		TESRace * race = (TESRace *)form;
		for(UInt32 i = 0; i <= 1; i++) {
			auto chargenData = race->faceRelatedData[i];
			if(chargenData) {
				auto tintData = chargenData->tintingTemplate;
				if(tintData) {
					for(auto tintCategory : tintData->groups) {
						for(auto tintEntry : tintCategory->entries) {
							if(tintEntry->chargenConditions) {
								tintEntry->chargenConditions.head = nullptr;
								total++;
							}
						}
					}
				}
			}
		}
	});
	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	_MESSAGE("Completed tint unlock of %d tints in %f seconds", total.load(), elapsed_seconds.count());
}

void CharGenInterface::ProcessHairColor(NiAVObject * node, BGSColorForm * colorForm, BSLightingShaderMaterialBase * shaderMaterial)
{
	VisitObjects(node, [&](NiPointer<NiAVObject> object)
	{
		BSTriShape * trishape = object->IsTriShape();
		if(trishape) {
			BSLightingShaderProperty * lightingShader = netimmerse_cast<BSLightingShaderProperty *, NiProperty>(trishape->properties[1].get());
			if(lightingShader) { // FIXME: We should check the GrayscaleToPalette flag here
				BSLightingShaderMaterialBase * material = static_cast<BSLightingShaderMaterialBase *>(lightingShader->material);
				if(material && material->lookupTexture) {

					std::string fullPath = material->lookupTexture->name.c_str();
					std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), ::tolower);

					fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\"); // Replace multiple slashes or forward slashes with one backslash
					fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), ""); // Remove all backslashes from the front
					fullPath = std::regex_replace(fullPath, std::regex(".*?textures\\\\"), ""); // Remove everything before and including the textures path root
					
					

					bool bNeedsCustomLUT = (colorForm->flags & 0x8000) == 0x8000;
					bool bUsingCustomLUT = IsLUTUsed(fullPath.c_str());
					bool bEligibleCustomLUT = fullPath.compare(HairGradientPalette) == 0;

					char destBuff[256];
					F4EEFixedString str;
					const char * pNewPalettePath = nullptr;

					// We don't want the custom LUT anymore
					if(bUsingCustomLUT && !bNeedsCustomLUT)
					{
						strcpy_s(destBuff, 256, "DATA\\TEXTURES\\");
						strcat_s(destBuff, 256, HairGradientPalette);
						pNewPalettePath = destBuff;
					}

					// We are eligible for a custom LUT or are already using one and we need to change texture
					if((bEligibleCustomLUT || bUsingCustomLUT) && bNeedsCustomLUT)
					{
						if(GetLUTFromColor(colorForm, str))
						{
							strcpy_s(destBuff, 256, "DATA\\TEXTURES\\");
							strcat_s(destBuff, 256, str.c_str());
							pNewPalettePath = destBuff;
						}
					}

					// Create a new TXST with the new palette path in it
					if(pNewPalettePath)
					{
						BSShaderTextureSet * textureSet = BSShaderTextureSet::CreateObject();
						BSShaderTextureSet * otherSet = (BSShaderTextureSet*)material->textureSet.get();
						for(int i = 0; i < 10; i++)
						{
							const char * path = otherSet->textureNames[i].c_str();
							if(path && path[0] != 0)
							{
								textureSet->SetTextureFilename((BSShaderProperty::TextureTypeEnum)i, otherSet->textureNames[i].c_str());
							}
						}

						//textureSet->Copy((BSShaderTextureSet*)material->textureSet.m_pObject);
						textureSet->SetTextureFilename(BSShaderProperty::TextureTypeEnum::kHeight, pNewPalettePath);
						material->textureSet = textureSet;
						material->ClearTextures();
						CALL_MEMBER_FN(lightingShader, LoadTextureSet)(0);
					}
				}
			}
		}

		return false;
	});
}

const char * CharGenInterface::ProcessEyebrowPath(TESNPC * npc)
{
	auto hairTexturePath = npc->formRace->hairColorLookupTexture.textureName.c_str();

	BGSColorForm * colorForm = npc->GetHairColor();
	if(colorForm && (colorForm->flags & 0x8000) == 0x8000) {

		std::string fullPath = hairTexturePath;
		std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), ::tolower);

		fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\"); // Replace multiple slashes or forward slashes with one backslash
		fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), ""); // Remove all backslashes from the front
		fullPath = std::regex_replace(fullPath, std::regex(".*?textures\\\\"), ""); // Remove everything before and including the textures path


		bool bEligibleCustomLUT = fullPath.compare(HairGradientPalette) == 0;
		if(bEligibleCustomLUT) {
			F4EEFixedString str;
			if(GetLUTFromColor(colorForm, str))
			{
				return str.c_str();
			}
		}
	}

	return hairTexturePath;
}

#include <unordered_set>

bool CharGenInterface::LoadHairColorData(const std::string & filePath, const ModInfo * modInfo)
{
	BSResourceNiBinaryStream binaryStream(filePath.c_str());
	if(binaryStream)
	{
		std::string strFile;
		BSReadAll(&binaryStream, &strFile);

		Json::Reader reader;
		Json::Value root;

		if(!reader.parse(strFile, root)) {
			return false;
		}

		auto colors = root["Colors"];
		if(!colors.isArray())
			return false;

		std::unordered_set<BGSColorForm*> colorForms;

		// traverse instances
		for(auto & item : colors)
		{
			try
			{
				std::vector<BGSColorForm*> entryForms;
				auto formEntry = item["Form"];

				std::function<BGSColorForm*(Json::Value & value)> GetColorForm = [&modInfo, &filePath](Json::Value & value) -> BGSColorForm *
				{
					UInt32 formId = 0;
					sscanf_s(value.asCString(), "%X", &formId);

					formId |= modInfo->GetPartialIndex() << (modInfo->IsLight() ? 16 : 24);

					TESForm * form = LookupFormByID(formId);
					if(!form) {
						_WARNING("Loading Hair Colors (%s): Could not find form %08X", filePath.c_str(), formId);
						return nullptr;
					}

					BGSColorForm * colorForm = DYNAMIC_CAST(form, TESForm, BGSColorForm);
					if(!colorForm) {
						_WARNING("Loading Hair Colors (%s): Wrong form type %08X", filePath.c_str(), formId);
						return nullptr;
					}

					return colorForm;
				};

				if(formEntry.isString())
				{
					BGSColorForm * colorForm = GetColorForm(formEntry);
					if(colorForm)
						entryForms.push_back(colorForm);
				}
				else if(formEntry.isArray())
				{
					for(auto & formItem : formEntry)
					{
						BGSColorForm * colorForm = GetColorForm(formItem);
						if(colorForm)
							entryForms.push_back(colorForm);
					}
				}
				
				std::string palettePath = item["LUT"].asString();
				UInt32 gender = item["Gender"].asUInt();
				auto raceList = item["Races"];
				
				for(BGSColorForm * colorForm : entryForms)
				{
					colorForm->flags |= 0x8000;
					colorForms.insert(colorForm);
					
					for(auto raceItem : raceList)
					{
						try
						{
							std::string raceName = raceItem.asString();
							TESRace * race = GetRaceByName(raceName);
							if(!race) {
								_WARNING("Loading Hair Colors (%s): Could not find race %s", filePath.c_str(), raceName);
								continue;
							}

							for(UInt32 i = 0; i <= 1; i++)
							{
								if((i == 0 && !(gender & 0x01)) || (i == 1 && !(gender & 0x02)))
									continue;

								auto charGenData = race->faceRelatedData[i];
								if(!charGenData)
									continue;

								if(charGenData->availableHairColors)
									charGenData->availableHairColors->push_back(colorForm);

								auto paletteStr = F4EEFixedString(palettePath.c_str());
								m_LUTMap.emplace(std::make_pair(colorForm, paletteStr));
								m_LUTs.insert(paletteStr);
							}
						}
						catch(const std::exception& e)
						{
							_ERROR("Loading Hair Colors (%s): %s", filePath.c_str(), e.what());
						}
					}
				}
			}
			catch(const std::exception& e)
			{
				_ERROR("Loading Hair Colors (%s): %s", filePath.c_str(), e.what());
			}
		}

		TESForm * form = LookupFormByID(0x1a4ae8); // ChargenOptionsSortList
		if(form) {
			BGSListForm * orderedList = DYNAMIC_CAST(form, TESForm, BGSListForm);
			if(orderedList) {
				TESForm * colorForm = nullptr;
				auto it = orderedList->arrayOfForms.begin();
				for(; it != orderedList->arrayOfForms.end(); it++)
				{
					TESForm * entry = *it;
					BGSColorForm * colorEntry = DYNAMIC_CAST(entry, TESForm, BGSColorForm);
					if(colorEntry && !colorForm) {
						colorForm = colorEntry;
					}
					else if(colorForm && !colorEntry) {
						break;
					}
				}

				for(auto & colorForm : colorForms)
				{
					orderedList->arrayOfForms.insert(it, colorForm);
				}
			}
		}
	}

	return true;
}

bool CharGenInterface::IsLUTUsed(const F4EEFixedString & str)
{
	auto it = m_LUTs.find(str);
	if(it != m_LUTs.end())
	{
		return true;
	}

	return false;
}

bool CharGenInterface::GetLUTFromColor(BGSColorForm * color, F4EEFixedString & str)
{
	auto it = m_LUTMap.find(color);
	if(it != m_LUTMap.end())
	{
		str = it->second;
		return true;
	}

	return false;
}
/*
void CharGen::SetBaseTintTextureOverride(BGSTextureSet * textureSet)
{
	m_faceTextureOverride.faceTextures = textureSet;
}

void CharGen::LockBaseTextureOverride()
{
	m_faceTextureLock.lock();
}

void CharGen::ReleaseBaseTextureOverride()
{
	m_faceTextureLock.unlock();
}

TESNPC::HeadData * CharGen::ProcessHeadData(TESNPC * npc)
{
	std::lock_guard<std::mutex> locker(m_faceTextureLock);
	if(m_faceTextureOverride.faceTextures) {
		return &m_faceTextureOverride;
	}

	return npc->headData;
}*/
