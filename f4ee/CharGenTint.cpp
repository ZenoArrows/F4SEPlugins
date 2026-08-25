#include "CharGenTint.h"
#include "CharGenInterface.h"
#include "Utilities.h"

#include <set>

std::map<std::string, UInt8> g_flagMap;
std::map<std::string, BGSCharacterTint::EntrySlot> g_slotMap;
std::map<std::string, BGSCharacterTint::BlendOp> g_blendMap;

void InitFlagMap()
{
	g_flagMap.emplace(std::make_pair("OnOff", (UInt8)BGSCharacterTint::Template::Entry::kOnOffOnly));
	g_flagMap.emplace(std::make_pair("ChargenDetail", (UInt8)BGSCharacterTint::Template::Entry::kIsChargenDetaul));
	g_flagMap.emplace(std::make_pair("TakesSkinTone", (UInt8)BGSCharacterTint::Template::Entry::kTakesSkinTone));
}

void InitSlotMap()
{
	g_slotMap.emplace(std::make_pair("ForeheadMask", BGSCharacterTint::EntrySlot::kForeheadMask));
	g_slotMap.emplace(std::make_pair("EyesMask", BGSCharacterTint::EntrySlot::kEyesMask));
	g_slotMap.emplace(std::make_pair("NoseMask", BGSCharacterTint::EntrySlot::kNoseMask));
	g_slotMap.emplace(std::make_pair("EarsMask", BGSCharacterTint::EntrySlot::kEarsMask));
	g_slotMap.emplace(std::make_pair("CheeksMask", BGSCharacterTint::EntrySlot::kCheeksMask));
	g_slotMap.emplace(std::make_pair("MouthMask", BGSCharacterTint::EntrySlot::kMouthMask));
	g_slotMap.emplace(std::make_pair("NeckMask", BGSCharacterTint::EntrySlot::kNeckMask));
	g_slotMap.emplace(std::make_pair("LipColor", BGSCharacterTint::EntrySlot::kLipColor));
	g_slotMap.emplace(std::make_pair("CheekColor", BGSCharacterTint::EntrySlot::kCheekColor));
	g_slotMap.emplace(std::make_pair("Eyeliner", BGSCharacterTint::EntrySlot::kEyeliner));
	g_slotMap.emplace(std::make_pair("EyeSocketUpper", BGSCharacterTint::EntrySlot::kEyeSocketUpper));
	g_slotMap.emplace(std::make_pair("EyeSocketLower", BGSCharacterTint::EntrySlot::kEyeSocketLower));
	g_slotMap.emplace(std::make_pair("SkinTone", BGSCharacterTint::EntrySlot::kSkinTone));
	g_slotMap.emplace(std::make_pair("Paint", BGSCharacterTint::EntrySlot::kPaint));
	g_slotMap.emplace(std::make_pair("LaughLines", BGSCharacterTint::EntrySlot::kLaughLines));
	g_slotMap.emplace(std::make_pair("CheekColorLower", BGSCharacterTint::EntrySlot::kCheekColorLower));
	g_slotMap.emplace(std::make_pair("Nose", BGSCharacterTint::EntrySlot::kNose));
	g_slotMap.emplace(std::make_pair("Chin", BGSCharacterTint::EntrySlot::kChin));
	g_slotMap.emplace(std::make_pair("Neck", BGSCharacterTint::EntrySlot::kNeck));
	g_slotMap.emplace(std::make_pair("Forehead", BGSCharacterTint::EntrySlot::kForehead));
	g_slotMap.emplace(std::make_pair("Dirt", BGSCharacterTint::EntrySlot::kDirt));
	g_slotMap.emplace(std::make_pair("Scar", BGSCharacterTint::EntrySlot::kScars));
	g_slotMap.emplace(std::make_pair("FaceDetail", BGSCharacterTint::EntrySlot::kFaceDetail));
	g_slotMap.emplace(std::make_pair("Brows", BGSCharacterTint::EntrySlot::kBrow));
	g_slotMap.emplace(std::make_pair("Wrinkles", BGSCharacterTint::EntrySlot::kWrinkles));
	g_slotMap.emplace(std::make_pair("Beard", BGSCharacterTint::EntrySlot::kBeard));
}

void InitBlendMap()
{
	g_blendMap.emplace(std::make_pair("Default", BGSCharacterTint::BlendOp::kDefault));
	g_blendMap.emplace(std::make_pair("Multiply", BGSCharacterTint::BlendOp::kMultiply));
	g_blendMap.emplace(std::make_pair("Overlay", BGSCharacterTint::BlendOp::kOverlay));
	g_blendMap.emplace(std::make_pair("SoftLight", BGSCharacterTint::BlendOp::kSoftLight));
	g_blendMap.emplace(std::make_pair("HardLight", BGSCharacterTint::BlendOp::kHardLight));
}

BGSCharacterTint::EntrySlot CharGenTintObject::ParseSlot(const std::string & slotName)
{
	if(g_slotMap.empty())
		InitSlotMap();

	auto iter = g_slotMap.find(slotName.c_str());
	if(iter != g_slotMap.end())
	{
		return iter->second;
	}

	_ERROR("Unknown slot name: %s", slotName.c_str());
	return BGSCharacterTint::EntrySlot::kFaceDetail;
}

std::string CharGenTintObject::WriteSlot(BGSCharacterTint::EntrySlot slotId)
{
	if(g_slotMap.empty())
		InitSlotMap();

	for(auto & slot : g_slotMap)
	{
		if(slotId == slot.second)
		{
			return slot.first;
		}
	}

	return "";
}

RE::BGSCharacterTint::BlendOp CharGenTintObject::ParseBlendOp(const std::string & blendName)
{
	if(g_blendMap.empty())
		InitBlendMap();

	auto iter = g_blendMap.find(blendName);
	if(iter != g_blendMap.end())
	{
		return iter->second;
	}

	_ERROR("Unknown blend operation: %s", blendName.c_str());
	return BGSCharacterTint::BlendOp::kDefault;
}

std::string CharGenTintObject::WriteBlendOp(BGSCharacterTint::BlendOp blendOp)
{
	if(g_blendMap.empty())
		InitBlendMap();

	for(auto & blend : g_blendMap)
	{
		if(blendOp == blend.second)
		{
			return blend.first;
		}
	}

	return "";
}

UInt32 CharGenTintObject::ParseFlags(const Json::Value & value)
{
	if(g_flagMap.empty())
		InitFlagMap();

	UInt32 flags = 0;
	for(auto & str : value)
	{
		auto iter = g_flagMap.find(str.asString());
		if(iter != g_flagMap.end())
		{
			flags |= iter->second;
		}
	}

	return flags;
}

void CharGenTintObject::WriteFlags(UInt32 flags, Json::Value & value)
{
	if(g_flagMap.empty())
		InitFlagMap();

	for(auto & flag : g_flagMap)
	{
		if((flags & flag.second) == flag.second)
			value.append(flag.first);
	}
}

void CharGenTintObject::ForEachGender(const TESRace * race, UInt8 gender, std::function<void(BGSCharacterTint::Template::Groups *, UInt8)> func)
{
	if(gender == 2)
	{
		for(UInt8 i = 0; i <= 1; i++)
		{
			auto chargenData = race->faceRelatedData[i];
			if(chargenData)
			{
				auto tints = chargenData->tintingTemplate;
				if(tints)
				{
					func(tints, i);
				}
			}
		}
	}
	else
	{
		auto chargenData = race->faceRelatedData[gender];
		if(chargenData)
		{
			auto tints = chargenData->tintingTemplate;
			if(tints)
			{
				func(tints, gender);
			}
		}
	}
}

BGSCharacterTint::Template::Group * CharGenTintObject::GetCategoryByName(const BGSCharacterTint::Template::Groups * data, const char * name)
{
	for(BGSCharacterTint::Template::Group * tintData : data->groups)
	{
		if(_stricmp(tintData->name.c_str(), name) == 0)
			return tintData;
	}

	return nullptr;
}

BGSCharacterTint::Template::Group * CharGenTintObject::GetCategoryByID(const BGSCharacterTint::Template::Groups * data, UInt32 id)
{
	for(BGSCharacterTint::Template::Group * tintData : data->groups)
	{
		if(tintData->chargenIndex == id)
			return tintData;
	}

	return nullptr;
}

BGSCharacterTint::Template::Entry * CharGenTintObject::GetTemplateByID(const BSTArray<BGSCharacterTint::Template::Entry*> * data, UInt32 id)
{
	for(BGSCharacterTint::Template::Entry * tintData : *data)
	{
		if(tintData->uniqueID == id)
			return tintData;
	}

	return nullptr;
}

BGSCharacterTint::Template::Group * CharGenTintObject::CreateCategory()
{
	BGSCharacterTint::Template::Group* data = (BGSCharacterTint::Template::Group*)Heap_Allocate(sizeof(BGSCharacterTint::Template::Group));
	memset(data, 0, sizeof(BGSCharacterTint::Template::Group));
	return data;
}

void CharGenTintObject::SetCategory(BGSCharacterTint::Template::Group * tintData)
{
	tintData->name = name.c_str();
	tintData->chargenIndex = categoryId;
	tintData->id = -1;
}

bool CharGenTintObject::Apply(const TESRace * race, UInt8 gender)
{
	try
	{
		ForEachGender(race, gender, [&](BGSCharacterTint::Template::Groups * tints, UInt8 genderId)
		{
			BGSCharacterTint::Template::Group * tintData = GetCategoryByID(tints, categoryId);
			if(!tintData)
			{
				if(name.empty())
				{
					throw std::exception("Invalid category name for identifier");
				}


				tintData = CreateCategory();
				SetCategory(tintData);
				tints->groups.push_back(tintData);
			}
			else
			{
				// Assign new category data
				SetCategory(tintData);
			}
		});

		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

bool CharGenTintObject::Parse(const Json::Value & entry)
{
	try
	{
		if(entry.isMember("Name")) {
			name = entry["Name"].asString();
			modified |= kModified_Name;
		}
		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

bool CharGenTintBase::Parse(const Json::Value & entry)
{
	__super::Parse(entry);
	try
	{
		if(entry.isMember("Category")) {
			categoryId = entry["Category"].asUInt();
		}
		if(entry.isMember("FixedCategory")) {
			fixedCategory = entry["FixedCategory"].asString();
		}
		if(entry.isMember("Slot")) {
			slot = ParseSlot(entry["Slot"].asString());
			modified |= kModified_Slot;
		}

		if(entry.isMember("Flags")) {
			flags = (UInt8)ParseFlags(entry["Flags"]);
			modified |= kModified_Flags;
		}
		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

void CharGenTintBase::Set(BGSCharacterTint::Template::Entry* entry)
{
	if(IsFlagSet(kModified_Name))
		entry->name = name.c_str();
	const_cast<std::uint16_t&>(entry->uniqueID) = templateId;

	if(IsFlagSet(kModified_Slot))
		entry->slot = (BGSCharacterTint::EntrySlot)slot;

	if(IsFlagSet(kModified_Flags))
		entry->flags = flags;
}

bool CharGenTintBase::Apply(const TESRace * race, UInt8 gender)
{
	try
	{
		ForEachGender(race, gender, [&](BGSCharacterTint::Template::Groups * tints, UInt8 genderId)
		{
			BGSCharacterTint::Template::Group * tintData = nullptr;
			if(categoryId != 0)
				tintData = GetCategoryByID(tints, categoryId);
			else
				tintData = GetCategoryByName(tints, fixedCategory.c_str());

			if(tintData)
			{
				BGSCharacterTint::Template::Entry* tintTemplate = GetTemplateByID(&tintData->entries, templateId);
				if(!tintTemplate)
				{
					// Create new entry
					tintTemplate = Create();
					Set(tintTemplate);
					tintData->entries.push_back(tintTemplate);
				}
				else
				{
					// Assign to existing template
					Set(tintTemplate);
				}
			}
		});

		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

bool CharGenTintMask::Parse(const Json::Value & entry)
{
	__super::Parse(entry);
	try
	{
		if(entry.isMember("Texture"))
		{
			texture = entry["Texture"].asString();
			modified |= kModified_Texture;
		}
		if(entry.isMember("BlendOp"))
		{
			blendOp = ParseBlendOp(entry["BlendOp"].asString());
			modified |= kModified_BlendOp;
		}
		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

BGSCharacterTint::Template::Entry* CharGenTintMask::Create()
{
	auto pMask = new BGSCharacterTint::Template::Mask();
	pMask->maskTextureName = "";
	return pMask;
}

void CharGenTintMask::Set(BGSCharacterTint::Template::Entry* entry)
{
	__super::Set(entry);

	BGSCharacterTint::Template::Mask* pMask = (BGSCharacterTint::Template::Mask*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__Mask);
	if(pMask)
	{
		if(IsFlagSet(kModified_BlendOp))
			pMask->blendOp = (RE::BGSCharacterTint::BlendOp)blendOp;

		if(IsFlagSet(kModified_Texture))
			pMask->maskTextureName = texture.c_str();
	}
}


bool CharGenTintPalette::Parse(const Json::Value & entry)
{
	__super::Parse(entry);
	try
	{
		if(entry.isMember("Texture"))
		{
			texture = entry["Texture"].asString();
			modified |= kModified_Texture;
		}
		
		auto entries = entry["Colors"];
		for(auto & colorEntry : entries)
		{
			BGSCharacterTint::Template::Palette::ColorValue data;
			memset(&data, 0, sizeof(BGSCharacterTint::Template::Palette::ColorValue));
			data.swatchID = colorEntry["Id"].asUInt();
			data.color = DYNAMIC_CAST(GetFormFromIdentifier(colorEntry["Form"].asString()), TESForm, BGSColorForm);
			data.value = colorEntry["Alpha"].asFloat();

			if(colorEntry.isMember("BlendOp"))
				data.blendOp = ParseBlendOp(colorEntry["BlendOp"].asString());

			if(data.color)
				colors.push_back(data);
		}

		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

BGSCharacterTint::Template::Entry* CharGenTintPalette::Create()
{
	auto pPalette = new BGSCharacterTint::Template::Palette();
	pPalette->maskTextureName = "";
	return pPalette;
}

void CharGenTintPalette::Set(BGSCharacterTint::Template::Entry* entry)
{
	__super::Set(entry);

	BGSCharacterTint::Template::Palette* pPalette = (BGSCharacterTint::Template::Palette*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__Palette);
	if(pPalette)
	{
		if(IsFlagSet(kModified_Texture))
			pPalette->maskTextureName = texture.c_str();

		std::set<UInt16> colorIds;
		for(BGSCharacterTint::Template::Palette::ColorValue colorData : pPalette->colorValues)
			colorIds.insert(colorData.swatchID);

		// Merge new colors into the list
		for(auto & colorData : colors)
		{
			// Only add color Ids not in the list
			if(colorIds.find(colorData.swatchID) == colorIds.end())
				pPalette->colorValues.push_back(colorData);
		}
	}
}

bool CharGenTintTextureSet::Parse(const Json::Value & entry)
{
	__super::Parse(entry);
	try
	{
		if(entry.isMember("Diffuse"))
		{
			diffuse = entry["Diffuse"].asString();
			modified |= kModified_Diffuse;
		}

		if(entry.isMember("Normal"))
		{
			normal = entry["Normal"].asString();
			modified |= kModified_Normal;
		}

		if(entry.isMember("Specular"))
		{
			specular = entry["Specular"].asString();
			modified |= kModified_Specular;
		}

		if(entry.isMember("BlendOp"))
		{
			blendOp = ParseBlendOp(entry["BlendOp"].asString());
			modified |= kModified_BlendOp;
		}

		if(entry.isMember("Default"))
		{
			defaultValue = entry["Default"].asFloat();
			modified |= kModified_Default;
		}

		return true;
	}
	catch(const std::exception& e)
	{
		_ERROR(e.what());
	}

	return false;
}

BGSCharacterTint::Template::Entry* CharGenTintTextureSet::Create()
{
	auto textureSet = new BGSCharacterTint::Template::TextureSet();
	textureSet->diffuse = "";
	textureSet->normal = "";
	textureSet->specular = "";
	return textureSet;
}

void CharGenTintTextureSet::Set(BGSCharacterTint::Template::Entry* entry)
{
	__super::Set(entry);

	BGSCharacterTint::Template::TextureSet* pTextureSet = (BGSCharacterTint::Template::TextureSet*)Runtime_DynamicCast(entry, RTTI::BGSCharacterTint__Template__Entry, RTTI::BGSCharacterTint__Template__TextureSet);
	if(pTextureSet)
	{
		if(IsFlagSet(kModified_Diffuse))
			pTextureSet->diffuse = diffuse.c_str();
		if(IsFlagSet(kModified_Normal))
			pTextureSet->normal = normal.c_str();
		if(IsFlagSet(kModified_Specular))
			pTextureSet->specular = specular.c_str();
		if(IsFlagSet(kModified_BlendOp))
			pTextureSet->blendOp = blendOp;
		if(IsFlagSet(kModified_Default))
			pTextureSet->defaultValue = defaultValue;
	}
}