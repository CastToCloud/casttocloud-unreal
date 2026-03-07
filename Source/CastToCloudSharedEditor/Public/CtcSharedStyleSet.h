// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Styling/SlateStyle.h>

/**
 * The style class that contains UI elements specific to the CastToCloud plugin.
 */
class FCtcSharedStyleSet : public FSlateStyleSet
{
public:
	/**
	 * Access the singleton instance for this SlateStyle
	 */
	static const FCtcSharedStyleSet& Get();
	/**
	 * Creates and Registers the plugin SlateStyle
	 */
	static void Initialize();
	/**
	 * Unregisters the plugin SlateStyle
	 */
	static void Shutdown();

private:
	FCtcSharedStyleSet();

	// Begin FSlateStyleSet Interface
	virtual const FName& GetStyleSetName() const override;
	// End FSlateStyleSet Interface

	/**
	 * Unique name for this SlateStyle
	 */
	static FName StyleName;
	/**
	 * Singleton instances of this SlateStyle.
	 */
	static TUniquePtr<FCtcSharedStyleSet> Inst;
};
