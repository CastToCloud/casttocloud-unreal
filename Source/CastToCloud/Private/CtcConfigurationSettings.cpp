// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcConfigurationSettings.h"

#include <Misc/App.h>

// TODO: Make the button disable for impossible configurations, such a Shipping Editor

FCtcConfigurationFlavourPair::FCtcConfigurationFlavourPair(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	BuildConfiguration = static_cast<uint8>(InConfiguration);
	BuildTargetType = static_cast<uint8>(InTargetType);
}

EBuildConfiguration FCtcConfigurationFlavourPair::GetBuildConfiguration() const
{
	return static_cast<EBuildConfiguration>(BuildConfiguration);
}

EBuildTargetType FCtcConfigurationFlavourPair::GetBuildTargetType() const
{
	return static_cast<EBuildTargetType>(BuildTargetType);
}

bool FCtcConfigurationSettings::IsCurrentConfigurationAllowed() const
{
	return HasFlavour(FApp::GetBuildConfiguration(), FApp::GetBuildTargetType());
}

bool FCtcConfigurationSettings::HasFlavour(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType) const
{
	const bool bFound = Flavors.ContainsByPredicate(
		[InConfiguration, InTargetType](const FCtcConfigurationFlavourPair& Flavor)
		{
			return InConfiguration == Flavor.GetBuildConfiguration() && InTargetType == Flavor.GetBuildTargetType();
		}
	);

	return bFound;
}

void FCtcConfigurationSettings::AddFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	Flavors.Add(FCtcConfigurationFlavourPair{InConfiguration, InTargetType});
}

void FCtcConfigurationSettings::RemoveFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	Flavors.RemoveAll(
		[InConfiguration, InTargetType](const FCtcConfigurationFlavourPair& Flavor)
		{
			return InConfiguration == Flavor.GetBuildConfiguration() && InTargetType == Flavor.GetBuildTargetType();
		}
	);
}
