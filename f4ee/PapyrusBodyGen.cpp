#include "PapyrusBodyGen.h"
#include "BodyMorphInterface.h"
#include "BodyGenInterface.h"
#include "SkinInterface.h"


extern BodyMorphInterface g_bodyMorphInterface;
extern BodyGenInterface g_bodyGenInterface;
extern SkinInterface g_skinInterface;

namespace papyrusBodyGen
{
	void SetMorph(std::monostate, Actor * actor, bool isFemale, BSFixedString morph, BGSKeyword * keyword, float value)
	{
		g_bodyMorphInterface.SetMorph(actor, isFemale, morph, keyword, value);
	}

	float GetMorph(std::monostate, Actor * actor, bool isFemale, BSFixedString morph, BGSKeyword * keyword)
	{
		return g_bodyMorphInterface.GetMorph(actor, isFemale, morph, keyword);
	}

	void RemoveMorphsByName(std::monostate, Actor * actor, bool isFemale, BSFixedString morph)
	{
		g_bodyMorphInterface.RemoveMorphsByName(actor, isFemale, morph);
	}

	void RemoveMorphsByKeyword(std::monostate, Actor * actor, bool isFemale, BGSKeyword * keyword)
	{
		g_bodyMorphInterface.RemoveMorphsByKeyword(actor, isFemale, keyword);
	}

	void RemoveAllMorphs(std::monostate, Actor * actor, bool isFemale)
	{
		g_bodyMorphInterface.ClearMorphs(actor, isFemale);
	}

	std::vector<BGSKeyword*> GetKeywords(std::monostate, Actor * actor, bool isFemale, BSFixedString morph)
	{
		std::vector<BGSKeyword*> keywords;
		g_bodyMorphInterface.GetKeywords(actor, isFemale, morph, keywords);
		return keywords;
	}

	std::vector<BSFixedString> GetMorphs(std::monostate, Actor * actor, bool isFemale)
	{
		std::vector<BSFixedString> morphs;
		g_bodyMorphInterface.GetMorphs(actor, isFemale, morphs);
		return morphs;
	}

	void ClearAll(std::monostate)
	{
		g_bodyMorphInterface.Revert();
	}

	void RegenerateMorphs(std::monostate, Actor * actor, bool update)
	{
		if(!actor)
			return;

		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(!npc)
			return;

		SEX gender = CALL_MEMBER_FN(npc, GetSex)();
		bool isFemale = gender == SEX::kFemale ? true : false;

		g_bodyMorphInterface.ClearMorphs(actor, isFemale);
		g_bodyGenInterface.EvaluateBodyMorphs(actor, isFemale);

		if(update)
			g_bodyMorphInterface.UpdateMorphs(actor);
	}

	void UpdateMorphs(std::monostate, Actor * actor)
	{
		g_bodyMorphInterface.UpdateMorphs(actor);
	}

	bool SetSkinOverride(std::monostate, Actor * actor, BSFixedString id)
	{
		if(!actor)
			return false;

		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(!npc)
			return false;

		SEX gender = CALL_MEMBER_FN(npc, GetSex)();
		bool isFemale = gender == SEX::kFemale ? true : false;

		bool ret = g_skinInterface.AddSkinOverride(actor, id, isFemale);
		if(ret)
			g_skinInterface.UpdateSkinOverride(actor, true);
		return ret;
	}

	bool RemoveSkinOverride(std::monostate, Actor * actor)
	{
		if(!actor)
			return false;

		TESNPC * npc = DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
		if(!npc)
			return false;

		SEX gender = CALL_MEMBER_FN(npc, GetSex)();
		bool isFemale = gender == SEX::kFemale ? true : false;

		bool ret = g_skinInterface.RemoveSkinOverride(actor);
		if(ret)
			g_skinInterface.UpdateSkinOverride(actor, true);
		return ret;
	}
};

void papyrusBodyGen::RegisterFuncs(BSScript::IVirtualMachine* vm)
{
	vm->BindNativeMethod("SetMorph", "BodyGen", papyrusBodyGen::SetMorph);
	vm->BindNativeMethod("GetMorph", "BodyGen", papyrusBodyGen::GetMorph);
	vm->BindNativeMethod("RemoveMorphsByName", "BodyGen", papyrusBodyGen::RemoveMorphsByName);
	vm->BindNativeMethod("RemoveMorphsByKeyword", "BodyGen", papyrusBodyGen::RemoveMorphsByKeyword);
	vm->BindNativeMethod("RemoveAllMorphs", "BodyGen", papyrusBodyGen::RemoveAllMorphs);
	vm->BindNativeMethod("GetKeywords", "BodyGen", papyrusBodyGen::GetKeywords);
	vm->BindNativeMethod("GetMorphs", "BodyGen", papyrusBodyGen::GetMorphs);
	vm->BindNativeMethod("RegenerateMorphs", "BodyGen", papyrusBodyGen::RegenerateMorphs);
	vm->BindNativeMethod("UpdateMorphs", "BodyGen", papyrusBodyGen::UpdateMorphs);
	vm->BindNativeMethod("ClearAll", "BodyGen", papyrusBodyGen::ClearAll);
	vm->BindNativeMethod("SetSkinOverride", "BodyGen", papyrusBodyGen::SetSkinOverride);
	vm->BindNativeMethod("RemoveSkinOverride", "BodyGen", papyrusBodyGen::RemoveSkinOverride);
}
