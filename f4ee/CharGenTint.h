#pragma once

#include "json\json.h"
#include <functional>

class CharGenTintObject
{
public:
	CharGenTintObject(UInt32 id) : categoryId(id), modified(0) { }
	
	virtual bool Apply(const TESRace * race, UInt8 gender);
	virtual bool Parse(const Json::Value & entry);

	virtual BGSCharacterTint::Template::Group * CreateCategory();
	virtual void SetCategory(BGSCharacterTint::Template::Group * category);

	virtual BGSCharacterTint::Template::Entry* Create() { return nullptr; }
	virtual void Set(BGSCharacterTint::Template::Entry* entry) { }

	static void ForEachGender(const TESRace * race, UInt8 gender, std::function<void(BGSCharacterTint::Template::Groups *, UInt8)> func);
	static BGSCharacterTint::Template::Group * GetCategoryByID(const BGSCharacterTint::Template::Groups * data, UInt32 id);
	static BGSCharacterTint::Template::Group * GetCategoryByName(const BGSCharacterTint::Template::Groups * data, const char * name);
	static BGSCharacterTint::Template::Entry * GetTemplateByID(const BSTArray<BGSCharacterTint::Template::Entry*> * data, UInt32 id);

	static BGSCharacterTint::EntrySlot ParseSlot(const std::string & slotName);
	static std::string WriteSlot(BGSCharacterTint::EntrySlot entrySlot);
	
	static BGSCharacterTint::BlendOp ParseBlendOp(const std::string & blendName);
	static std::string WriteBlendOp(BGSCharacterTint::BlendOp blendOp);

	static UInt32 ParseFlags(const Json::Value & value);
	static void WriteFlags(UInt32 flags, Json::Value & value);

	enum ModifiedFlag
	{
		kModified_Name = (1 << 0),
		kModified_Slot = (1 << 1),
		kModified_Flags = (1 << 2),
		kModified_BlendOp = (1 << 3),
		kModified_Texture = (1 << 4),
		kModified_Diffuse = (1 << 5),
		kModified_Normal = (1 << 6),
		kModified_Specular = (1 << 7),
		kModified_Default = (1 << 8),
	};

	bool IsFlagSet(UInt32 flag) { return (modified & flag) == flag; }

	UInt32 modified;
	std::string name;
	UInt32 categoryId;
	std::string fixedCategory;
};

class CharGenTintBase : public CharGenTintObject
{
public:
	CharGenTintBase(UInt16 id) : CharGenTintObject(0), templateId(id), slot(BGSCharacterTint::EntrySlot::kFaceDetail), flags(0) { }
	virtual ~CharGenTintBase() { }

	virtual BGSCharacterTint::Template::Group * CreateCategory() { return nullptr; }
	virtual void SetCategory(BGSCharacterTint::Template::Group * category) { }

	virtual void Set(BGSCharacterTint::Template::Entry* entry);

	virtual bool Apply(const TESRace * race, UInt8 gender);
	virtual bool Parse(const Json::Value & entry);

	UInt16						templateId;
	BGSCharacterTint::EntrySlot	slot;
	UInt8						flags;
};

class CharGenTintMask : public CharGenTintBase
{
public:
	CharGenTintMask(UInt16 id) : CharGenTintBase(id), blendOp(BGSCharacterTint::BlendOp::kDefault) { }
	virtual ~CharGenTintMask() { }

	virtual bool Parse(const Json::Value & entry);
	virtual BGSCharacterTint::Template::Entry* Create();
	virtual void Set(BGSCharacterTint::Template::Entry* entry);

	std::string					texture;
	BGSCharacterTint::BlendOp	blendOp;
};

class CharGenTintPalette : public CharGenTintBase
{
public:
	CharGenTintPalette(UInt16 id) : CharGenTintBase(id) { }
	virtual ~CharGenTintPalette() { }

	virtual bool Parse(const Json::Value & entry);
	virtual BGSCharacterTint::Template::Entry* Create();
	virtual void Set(BGSCharacterTint::Template::Entry* entry);

	std::string		texture;
	std::vector<BGSCharacterTint::Template::Palette::ColorValue> colors;
};

class CharGenTintTextureSet : public CharGenTintBase
{
public:
	CharGenTintTextureSet(UInt16 id) : CharGenTintBase(id), blendOp(BGSCharacterTint::BlendOp::kDefault), defaultValue(0.0f) { }
	virtual ~CharGenTintTextureSet() { }

	virtual bool Parse(const Json::Value & entry);
	virtual BGSCharacterTint::Template::Entry* Create();
	virtual void Set(BGSCharacterTint::Template::Entry* entry);

	std::string 				diffuse;
	std::string 				normal;
	std::string 				specular;
	BGSCharacterTint::BlendOp	blendOp;
	float						defaultValue;
};