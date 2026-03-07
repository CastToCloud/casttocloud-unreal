// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <IDetailCustomization.h>

class FCtcSharedSettingsDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// ~Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~End IDetailCustomization interface
};
