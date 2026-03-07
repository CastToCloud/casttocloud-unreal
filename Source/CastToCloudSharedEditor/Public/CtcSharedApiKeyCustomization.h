// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <IPropertyTypeCustomization.h>
#include <Interfaces/IHttpRequest.h>

#include "CtcSharedApiKeyCustomization.generated.h"

USTRUCT()
struct FCtcSharedApiKeyPermissionObject
{
	GENERATED_BODY()

	UPROPERTY()
	FString PermissionScope;

	UPROPERTY()
	FString PermissionSensitivity;
};

USTRUCT()
struct FCtcSharedApiKeyPermissionResponse
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCtcSharedApiKeyPermissionObject> Permissions;
};

struct FCtcSharedApiKeyStatus
{
	FCtcSharedApiKeyStatus();

	void Reset();
	bool IsEmpty() const;
	FText GetDisplayText() const;
	FLinearColor GetDisplayColor() const;

	TValueOrError<FString, FString> Result;
};

class FCtcSharedApiKeyCustomization : public IPropertyTypeCustomization, public FTickableEditorObject
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
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FCtcSharedApiKeyCustomization, STATGROUP_Tickables); }
	// ~End FTickableEditorObject interface

	void SendApiKeyPermissionsRequest(const FString& ApiKey);
	void OnApiKeyPermissionsRetrieved(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ApiKeyUsed);

	TOptional<FString> ApiKeyInVerificationProcess;
	FCtcSharedApiKeyStatus ApiKeyStatus;

	TSharedPtr<SEditableTextBox> ApiKeyInputTextBox;
	TOptional<float> TimeToPermissionRequest = {};
	FString ApiKeyPreviousValue = TEXT("");

	TArray<FString> AllowedSensitivities;
	bool bShowPassword = false;
};
