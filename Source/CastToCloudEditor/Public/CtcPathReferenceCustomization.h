// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <IPropertyTypeCustomization.h>

class FCtcPathReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:
	// ~Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	// ~End IPropertyTypeCustomization interface

	FString GetBrowseDirectory() const;
	/**
	 * Gets the file path.
	 */
	FString GetFilePath() const;
	/**
	 * Called when a path is selected.
	 */
	void OnFilePathPicked(const FString& PickedPath);
	/**
	 * Pointer to the string that will be seet when changing the path
	 */
	TSharedPtr<IPropertyHandle> PathStringProperty;
	/*
	 * Directory Path to use when first opening the desktop browser
	 */
	TOptional<FString> FallbackBrowseDirectory;
};
