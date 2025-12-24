// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcStyleSet.h"

#include <Interfaces/IPluginManager.h>
#include <Styling/AppStyle.h>
#include <Styling/SlateStyleRegistry.h>

FName FCtcStyleSet::StyleName = FName("CtcStyleSet");
TUniquePtr<FCtcStyleSet> FCtcStyleSet::Inst = nullptr;

const FName& FCtcStyleSet::GetStyleSetName() const
{
	return StyleName;
}

const FCtcStyleSet& FCtcStyleSet::Get()
{
	ensure(Inst.IsValid());
	return *Inst;
}

void FCtcStyleSet::Initialize()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FCtcStyleSet>(new FCtcStyleSet);
	}
}

void FCtcStyleSet::Shutdown()
{
	if (Inst.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Inst.Get());
		Inst.Reset();
	}
}

FCtcStyleSet::FCtcStyleSet() : FSlateStyleSet(StyleName)
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	FSlateStyleSet::SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("CastToCloud"))->GetBaseDir() / TEXT("Resources"));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}
