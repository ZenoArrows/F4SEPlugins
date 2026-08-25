#include "PapyrusOverlays.h"

#include "OverlayInterface.h"

#include <variant>
#include <vector>

extern OverlayInterface g_overlayInterface;

namespace papyrusOverlays
{
	using Entry = BSScript::structure_wrapper<"Overlays", "Entry">;

	UInt32 Add(std::monostate, Actor * actor, bool isFemale, Entry overlay)
	{
		if(actor) {
			SInt32 priority;
			BSFixedString id;
			NiColorA color;
			NiPoint2 offsetUV;
			NiPoint2 scaleUV;

			priority = overlay.find<SInt32>("priority").value_or(0);
			id = overlay.find<RE::BSFixedString>("template").value_or("");
			color.r = overlay.find<float>("red").value_or(0.0f);
			color.g = overlay.find<float>("green").value_or(0.0f);
			color.b = overlay.find<float>("blue").value_or(0.0f);
			color.a = overlay.find<float>("alpha").value_or(0.0f);
			offsetUV.x = overlay.find<float>("offset_u").value_or(0.0f);
			offsetUV.y = overlay.find<float>("offset_v").value_or(0.0f);
			scaleUV.x = overlay.find<float>("scale_u").value_or(0.0f);
			scaleUV.y = overlay.find<float>("scale_v").value_or(0.0f);

			UInt32 uid = g_overlayInterface.AddOverlay(actor, isFemale, priority, id, color, offsetUV, scaleUV);
			overlay.insert("uid", uid);
			return uid;
		}
		return 0;
	}

	bool Set(std::monostate, Actor * actor, bool isFemale, UInt32 uid, Entry overlay)
	{
		auto pOverlay = g_overlayInterface.GetActorOverlayByUID(actor, isFemale, uid);
		if(pOverlay.second) {
			SInt32 priority;
			BSFixedString id;
			NiColorA color;
			NiPoint2 offsetUV;
			NiPoint2 scaleUV;

			priority = overlay.find<SInt32>("priority").value_or(0);
			id = overlay.find<RE::BSFixedString>("template").value_or("");
			color.r = overlay.find<float>("red").value_or(0.0f);
			color.g = overlay.find<float>("green").value_or(0.0f);
			color.b = overlay.find<float>("blue").value_or(0.0f);
			color.a = overlay.find<float>("alpha").value_or(0.0f);
			offsetUV.x = overlay.find<float>("offset_u").value_or(0.0f);
			offsetUV.y = overlay.find<float>("offset_v").value_or(0.0f);
			scaleUV.x = overlay.find<float>("scale_u").value_or(0.0f);
			scaleUV.y = overlay.find<float>("scale_v").value_or(0.0f);

			if(pOverlay.first != priority) {
				g_overlayInterface.ReorderOverlay(actor, isFemale, uid, priority);
			}

			auto pOverlayData = pOverlay.second;
			pOverlayData->tintColor = color;
			pOverlayData->offsetUV = offsetUV;
			pOverlayData->scaleUV = scaleUV;
			pOverlayData->UpdateFlags();
			return true;
		}

		return false;
	}

	std::optional<Entry> Get(std::monostate, Actor * actor, bool isFemale, UInt32 uid)
	{
		Entry overlay;
		auto pOverlay = g_overlayInterface.GetActorOverlayByUID(actor, isFemale, uid);
		if(pOverlay.second) {
			BSFixedString templateName = pOverlay.second->templateName ? pOverlay.second->templateName->c_str() : "";

			overlay.insert("uid", uid);
			overlay.insert("priority", pOverlay.first);
			overlay.insert("template", templateName);

			overlay.insert("red", pOverlay.second->tintColor.r);
			overlay.insert("green", pOverlay.second->tintColor.g);
			overlay.insert("blue", pOverlay.second->tintColor.b);
			overlay.insert("alpha", pOverlay.second->tintColor.a);

			overlay.insert("offset_u", pOverlay.second->offsetUV.x);
			overlay.insert("offset_v", pOverlay.second->offsetUV.y);
			overlay.insert("scale_u", pOverlay.second->scaleUV.x);
			overlay.insert("scale_v", pOverlay.second->scaleUV.y);

			return overlay;
		}

		return std::nullopt;
	}

	// Only looks at slot, priority, owner, and material to remove an entry
	bool Remove(std::monostate, Actor * actor, bool isFemale, UInt32 uid)
	{
		if(actor) {
			return g_overlayInterface.RemoveOverlay(actor, isFemale, uid);
		}

		return false;
	}

	bool RemoveAll(std::monostate, Actor * actor, bool isFemale)
	{
		if(actor) {
			return g_overlayInterface.RemoveAll(actor, isFemale);
		}

		return false;
	}

	std::vector<Entry> GetAll(std::monostate, Actor * actor, bool isFemale)
	{
		std::vector<Entry> results;
		if(!actor)
			return results;

		g_overlayInterface.ForEachOverlay(actor, isFemale, [&](SInt32 priority, const OverlayInterface::OverlayDataPtr & overlay)
		{
			Entry entry;
			entry.insert("uid", overlay->uid);
			entry.insert("priority", priority);

			BSFixedString templateName = overlay->templateName ? overlay->templateName->c_str() : "";
			entry.insert("template", templateName);

			entry.insert("red", overlay->tintColor.r);
			entry.insert("green", overlay->tintColor.g);
			entry.insert("blue", overlay->tintColor.b);
			entry.insert("alpha", overlay->tintColor.a);

			entry.insert("offset_u", overlay->offsetUV.x);
			entry.insert("offset_v", overlay->offsetUV.y);
			entry.insert("scale_u", overlay->scaleUV.x);
			entry.insert("scale_v", overlay->scaleUV.y);

			results.push_back(entry);
		});

		return results;
	}

	void ClearAll(std::monostate)
	{
		g_overlayInterface.Revert();
	}
	
	void Update(std::monostate, Actor * actor)
	{
		g_overlayInterface.UpdateOverlays(actor);
	}
};

void papyrusOverlays::RegisterFuncs(BSScript::IVirtualMachine* vm)
{
	vm->BindNativeMethod("Add", "Overlays", papyrusOverlays::Add, true);
	vm->BindNativeMethod("Remove", "Overlays", papyrusOverlays::Remove, true);
	vm->BindNativeMethod("Set", "Overlays", papyrusOverlays::Set, true);
	vm->BindNativeMethod("Get", "Overlays", papyrusOverlays::Get, true);
	vm->BindNativeMethod("RemoveAll", "Overlays", papyrusOverlays::RemoveAll, true);
	vm->BindNativeMethod("GetAll", "Overlays", papyrusOverlays::GetAll, true);
	vm->BindNativeMethod("Update", "Overlays", papyrusOverlays::Update, true);
	vm->BindNativeMethod("ClearAll", "Overlays", papyrusOverlays::ClearAll, true);
}
