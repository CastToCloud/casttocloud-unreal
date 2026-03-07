// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcSharedConfigurationSettings.generated.h"

/**
 * Serializable Pair containing a BuildConfiguration & BuildTarget
 */
USTRUCT()
struct CASTTOCLOUDSHARED_API FCtcSharedConfigurationFlavourPair
{
	GENERATED_BODY()

	FCtcSharedConfigurationFlavourPair() = default;
	FCtcSharedConfigurationFlavourPair(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);

	UPROPERTY()
	uint8 BuildConfiguration = 0;

	UPROPERTY()
	uint8 BuildTargetType = 0;

	EBuildConfiguration GetBuildConfiguration() const;
	EBuildTargetType GetBuildTargetType() const;
};

/**
 * Maintains an underlying list of FlavourPair plus some helper methods to determine current settings state
 */
USTRUCT()
struct CASTTOCLOUDSHARED_API FCtcSharedConfigurationSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCtcSharedConfigurationFlavourPair> Flavors;

	bool IsCurrentConfigurationAllowed() const;
	bool HasFlavour(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType) const;
	void AddFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);
	void RemoveFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);
};

static FCtcSharedConfigurationSettings PackagedDedicatedServer = FCtcSharedConfigurationSettings(
	{.Flavors = {
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Server),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Server),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Development, EBuildTargetType::Server),
	}}
);

static FCtcSharedConfigurationSettings PackagedGameClients = FCtcSharedConfigurationSettings(
	{.Flavors = {
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Game),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Game),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Development, EBuildTargetType::Game),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Client),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Client),
		FCtcSharedConfigurationFlavourPair(EBuildConfiguration::Development, EBuildTargetType::Client),
	}}
);
