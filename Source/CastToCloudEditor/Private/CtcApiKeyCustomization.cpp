// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcApiKeyCustomization.h"

#include <DetailWidgetRow.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <HttpModule.h>
#include <IDetailChildrenBuilder.h>
#include <Interfaces/IHttpResponse.h>
#include <JsonObjectConverter.h>
#include <Widgets/Input/SComboButton.h>
#include <Widgets/Input/SEditableTextBox.h>

#include "CtcApiKey.h"
#include "CtcSharedSettings.h"

FCtcApiKeyStatus::FCtcApiKeyStatus() : Result(MakeValue(TEXT("")))
{
}

void FCtcApiKeyStatus::Reset()
{
	Result = MakeValue(TEXT(""));
}

bool FCtcApiKeyStatus::IsEmpty() const
{
	return Result.HasValue() && Result.GetValue() == TEXT("");
}

FText FCtcApiKeyStatus::GetDisplayText() const
{
	const FString ResultString = Result.HasValue() ? Result.GetValue() : Result.GetError();
	return FText::FromString(ResultString);
}

FLinearColor FCtcApiKeyStatus::GetDisplayColor() const
{
	return Result.HasValue() ? FLinearColor::Green : FLinearColor::Red;
}

TSharedRef<IPropertyTypeCustomization> FCtcApiKeyCustomization::MakeInstance()
{
	return MakeShared<FCtcApiKeyCustomization>();
}

void FCtcApiKeyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructProperty, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FCtcApiKeyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructProperty, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FString& SensitivityMetaData = StructProperty->GetMetaData("AllowedSensitivity");
	SensitivityMetaData.ParseIntoArray(AllowedSensitivities, TEXT("+"));

	TSharedPtr<IPropertyHandle> ApiKeyProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCtcApiKey, ApiKey));

	// clang-format off
	ChildBuilder.AddCustomRow(INVTEXT("ApiKey"))
	            .NameContent()
		[
			StructProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ApiKeyInputTextBox, SEditableTextBox)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.IsPassword_Lambda([this, ApiKeyProperty]()
					{
						if (bShowPassword)
						{
							return false;
						}
						
						FString Value;
						ApiKeyProperty->GetValue(Value);
						return !Value.IsEmpty() && Value != FCtcApiKey::PlaceholderKey;
					})
					.IsReadOnly_Lambda([this]()
					{
						return ApiKeyInVerificationProcess.IsSet();
					})
					.OnTextChanged_Lambda([ApiKeyProperty, this](const FText& InText)
					{
						ApiKeyProperty->SetValue(InText.ToString());
					})
					.Text_Lambda([ApiKeyProperty]()
					{
						FString Value;
						ApiKeyProperty->GetValue(Value);
						return FText::FromString(Value);
					})
					.OnTextCommitted_Lambda([ApiKeyProperty, this](const FText& InText, ETextCommit::Type Type)
					{
						ApiKeyProperty->SetValue(InText.ToString());
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.OnGetMenuContent_Lambda([this, ApiKeyProperty]()
					{
						FMenuBuilder QuickActionsMenuBuilds(true, nullptr);

						FUIAction GenerateAction(FExecuteAction::CreateLambda([this]()
						{
							const UCtcSharedSettings* SharedSettings = GetDefault<UCtcSharedSettings>();
							const FString NewApiKeyRedirect = FString::Printf(TEXT("%s/organization/redirect?to=api-keys"), *SharedSettings->DashboardUrl);
							FPlatformProcess::LaunchURL(*NewApiKeyRedirect, nullptr, nullptr);
						}));
						QuickActionsMenuBuilds.AddMenuEntry(INVTEXT("Generate New Api Key"), INVTEXT("Generate a Api Key via the web Dashboard"), FSlateIcon(), GenerateAction);

						TAttribute<FText> ShowPasswordTitle = TAttribute<FText>::CreateLambda([this]()
						{
							return bShowPassword ? INVTEXT("Hide Password") : INVTEXT("Show Password");
							
						});
						TAttribute<FText> ShowPasswordTooltip = TAttribute<FText>::CreateLambda([this]()
						{
							return bShowPassword ? INVTEXT("Hide the password") : INVTEXT("Show the password");
						});
						FUIAction ShowPasswordAction(FExecuteAction::CreateLambda([this]()
						{
							bShowPassword = !bShowPassword;
						}));
						QuickActionsMenuBuilds.AddMenuEntry(ShowPasswordTitle, ShowPasswordTooltip, FSlateIcon(), ShowPasswordAction);

						FUIAction RefreshAction(FExecuteAction::CreateLambda([this, ApiKeyProperty]()
						{
							FString Value;
							ApiKeyProperty->GetValue(Value);
							SendApiKeyPermissionsRequest(Value);
						}));
						QuickActionsMenuBuilds.AddMenuEntry(INVTEXT("Refresh Permissions"), INVTEXT("Re-evaluate the Api Key permissions"), FSlateIcon(), RefreshAction);

						return QuickActionsMenuBuilds.MakeWidget();
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					return ApiKeyStatus.GetDisplayText();
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return ApiKeyStatus.GetDisplayColor();
				})
				.Visibility_Lambda([this]()
				{
					return !ApiKeyStatus.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]
		];
	// clang-format on
}

void FCtcApiKeyCustomization::Tick(float DeltaTime)
{
	if (ApiKeyInputTextBox)
	{
		if (TimeToPermissionRequest.IsSet())
		{
			*TimeToPermissionRequest -= DeltaTime;
		}

		const FString CurrentInput = ApiKeyInputTextBox->GetText().ToString();
		if (CurrentInput != ApiKeyPreviousValue)
		{
			TimeToPermissionRequest = 0.2f;
		}

		if (TimeToPermissionRequest.IsSet() && *TimeToPermissionRequest < 0.0f)
		{
			TimeToPermissionRequest.Reset();
			SendApiKeyPermissionsRequest(CurrentInput);
		}

		ApiKeyPreviousValue = CurrentInput;
	}
}

void FCtcApiKeyCustomization::SendApiKeyPermissionsRequest(const FString& ApiKey)
{
	if (ApiKey == FCtcApiKey::PlaceholderKey || ApiKey.IsEmpty())
	{
		// No need to verify placeholder or empty keys, just return early so the user can write something in faster.
		ApiKeyStatus.Reset();
		return;
	}

	if (ApiKeyInVerificationProcess.IsSet() && ApiKeyInVerificationProcess == ApiKey)
	{
		// We already have a request in progress to verify this key.
		return;
	}

	// NOTE: This might override an in-progress key. In that case a new request will be triggered.
	//  The result of the previous request will be discarded when compared against the current value we are setting now.
	ApiKeyInVerificationProcess = ApiKey;

	const UCtcSharedSettings* SharedSettings = GetDefault<UCtcSharedSettings>();
	const FString UrlEndpoint = FString::Printf(TEXT("%s/organization/get-api-key-permissions"), *SharedSettings->ApiUrl);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb("GET");
	Request->SetURL(UrlEndpoint);
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader("X-API-Key", ApiKey);

	Request->OnProcessRequestComplete().BindRaw(this, &FCtcApiKeyCustomization::OnApiKeyPermissionsRetrieved, ApiKey);
	Request->ProcessRequest();
}

void FCtcApiKeyCustomization::OnApiKeyPermissionsRetrieved(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ApiKeyUsed)
{
	if (!ApiKeyInVerificationProcess.IsSet() || ApiKeyInVerificationProcess != ApiKeyUsed)
	{
		return;
	}

	ApiKeyInVerificationProcess.Reset();

	if (!bWasSuccessful || !Request.IsValid())
	{
		ApiKeyStatus.Result = MakeError(FString(TEXT("Failed to connect to backend.")));
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		ApiKeyStatus.Result = MakeError(FString::Printf(TEXT("Backend return error code: %s message: %s"), *LexToString(Response->GetResponseCode()), *Response->GetContentAsString()));
		return;
	}

	FCtcApiKeyPermissionResponse Result;
	const FString DataString = Response->GetContentAsString();

	const bool bConvertSuccess = FJsonObjectConverter::JsonObjectStringToUStruct(DataString, &Result, 0, 0);
	if (!bConvertSuccess)
	{
		ApiKeyStatus.Result = MakeError(FString::Printf(TEXT("Failed to parse response from backend: %s"), *Response->GetContentAsString()));
		return;
	}

	TArray<FString> ProhibitedPermissions;
	for (const auto& Permission : Result.Permissions)
	{
		if (!AllowedSensitivities.Contains(Permission.PermissionSensitivity))
		{
			ProhibitedPermissions.Add(Permission.PermissionScope);
		}
	}
	if (ProhibitedPermissions.Num() > 0)
	{
		ApiKeyStatus.Result = MakeError(FString::Printf(TEXT("Contains prohibited permissions: %s"), *FString::Join(ProhibitedPermissions, TEXT(", "))));
		return;
	}

	TArray<FString> ResultLines;
	for (const auto& Permission : Result.Permissions)
	{
		ResultLines.Add(FString::Printf(TEXT("- %s"), *Permission.PermissionScope));
	}

	const FString SuccessMessage = FString(TEXT("Key has access to:\n")) + FString::Join(ResultLines, TEXT("\n"));
	ApiKeyStatus.Result = MakeValue(SuccessMessage);
}