// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcConfigurationSettings.generated.h"

/**
 * Serializable Pair containing a BuildConfiguration & BuildTarget
 */
USTRUCT()
struct CASTTOCLOUD_API FCtcConfigurationFlavourPair
{
	GENERATED_BODY()

	FCtcConfigurationFlavourPair() = default;
	FCtcConfigurationFlavourPair(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);

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
struct CASTTOCLOUD_API FCtcConfigurationSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCtcConfigurationFlavourPair> Flavors;

	bool IsCurrentConfigurationAllowed() const;
	bool HasFlavour(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType) const;
	void AddFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);
	void RemoveFlavor(EBuildConfiguration InConfiguration, EBuildTargetType InTargetType);
};

static FCtcConfigurationSettings ProductionDedicatedServer = FCtcConfigurationSettings(
	{.Flavors = {
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Server),
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Server),
	 }}
);

static FCtcConfigurationSettings ProductionGameClients = FCtcConfigurationSettings(
	{.Flavors = {
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Game),
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Game),
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Shipping, EBuildTargetType::Client),
		 FCtcConfigurationFlavourPair(EBuildConfiguration::Test, EBuildTargetType::Client),
	 }}
);