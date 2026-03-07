// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedConfigurationSettings.h"

#include <Misc/App.h>

// TODO: Make the button disable for impossible configurations, such a Shipping Editor

FCtcSharedConfigurationFlavourPair::FCtcSharedConfigurationFlavourPair(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	BuildConfiguration = static_cast<uint8>(InConfiguration);
	BuildTargetType = static_cast<uint8>(InTargetType);
}

EBuildConfiguration FCtcSharedConfigurationFlavourPair::GetBuildConfiguration() const
{
	return static_cast<EBuildConfiguration>(BuildConfiguration);
}

EBuildTargetType FCtcSharedConfigurationFlavourPair::GetBuildTargetType() const
{
	return static_cast<EBuildTargetType>(BuildTargetType);
}

bool FCtcSharedConfigurationSettings::IsCurrentConfigurationAllowed() const
{
	return HasFlavour(FApp::GetBuildConfiguration(), FApp::GetBuildTargetType());
}

bool FCtcSharedConfigurationSettings::HasFlavour(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType) const
{
	const bool bFound = Flavors.ContainsByPredicate(
		[InConfiguration, InTargetType](const FCtcSharedConfigurationFlavourPair& Flavor)
		{
			return InConfiguration == Flavor.GetBuildConfiguration() && InTargetType == Flavor.GetBuildTargetType();
		}
	);

	return bFound;
}

void FCtcSharedConfigurationSettings::AddFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	Flavors.Add(FCtcSharedConfigurationFlavourPair{InConfiguration, InTargetType});
}

void FCtcSharedConfigurationSettings::RemoveFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType)
{
	Flavors.RemoveAll(
		[InConfiguration, InTargetType](const FCtcSharedConfigurationFlavourPair& Flavor)
		{
			return InConfiguration == Flavor.GetBuildConfiguration() && InTargetType == Flavor.GetBuildTargetType();
		}
	);
}
