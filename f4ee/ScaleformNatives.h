#pragma once


class F4EEScaleform_LoadPreset : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SavePreset : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_ReadPreset : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetCurrentBoneRegionID : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_AllowTextInput : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetExternalFiles : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetBodySliders : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetBodyMorph : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_CloneBodyMorphs : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_UpdateBodyMorphs : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetOverlays : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetOverlayTemplates : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_CreateOverlay : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_DeleteOverlay : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetOverlayData : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_ReorderOverlay : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_UpdateOverlays : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_CloneOverlays : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetEquippedItems : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_UnequipItems : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_EquipItems : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetSkinOverrides : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetSkinOverride : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetSkinOverride : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_UpdateSkinOverride : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_CloneSkinOverride : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetSkinColor : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetSkinColor : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_GetExtraColor : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};

class F4EEScaleform_SetExtraColor : public GFx::FunctionHandler
{
public:
	virtual void	Call(const Params& params);
};