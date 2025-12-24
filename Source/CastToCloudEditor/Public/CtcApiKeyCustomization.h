// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <IPropertyTypeCustomization.h>
#include <Interfaces/IHttpRequest.h>

#include "CtcApiKeyCustomization.generated.h"

USTRUCT()
struct FCtcApiKeyPermissionObject
{
	GENERATED_BODY()

	UPROPERTY()
	FString PermissionScope;

	UPROPERTY()
	FString PermissionSensitivity;
};

USTRUCT()
struct FCtcApiKeyPermissionResponse
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCtcApiKeyPermissionObject> Permissions;
};

struct FCtcApiKeyStatus
{
	FCtcApiKeyStatus();

	void Reset();
	bool IsEmpty() const;
	FText GetDisplayText() const;
	FLinearColor GetDisplayColor() const;

	TValueOrError<FString, FString> Result;
};

class FCtcApiKeyCustomization : public IPropertyTypeCustomization, public FTickableEditorObject
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:
	// ~Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructProperty, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructProperty, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	// ~End IPropertyTypeCustomization interface

	// ~Begin FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FCtcApiKeyCustomization, STATGROUP_Tickables); }
	// ~End FTickableEditorObject interface

	void SendApiKeyPermissionsRequest(const FString& ApiKey);
	void OnApiKeyPermissionsRetrieved(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ApiKeyUsed);

	TOptional<FString> ApiKeyInVerificationProcess;
	FCtcApiKeyStatus ApiKeyStatus;

	TSharedPtr<SEditableTextBox> ApiKeyInputTextBox;
	TOptional<float> TimeToPermissionRequest = {};
	FString ApiKeyPreviousValue = TEXT("");

	TArray<FString> AllowedSensitivities;
	bool bShowPassword = false;
};