// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedStyleSet.h"

#include <Interfaces/IPluginManager.h>
#include <Styling/AppStyle.h>
#include <Styling/SlateStyleRegistry.h>

FName FCtcSharedStyleSet::StyleName = FName("CtcSharedStyleSet");
TUniquePtr<FCtcSharedStyleSet> FCtcSharedStyleSet::Inst = nullptr;

const FName& FCtcSharedStyleSet::GetStyleSetName() const
{
	return StyleName;
}

const FCtcSharedStyleSet& FCtcSharedStyleSet::Get()
{
	ensure(Inst.IsValid());
	return *Inst;
}

void FCtcSharedStyleSet::Initialize()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FCtcSharedStyleSet>(new FCtcSharedStyleSet);
	}
}

void FCtcSharedStyleSet::Shutdown()
{
	if (Inst.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Inst.Get());
		Inst.Reset();
	}
}

FCtcSharedStyleSet::FCtcSharedStyleSet() : FSlateStyleSet(StyleName)
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	FSlateStyleSet::SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("CastToCloud"))->GetBaseDir() / TEXT("Resources"));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}
