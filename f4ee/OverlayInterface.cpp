#include "OverlayInterface.h"


#include "json\json.h"
#include "ActorUpdateManager.h"
#include "Utilities.h"

using namespace REX::W32;
#include "common/IDirectoryIterator.h"

#include <stdio.h>
#include <set>
#include <unordered_map>
#include <algorithm>

extern OverlayInterface g_overlayInterface;
extern ActorUpdateManager g_actorUpdateManager;

extern StringTable g_stringTable;
extern bool g_bEnableOverlays;
extern const F4SE::TaskInterface * g_task;

NiNode * OverlayInterface::GetOverlayRoot(Actor * actor, NiNode * rootNode, bool createIfNecessary)
{
	BSFixedString overlayName("[Overlays]");
	NiAVObject * overlayNode = rootNode->GetObjectByName(overlayName);
	if(!overlayNode && createIfNecessary) {
		overlayNode = new NiNode();
		overlayNode->name = "[Overlays]";
		rootNode->AttachChild(overlayNode, false);
	}

	return overlayNode ? overlayNode->IsNode() : nullptr;
}

void OverlayInterface::DestroyOverlaySlot(Actor * actor, NiNode * overlayHolder, UInt32 slotIndex)
{
	std::set<NiPointer<NiAVObject>> nodesToDelete;
	for(NiPointer<NiAVObject> childObject : overlayHolder->children)
	{
		UInt32 targetSlot = 0;
		if(childObject) {
			if(sscanf_s(childObject->name.c_str(), "[%d][", &targetSlot) && targetSlot == slotIndex)
				nodesToDelete.insert(childObject);
		}
	}

	for(auto & node : nodesToDelete)
		overlayHolder->DetachChild(node.get());
}

bool OverlayInterface::UpdateOverlays(Actor * actor, NiNode * rootNode, NiPointer<NiAVObject> object, UInt32 slotIndex)
{
	TESNPC * npc =  DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
	if(!npc)
		return false;

	SEX gender = CALL_MEMBER_FN(npc, GetSex)();
	bool isFemale = gender == SEX::kFemale ? true : false;

	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	bool hasOverlays = hit != m_overlays[isFemale ? 1 : 0].end();

	NiNode* overlayHolder = GetOverlayRoot(actor, rootNode, false);
	if (overlayHolder && !hasOverlays)
	{
		overlayHolder->parent->DetachChild(overlayHolder);
		overlayHolder = nullptr;
	}
	else if(hasOverlays)
	{
		overlayHolder = GetOverlayRoot(actor, rootNode, true);
	}

	if(overlayHolder)
	{
		DestroyOverlaySlot(actor, overlayHolder, slotIndex);

		std::unordered_multimap<UInt32, BSTriShape*> candidates;
		VisitObjects(object, [&](NiPointer<NiAVObject> node)
		{
			BSTriShape * trishape = node->IsTriShape();
			if(trishape) {
				BSLightingShaderProperty * shaderProperty = netimmerse_cast<BSLightingShaderProperty *, NiProperty>(trishape->properties[1].get());
				if(shaderProperty) {
					BSLightingShaderMaterialBase * newMaterial = static_cast<BSLightingShaderMaterialBase *>(shaderProperty->material);
					if(newMaterial->GetFeature() == BSShaderMaterial::Feature::kSkinTint) {
						candidates.emplace(slotIndex, trishape);
					}
				}
			}
			return false;
		});

		char buff[256];
		for(auto & item : candidates)
		{
			sprintf_s(buff, 256, "[%d][%s]", item.first, item.second->name.c_str());
			NiNode * overlayRoot = new NiNode();
			overlayRoot->name = buff;
			overlayHolder->AttachChild(overlayRoot, true);

			UInt32 layerIndex = 0;

			std::vector<std::tuple<std::string, OverlayDataPtr, F4EEFixedString, bool>> overlays;

			ForEachOverlayBySlot(actor, isFemale, slotIndex, [&](SInt32, const OverlayDataPtr & overlay, const F4EEFixedString & material, bool effect)
			{
				sprintf_s(buff, 256, "Layer [%d][%d][%d]", layerIndex, item.first, overlay->uid);
				overlays.push_back(std::make_tuple(buff, overlay, material, effect));
				layerIndex++;
			});

			for(auto & ovl : overlays)
			{
				NiCloningProcess cp;
				memset(&cp, 0, sizeof(NiCloningProcess));
				cp.copyType = NiCloningProcess::CopyType::kCopyExact;

				BSTriShape * cloned = (BSTriShape*)item.second->CreateClone(cp);

				auto extraDataList = cloned->extra;
				if(extraDataList)
				{
					extraDataList->lock.lock_write();
					for(auto entry : extraDataList->extra)
					{
						entry->DecRefCount();
					}
					extraDataList->extra.clear();
					extraDataList->lock.unlock_write();
				}
				cloned->skinInstance = item.second->skinInstance;
				cloned->name = std::get<0>(ovl).c_str();
				overlayRoot->AttachChild(cloned, true);

				LoadMaterialData(npc, cloned, std::get<2>(ovl), std::get<3>(ovl), std::get<1>(ovl));
			}
		}
	}

	return true;
}

typedef UInt32 (* _LoadMaterialFile)(const char * filePath, BSShaderData * shaderData, UInt32 unk1);
static REL::Relocation<_LoadMaterialFile> LoadMaterialFile{ REL::ID{ 2274963 } };

void OverlayInterface::LoadMaterialData(TESNPC * npc, BSTriShape * shape, const F4EEFixedString & material, bool effect, const OverlayDataPtr & overlayData)
{
	// Convert the shader type... BECAUSE WE CAN
	BSLightingShaderProperty * shaderProperty = netimmerse_cast<BSLightingShaderProperty *, NiProperty>(shape->properties[1].get());
	if(shaderProperty && effect) {
		shape->properties[1] = new BSEffectShaderProperty();
		shape->properties[1]->name = material.c_str();
	}
	BSEffectShaderProperty * effectShader = netimmerse_cast<BSEffectShaderProperty *, NiProperty>(shape->properties[1].get());
	if(effectShader && !effect) {
		shape->properties[1] = new BSLightingShaderProperty();
		shape->properties[1]->name = material.c_str();
	}

	// Setup the Overlays materials
	BSShaderData shaderData;
	if(!LoadMaterialFile(material.c_str(), &shaderData, 0))
		CALL_MEMBER_FN(&shaderData, ApplyMaterialData)(shape, true);

	BSShaderProperty * newShader = netimmerse_cast<BSShaderProperty *, NiProperty>(shape->properties[1].get());
	if(newShader) {
		BSShaderMaterial * newMaterial = static_cast<BSShaderMaterial *>(newShader->material);
		if(newMaterial) {
			// Transform the UV
			float sU, sV, oU, oV;
			newMaterial->GetOffsetUV(&oU, &oV);
			newMaterial->GetScaleUV(&sU, &sV);

			oU += overlayData->offsetUV.x;
			oV += overlayData->offsetUV.y;

			sU *= overlayData->scaleUV.x;
			sV *= overlayData->scaleUV.y;

			newMaterial->SetOffsetUV(oU, oV);
			newMaterial->SetScaleUV(sU, sV);

			BSLightingShaderProperty * newLightingShader = netimmerse_cast<BSLightingShaderProperty *, NiProperty>(newShader);
			if(newLightingShader) {
				BSLightingShaderMaterialBase * shaderMaterialBase = static_cast<BSLightingShaderMaterialBase *>(newMaterial);

				// Alter Lighting Properties
				if(newMaterial->GetFeature() == BSLightingShaderMaterialBase::Feature::kSkinTint && newMaterial->GetType() == BSLightingShaderMaterialBase::Type::kLighting) {
					BSLightingShaderMaterialSkinTint * skinTint = static_cast<BSLightingShaderMaterialSkinTint *>(newMaterial);

					if(overlayData->flags & OverlayInterface::OverlayData::kHasTintColor) {
						skinTint->kTintColor = overlayData->tintColor;
					} else {
						skinTint->kTintColor.r = (float)npc->bodyTintColorR / 255.0f;
						skinTint->kTintColor.g = (float)npc->bodyTintColorG / 255.0f;
						skinTint->kTintColor.b = (float)npc->bodyTintColorB / 255.0f;
						skinTint->kTintColor.a = (float)npc->bodyTintColorA / 255.0f;
					}
				}
				if(overlayData->flags & OverlayInterface::OverlayData::kHasTintColor) {
					shaderMaterialBase->lookupScale = overlayData->remapIndex;
				}
			}

			// Alter Effect Properties
			BSEffectShaderProperty * newEffectShader = netimmerse_cast<BSEffectShaderProperty *, NiProperty>(newShader);
			if(newEffectShader) {
				if(newMaterial->GetFeature() == BSLightingShaderMaterialBase::Feature::kDefault && newMaterial->GetType() == BSLightingShaderMaterialBase::Type::kEffect) {
					BSEffectShaderMaterial * effectMaterial = static_cast<BSEffectShaderMaterial *>(newMaterial);
					if(overlayData->flags & OverlayInterface::OverlayData::kHasTintColor) {
						effectMaterial->baseColor = overlayData->tintColor;
					}
				}
			}
		}
	}
}

OverlayInterface::UniqueID OverlayInterface::AddOverlay(Actor * actor, bool isFemale, SInt32 priority, const F4EEFixedString & templateName, const NiColorA & tintColor, const NiPoint2 & offsetUV, const NiPoint2 & scaleUV)
{
	auto it = g_overlayInterface.m_overlayTemplates[isFemale ? 1 : 0].find(templateName);
	if(it == g_overlayInterface.m_overlayTemplates[isFemale ? 1 : 0].end())
	{
		return 0;
	}

	std::unique_lock<std::mutex> locker(m_overlayLock);
	PriorityMapPtr priorityMap;
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit != m_overlays[isFemale ? 1 : 0].end()) {
		priorityMap = hit->second;
	} else {
		priorityMap = std::make_shared<PriorityMap>();
		m_overlays[isFemale ? 1 : 0].emplace(actor->formID, priorityMap);
	}

	UniqueID uid = GetNextUID();
	OverlayDataPtr overlayData = std::make_shared<OverlayData>();
	overlayData->uid = uid;
	overlayData->templateName = g_stringTable.GetString(templateName);
	overlayData->tintColor = tintColor;
	overlayData->offsetUV = offsetUV;
	overlayData->scaleUV = scaleUV;
	overlayData->UpdateFlags();
	priorityMap->emplace(priority, overlayData);
	m_dataMap.emplace(uid, overlayData);
	return uid;
}

OverlayInterface::UniqueID OverlayInterface::GetNextUID()
{
	std::unique_lock<std::mutex> locker(m_overlayLock);

	OverlayInterface::UniqueID nextUID = 0;
	if(!m_freeIndices.empty()) {
		nextUID = m_freeIndices.back();
		m_freeIndices.pop_back();
	} else {
		nextUID = (OverlayInterface::UniqueID)m_dataMap.size() + 1; // This only happens when free indices is empty, meaning we've filled gaps in m_dataMap
	}

	return nextUID;
}

bool OverlayInterface::ReorderOverlay(Actor * actor, bool isFemale, UniqueID uid, SInt32 newPriority)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return false;

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return false;

	OverlayDataPtr overlayPtr = nullptr;
	for (auto it = priorityMap->begin(); it!= priorityMap->end(); ++it)
	{
		if(it->second && it->second->uid == uid) {
			overlayPtr = it->second;
			priorityMap->erase(it);
			break;
		}
	}

	if(overlayPtr) {
		priorityMap->emplace(newPriority, overlayPtr);
		return true;
	}

	return false;
}

bool OverlayInterface::RemoveOverlay(Actor * actor, bool isFemale, UniqueID uid)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return false;

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return false;

	for (auto it = priorityMap->begin(); it != priorityMap->end(); ++it)
	{
		OverlayDataPtr overlayPtr = it->second;
		if(!overlayPtr)
			continue;
		
		if(overlayPtr->uid == uid) {
			m_dataMap.erase(overlayPtr->uid);
			priorityMap->erase(it);
			return true;
		}
	}

	return false;
}

void OverlayInterface::CloneOverlays(Actor * source, Actor * target)
{
	if(!source || !target)
		return;

	std::unique_lock<std::mutex> locker(m_overlayLock);
	bool isFemale = false;
	TESNPC * npc = DYNAMIC_CAST(source->data.objectReference, TESForm, TESNPC);
	if(npc)
		isFemale = CALL_MEMBER_FN(npc, GetSex)() == SEX::kFemale ? true : false;

	auto it = m_overlays[isFemale ? 1 : 0].find(source->formID);
	if(it != m_overlays[isFemale ? 1 : 0].end()) {
		m_overlays[isFemale ? 1 : 0][target->formID] = it->second;
	}
}

std::pair<SInt32, OverlayInterface::OverlayDataPtr> OverlayInterface::GetActorOverlayByUID(Actor * actor, bool isFemale, UniqueID uid)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return std::make_pair<SInt32, OverlayDataPtr>(0, nullptr);

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return std::make_pair<SInt32, OverlayDataPtr>(0, nullptr);

	for (auto it = priorityMap->begin(); it != priorityMap->end(); ++it)
	{
		OverlayDataPtr overlayPtr = it->second;
		if(!overlayPtr)
			continue;

		if(overlayPtr->uid == uid) {
			return std::pair<SInt32, OverlayDataPtr>(it->first, overlayPtr);
		}
	}

	return std::make_pair<SInt32, OverlayDataPtr>(0, nullptr);
}

bool OverlayInterface::RemoveAll(Actor * actor, bool isFemale)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return false;

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return false;

	// Grab all the UIDs for this actor first
	for (auto it= priorityMap->begin(); it!= priorityMap->end(); ++it)
	{
		OverlayDataPtr overlayPtr = it->second;
		if(!overlayPtr)
			continue;

		m_freeIndices.push_back(overlayPtr->uid);
		m_dataMap.erase(overlayPtr->uid);
	}

	m_overlays[isFemale ? 1 : 0].erase(hit);
	return true;
}

bool OverlayInterface::ForEachOverlay(Actor * actor, bool isFemale, std::function<void(SInt32, const OverlayDataPtr&)> functor)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return false;

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return false;

	for(auto & it : *priorityMap)
	{
		if(it.second)
			functor(it.first, it.second);
	}

	return true;
}

bool OverlayInterface::ForEachOverlayBySlot(Actor * actor, bool isFemale, UInt32 slotIndex, std::function<void(SInt32, const OverlayDataPtr&, const F4EEFixedString &, bool)> functor)
{	
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto hit = m_overlays[isFemale ? 1 : 0].find(actor->formID);
	if(hit == m_overlays[isFemale ? 1 : 0].end())
		return false;

	PriorityMapPtr priorityMap = hit->second;
	if(!priorityMap)
		return false;

	for(auto & it : *priorityMap)
	{
		OverlayDataPtr pOverlay = it.second;
		if(pOverlay)
		{
			if(pOverlay->templateName)
			{
				auto pTemplate = GetTemplateByName(isFemale, *pOverlay->templateName);
				if(pTemplate) {
					auto slotIter = pTemplate->slotMaterial.find(slotIndex);
					if(slotIter != pTemplate->slotMaterial.end())
						functor(it.first, pOverlay, slotIter->second.first, slotIter->second.second);
				}
			}
		}
	}

	return true;
}

const OverlayInterface::OverlayTemplatePtr OverlayInterface::GetTemplateByName(bool isFemale, const F4EEFixedString& name)
{
	auto tmpIter = m_overlayTemplates[isFemale ? 1 : 0].find(name);
	if(tmpIter != m_overlayTemplates[isFemale ? 1 : 0].end()) {
		return tmpIter->second;
	}

	return nullptr;
}

void OverlayInterface::OverlayData::Save(const F4SE::SerializationInterface * intfc, UInt32 kVersion)
{
	Serialization::WriteData<UniqueID>(intfc, &uid);

	UInt32 stringId = g_stringTable.GetStringID(templateName);
	Serialization::WriteData<UInt32>(intfc, &stringId);

	Serialization::WriteData<UInt32>(intfc, &flags);

	if((flags & kHasTintColor) == kHasTintColor)
	{
		UInt32 a = (UInt32)std::max(0.0f, std::min(tintColor.a * 255.0f, 255.0f));
		UInt32 r = (UInt32)std::max(0.0f, std::min(tintColor.r * 255.0f, 255.0f));
		UInt32 g = (UInt32)std::max(0.0f, std::min(tintColor.g * 255.0f, 255.0f));
		UInt32 b = (UInt32)std::max(0.0f, std::min(tintColor.b * 255.0f, 255.0f));

		UInt32 tintARGB = (a << 24) | (r << 16) | (g << 8) | b;
		Serialization::WriteData<UInt32>(intfc, &tintARGB);
	}
	if((flags & kHasOffsetUV) == kHasOffsetUV)
	{
		Serialization::WriteData<float>(intfc, &offsetUV.x);
		Serialization::WriteData<float>(intfc, &offsetUV.y);
	}
	if((flags & kHasScaleUV) == kHasScaleUV)
	{
		Serialization::WriteData<float>(intfc, &scaleUV.x);
		Serialization::WriteData<float>(intfc, &scaleUV.y);
	}
	if((flags & kHasRemapIndex) == kHasRemapIndex)
	{
		Serialization::WriteData<float>(intfc, &remapIndex);
	}
}

bool OverlayInterface::OverlayData::Load(const F4SE::SerializationInterface * intfc, UInt32 kVersion, const std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	if (!Serialization::ReadData<UniqueID>(intfc, &uid))
	{
		_ERROR("%s - Error loading overlay uid", __FUNCTION__);
		return false;
	}

	UInt32 stringId;
	if (!Serialization::ReadData<UInt32>(intfc, &stringId))
	{
		_ERROR("%s - Error loading string id", __FUNCTION__);
		return false;
	}

	auto it = stringTable.find(stringId);
	if(it == stringTable.end())
	{
		_ERROR("%s - Error loading overlay material path string from table", __FUNCTION__);
		return false;
	}

	templateName = it->second;

	if (!Serialization::ReadData<UInt32>(intfc, &flags))
	{
		_ERROR("%s - Error loading overlay flags", __FUNCTION__);
		return false;
	}

	if((flags & kHasTintColor) == kHasTintColor)
	{
		UInt32 tintARGB;
		if (!Serialization::ReadData<UInt32>(intfc, &tintARGB))
		{
			_ERROR("%s - Error loading overlay tint color", __FUNCTION__);
			return false;
		}

		UInt32 a = (tintARGB >> 24) & 0xFF;
		UInt32 r = (tintARGB >> 16) & 0xFF;
		UInt32 g = (tintARGB >> 8) & 0xFF;
		UInt32 b = tintARGB & 0xFF;

		tintColor.a = std::max(0.0f, std::min(a / 255.0f, 1.0f));
		tintColor.r = std::max(0.0f, std::min(r / 255.0f, 1.0f));
		tintColor.g = std::max(0.0f, std::min(g / 255.0f, 1.0f));
		tintColor.b = std::max(0.0f, std::min(b / 255.0f, 1.0f));
	}

	if((flags & kHasOffsetUV) == kHasOffsetUV)
	{
		if (!Serialization::ReadData<float>(intfc, &offsetUV.x))
		{
			_ERROR("%s - Error loading overlay offset U", __FUNCTION__);
			return false;
		}

		if (!Serialization::ReadData<float>(intfc, &offsetUV.y))
		{
			_ERROR("%s - Error loading overlay offset V", __FUNCTION__);
			return false;
		}
	}

	if((flags & kHasScaleUV) == kHasScaleUV)
	{
		if (!Serialization::ReadData<float>(intfc, &scaleUV.x))
		{
			_ERROR("%s - Error loading overlay scale U", __FUNCTION__);
			return false;
		}

		if (!Serialization::ReadData<float>(intfc, &scaleUV.y))
		{
			_ERROR("%s - Error loading overlay scale V", __FUNCTION__);
			return false;
		}
	}

	if((flags & kHasRemapIndex) == kHasRemapIndex)
	{
		if (!Serialization::ReadData<float>(intfc, &remapIndex))
		{
			_ERROR("%s - Error loading overlay remap index", __FUNCTION__);
			return false;
		}
	}
	
	return true;
}

void OverlayInterface::PriorityMap::Save(const F4SE::SerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('OIPM', kVersion);

	UInt32 priorityCount = (UInt32)size();
	Serialization::WriteData<UInt32>(intfc, &priorityCount);

	// Save priority mapping
	for(auto & slot : *this)
	{
		SInt32 priority = slot.first;
		Serialization::WriteData<SInt32>(intfc, &priority);

		slot.second->Save(intfc, kVersion);
	}
}

bool OverlayInterface::PriorityMap::Load(const F4SE::SerializationInterface * intfc, bool isFemale, UInt32 kVersion, const std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	std::uint32_t type, length, version;

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'OIPM':
			{
				// Override Count
				UInt32 priorityCount = 0;
				if (!Serialization::ReadData<UInt32>(intfc, &priorityCount))
				{
					_ERROR("%s - Error loading priority count", __FUNCTION__);
					return false;
				}

				for (UInt32 i = 0; i < priorityCount; i++)
				{
					SInt32 priority;
					if (!Serialization::ReadData<SInt32>(intfc, &priority))
					{
						_ERROR("%s - Error loading priority", __FUNCTION__);
						return false;
					}

					OverlayDataPtr overlayData = std::make_shared<OverlayData>();
					if (!overlayData->Load(intfc, kVersion, stringTable))
					{
						_ERROR("%s - Error loading overlay data with priority %d", __FUNCTION__, priorityCount);
						return false;
					}

					// Don't add entries we dont have
					auto it = g_overlayInterface.m_overlayTemplates[isFemale ? 1 : 0].find(*overlayData->templateName);
					if(it != g_overlayInterface.m_overlayTemplates[isFemale ? 1 : 0].end())
					{
						emplace(priority, overlayData);
						g_overlayInterface.m_dataMap.emplace(overlayData->uid, overlayData);
						g_overlayInterface.m_highestUID = std::max(g_overlayInterface.m_highestUID, overlayData->uid);
					}
				}

				break;
			}
		default:
			{
				_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
				return false;
			}
		}
	}

	return true;
}

void OverlayInterface::OverlayMap::Save(const F4SE::SerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('OIOM', kVersion);

	UInt32 numOverlays = (UInt32)size();
	Serialization::WriteData<UInt32>(intfc, &numOverlays);

	// Save handle mappings
	for(auto & overlay : *this)
	{
		// Key
		Serialization::WriteData<UInt32>(intfc, &overlay.first);

		// Value
		overlay.second->Save(intfc, kVersion);
	}
}

bool OverlayInterface::OverlayMap::Load(const F4SE::SerializationInterface * intfc, bool isFemale, UInt32 kVersion, const std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	std::uint32_t type, length, version;

	if(intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
		case 'OIOM':
			{

				UInt32 overlays = 0;
				if (!Serialization::ReadData<UInt32>(intfc, &overlays))
				{
					_ERROR("%s - Error loading male overlay count", __FUNCTION__);
					return false;
				}

				for(UInt32 i = 0; i < overlays; i++)
				{
					UInt64 handle = 0;
					UInt32 formId = 0;
					if(version >= kVersion2)
					{
						// Key
						if (!Serialization::ReadData<UInt32>(intfc, &formId))
						{
							_ERROR("%s - Error loading actor formId", __FUNCTION__);
							return false;
						}
					}
					else if(version >= kVersion1)
					{
						// Key
						if (!Serialization::ReadData<UInt64>(intfc, &handle))
						{
							_ERROR("%s - Error loading actor key", __FUNCTION__);
							return false;
						}
					}


					PriorityMapPtr priorityMap = std::make_shared<PriorityMap>();
					if (!priorityMap->Load(intfc, isFemale, kVersion, stringTable))
					{
						_ERROR("%s - Error loading overlay priority map for actor %016llX", __FUNCTION__, handle);
						return false;
					}

					std::optional<std::uint64_t> newHandle = std::nullopt;
					std::optional<std::uint32_t> newFormId = std::nullopt;

					if(!g_bEnableOverlays)
						continue;

					if(version >= kVersion2)
					{
						newFormId = intfc->ResolveFormID(formId);
						if(!newFormId)
							continue;
					}
					else if(version >= kVersion1)
					{
						// Skip if handle is no longer valid.
						newHandle = intfc->ResolveHandle(handle);
						if (!newHandle)
							continue;
					}

					if(priorityMap->empty())
						continue;

					
					Actor * actor = nullptr;
					if(version >= kVersion2)
					{
						actor = DYNAMIC_CAST(LookupFormByID(*newFormId), TESForm, Actor);
					}
					else if(version >= kVersion1)
					{
						actor = (Actor*)PapyrusVM::GetObjectFromHandle(*newHandle, Actor::FORM_ID);
					}

					if(actor) {
						emplace(actor->formID, priorityMap);
						g_actorUpdateManager.PushUpdate(actor);
					}
				}

				return true;
			}
		}
	}

	return false;
}

void OverlayInterface::Save(const F4SE::SerializationInterface * intfc, UInt32 kVersion)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);

	intfc->OpenRecord('OVRL', kVersion);

	for(UInt32 g = 0; g <= 1; g++)
	{
		m_overlays[g].Save(intfc, kVersion);
	}
}

bool OverlayInterface::Load(const F4SE::SerializationInterface * intfc, UInt32 kVersion, const std::unordered_map<UInt32, StringTableItem> & stringTable)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);

	m_overlays[0].Load(intfc, false, kVersion, stringTable);
	m_overlays[1].Load(intfc, true, kVersion, stringTable);

	// Build the free index list
	UInt32 dataSize = m_highestUID;
	for(UInt32 uid = 1; uid <= dataSize; uid++)
	{
		auto it = m_dataMap.find(uid);
		if(it == m_dataMap.end()) {
			m_freeIndices.push_back(uid);
		}
	}
	
	return true;
}

void OverlayInterface::Revert()
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	m_overlays[0].clear();
	m_overlays[1].clear();
	m_freeIndices.clear();
	m_dataMap.clear();
}

const OverlayInterface::OverlayDataPtr OverlayInterface::GetOverlayByUID(UniqueID uid)
{
	std::unique_lock<std::mutex> locker(m_overlayLock);
	auto it = m_dataMap.find(uid);
	if(it != m_dataMap.end()) {
		return it->second;
	}

	return nullptr;
}

bool OverlayInterface::HasSkinChildren(NiPointer<NiAVObject> slot)
{
	return VisitObjects(slot, [&](NiPointer<NiAVObject> node)
	{
		BSTriShape * trishape = node->IsTriShape();
		if(trishape) {
			BSLightingShaderProperty * shaderProperty = netimmerse_cast<BSLightingShaderProperty *, NiProperty>(trishape->properties[1].get());
			if(shaderProperty) {
				BSLightingShaderMaterialBase * newMaterial = static_cast<BSLightingShaderMaterialBase *>(shaderProperty->material);
				if(newMaterial->GetFeature() == BSLightingShaderMaterialBase::Feature::kSkinTint) {
					return true;
				}
			}
		}
		return false;
	});
}


F4EEUpdateOverlays::F4EEUpdateOverlays(TESForm * form)
{
	m_formId = form ? form->formID : 0;
}

void F4EEUpdateOverlays::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	if(form) {
		Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
		if(actor) {
			// Delete all overlays
			NiNode * rootSkeleton = (NiNode *)actor->Get3D(false);
			if(rootSkeleton) {
				NiNode * overlayRoot = g_overlayInterface.GetOverlayRoot(actor, rootSkeleton, false);
				if(overlayRoot) {
					NiNode * parent = overlayRoot->parent;
					if(parent) {
						parent->DetachChild(overlayRoot);
					}
				}
			}

			// Delete the first person Overlays
			if(actor == PlayerCharacter::GetPlayer()) {
				NiNode * rootSkeleton = (NiNode *)actor->Get3D(true);
				if(rootSkeleton) {
					NiNode * overlayRoot = g_overlayInterface.GetOverlayRoot(actor, rootSkeleton, false);
					if(overlayRoot) {
						NiNode * parent = overlayRoot->parent;
						if(parent)
							parent->DetachChild(overlayRoot);
					}
				}
			}

			// Rebuild overlays
			BipedAnim* equipData[2];
			equipData[0] = actor->biped.get();
			equipData[1] = actor == PlayerCharacter::GetPlayer() ? PlayerCharacter::GetPlayer()->firstPersonBipedAnim.get() : nullptr;

			for(int s = 0; s < (actor == PlayerCharacter::GetPlayer() ? 2 : 1); s++)
			{
				if(!equipData[s])
					continue;

				for(int i = 0; i < 31; ++i)
				{
					NiPointer<NiAVObject> slotNode(equipData[s]->object[i].partClone);
					if(!slotNode)
						continue;

					// See if this node has any skin candidates first
					if(!g_overlayInterface.HasSkinChildren(slotNode))
						continue;

					NiNode * rootNode = GetRootNode(actor, slotNode);
					if(rootNode)
						g_overlayInterface.UpdateOverlays(actor, rootNode, slotNode, i);
				}
			}
		}
	}
}


F4EEOverlayUpdate::F4EEOverlayUpdate(TESForm * form, UInt32 uid)
{
	m_formId = form ? form->formID : 0;
	m_uid = uid;
}

void F4EEOverlayUpdate::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	if(!form)
		return;

	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if(!actor)
		return;

	TESNPC * npc =  DYNAMIC_CAST(actor->data.objectReference, TESForm, TESNPC);
	if(!npc)
		return;

	SEX gender = CALL_MEMBER_FN(npc, GetSex)();
	bool isFemale = gender == SEX::kFemale ? true : false;

	if(!actor->currentProcess)
		return;

	// Detaching the node will cause the game to regenerate when UpdateEquipment is called
	// We only need to detach armor, and armor that's even eligible for morphing
	BipedAnim* equipData[2];
	equipData[0] = actor->biped.get();
	equipData[1] = actor == PlayerCharacter::GetPlayer() ? PlayerCharacter::GetPlayer()->firstPersonBipedAnim.get() : nullptr;

	for(int s = 0; s < (actor == PlayerCharacter::GetPlayer() ? 2 : 1); s++)
	{
		if(!equipData[s])
			continue;

		for(int slot = 0; slot < 31; slot++)
		{
			NiPointer<NiAVObject> slotNode(equipData[s]->object[slot].partClone);
			if(!slotNode)
				continue;

			// See if this node has any skin candidates first
			if(!g_overlayInterface.HasSkinChildren(slotNode))
				continue;

			NiNode * rootNode = GetRootNode(actor, slotNode);
			if(!rootNode)
				continue;

			NiNode * overlayRoot = g_overlayInterface.GetOverlayRoot(actor, rootNode, false);
			if(!overlayRoot)
				continue;

			VisitObjects(overlayRoot, [&](NiPointer<NiAVObject> object)
			{
				BSTriShape * shape = object->IsTriShape();
				if(!shape)
					return false;

				UInt32 layerIndex;
				UInt32 slotIndex;
				UInt32 targetUID;
				if(sscanf_s(object->name.c_str(), "Layer [%d][%d][%d]", &layerIndex, &slotIndex, &targetUID) && targetUID == m_uid)
				{
					const OverlayInterface::OverlayDataPtr pOverlayData = g_overlayInterface.GetOverlayByUID(m_uid);
					if(!pOverlayData)
						return false;

					const OverlayInterface::OverlayTemplatePtr pTemplate = g_overlayInterface.GetTemplateByName(isFemale, *pOverlayData->templateName);
					if(!pTemplate)
						return false;

					auto material = pTemplate->slotMaterial.find(slotIndex);
					if(material != pTemplate->slotMaterial.end()) {
						g_overlayInterface.LoadMaterialData(npc, shape, material->second.first, material->second.second, pOverlayData);
					}
				}

				return false;
			});
		}
	}
}

bool OverlayInterface::UpdateOverlays(Actor * actor)
{
	if(!actor)
		return false;

	if(g_task)
		g_task->AddTask(new F4EEUpdateOverlays(actor));

	return true;
}

bool OverlayInterface::UpdateOverlay(Actor * actor, UInt32 uid)
{
	if(!actor)
		return false;

	if(g_task)
		g_task->AddTask(new F4EEOverlayUpdate(actor, uid));

	return true;
}

void OverlayInterface::LoadOverlayMods()
{
	std::string overlayPath("F4SE\\Plugins\\F4EE\\Overlays\\");

	// Load templates
	ForEachMod([&](const ModInfo * modInfo)
	{
		std::string templatesPath = overlayPath + std::string(modInfo->filename) + "\\overlays.json";
		LoadOverlayTemplates(templatesPath.c_str());
	});

	std::string loosePath("Data\\");
	loosePath += overlayPath;
	loosePath += "Loose";

	std::set<std::string> templates;
	for(IDirectoryIterator iter(loosePath.c_str(), "*.json"); !iter.Done(); iter.Next())
	{
		std::string	path = iter.GetFullPath();
		std::transform(path.begin(), path.begin(), path.end(), ::tolower);
		templates.insert(path);
	}

	for(auto & path : templates)
	{
		LoadOverlayTemplates(path);
	}
}


bool OverlayInterface::LoadOverlayTemplates(const std::string & filePath)
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
		_ERROR("%s - Failed to overlay file\t[%s]", __FUNCTION__, filePath.c_str());
		return false;
	}

	// traverse instances
	for(auto & item : root)
	{
		try
		{
			OverlayTemplatePtr pOverlayTemplate = nullptr;
			UInt8 gender = std::max(0u, std::min(item["gender"].asUInt(), 1u));

			F4EEFixedString id = item["id"].asCString();

			auto idIter = m_overlayTemplates[gender].find(id);
			if(idIter != m_overlayTemplates[gender].end())
				pOverlayTemplate = idIter->second;
			else {
				pOverlayTemplate = std::make_shared<OverlayTemplate>();
				m_overlayTemplates[gender].emplace(id, pOverlayTemplate);
				loadedTemplates++;
			}

			if(item.isMember("name"))
				pOverlayTemplate->displayName = item["name"].asCString();

			if(item.isMember("playable"))
				pOverlayTemplate->playable = item["playable"].asBool();

			if(item.isMember("sort"))
				pOverlayTemplate->sort = item["sort"].asInt();

			if(item.isMember("transformable"))
				pOverlayTemplate->transformable = item["transformable"].asBool();

			if(item.isMember("tintable"))
				pOverlayTemplate->tintable = item["tintable"].asBool();

			if(item.isMember("slots"))
			{
				auto slots = item["slots"];
				for(auto & slot : slots)
				{
					std::string material = slot["material"].asString();
					std::string extension;
					if(material.find_last_of(".") != std::string::npos)
						extension = material.substr(material.find_last_of(".")+1);

					bool isEffect = false;
					if(_stricmp(extension.c_str(), "bgem") == 0) {
						isEffect = true;
					}


					pOverlayTemplate->slotMaterial.emplace(slot["slot"].asUInt(), std::make_pair(slot["material"].asCString(), isEffect));
				}
			}
		}
		catch(const std::exception& e)
		{
			_ERROR(e.what());
		}
	}

	_MESSAGE("%s - Info - Loaded %d overlay template(s).\t[%s]", __FUNCTION__, loadedTemplates, filePath.c_str());
	return true;
}

void OverlayInterface::ForEachOverlayTemplate(bool isFemale, std::function<void(const F4EEFixedString&, const OverlayTemplatePtr&)> functor)
{
	for(auto & overlay : m_overlayTemplates[isFemale ? 1 : 0]) {
		functor(overlay.first, overlay.second);
	}
}