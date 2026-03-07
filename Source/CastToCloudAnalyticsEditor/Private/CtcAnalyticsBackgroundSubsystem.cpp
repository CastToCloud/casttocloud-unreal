// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsBackgroundSubsystem.h"

#include <AutomationBlueprintFunctionLibrary.h>
#include <Dom/JsonObject.h>
#include <EditorModeManager.h>
#include <Engine/WorldInitializationValues.h>
#include <Framework/Application/SlateApplication.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <Framework/Notifications/NotificationManager.h>
#include <HttpModule.h>
#include <IImageWrapper.h>
#include <IImageWrapperModule.h>
#include <ISinglePropertyView.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <LevelEditorSubsystem.h>
#include <Misc/Base64.h>
#include <Misc/MessageDialog.h>
#include <Modules/ModuleManager.h>
#include <Runtime/Launch/Resources/Version.h>
#include <SEditorViewport.h>
#include <SceneView.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>
#include <Slate/SceneViewport.h>
#include <Widgets/Notifications/SNotificationList.h>

#include "CtcAnalyticsEditorModule.h"
#include "CtcAnalyticsLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

static FAutoConsoleCommand UploadCurrentViewportAsBackground(
	TEXT("CastToCloud.Analytics.UploadCurrentViewportAsBackground"),
	TEXT(""),
	FConsoleCommandWithArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				GEditor->GetEditorSubsystem<UCtcAnalyticsBackgroundSubsystem>()->UploadCurrentViewportAsBackground();
			}
			else if (Args.Num() == 1)
			{
				GEditor->GetEditorSubsystem<UCtcAnalyticsBackgroundSubsystem>()->UploadCurrentViewportAsBackground(Args[0]);
			}
			else
			{
				UE_LOG(LogCtcAnalytics, Error, TEXT("UploadCurrentViewportAsBackground - invalid number of arguments"));
			}
		}
	)
);

void UCtcAnalyticsBackgroundSubsystem::UploadCurrentViewportAsBackground()
{
	UploadCurrentViewportAsBackground(BackgroundName);
}

void UCtcAnalyticsBackgroundSubsystem::UploadCurrentViewportAsBackground(const FString& InName)
{
	UWorld* World = CastToCloudSharedHelpers::GetCurrentWorld();

	FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	if (ViewportClient->IsActiveViewportType(LVT_Perspective))
	{
		const FText WarningTitle = INVTEXT("Upload Background Error");
		const FText WarningMessage = INVTEXT("Background screenshots need to be take in orthographic view. Switch now?");
		const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::OkCancel, WarningMessage, WarningTitle);

		if (Choice == EAppReturnType::Ok)
		{
			ViewportClient->SetViewportType(LVT_OrthoXY);
			ViewportClient->SetViewMode(VMI_Unlit);
		}

		return;
	}

	const TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
	const TSharedPtr<SViewport> SceneViewportWidget = EditorViewportWidget->GetSceneViewport()->GetViewportWidget().Pin();

	const FBox Bounds = GetViewportBounds(SceneViewportWidget);
	const TArray<uint8> ImageData = GetScreenshotImageData(SceneViewportWidget);

	UploadDataToBackend(World, Bounds, ImageData, InName);
}

void UCtcAnalyticsBackgroundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UCtcAnalyticsBackgroundSubsystem::OnPostWorldInitialization);
	FCtcAnalyticsEditorModule::OnBuildGameplayEventsMenu.AddUObject(this, &UCtcAnalyticsBackgroundSubsystem::OnBuildGameplayEventsMenu);
}

void UCtcAnalyticsBackgroundSubsystem::OnBuildGameplayEventsMenu(FMenuBuilder& MenuBuilder)
{
	const FName BackgroundNameProperty = GET_MEMBER_NAME_CHECKED(UCtcAnalyticsBackgroundSubsystem, BackgroundName);

	FSinglePropertyParams Params;
	Params.NamePlacement = EPropertyNamePlacement::Hidden;
	Params.bHideResetToDefault = true;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const TSharedPtr<ISinglePropertyView> SinglePropertyView = PropertyEditorModule.CreateSingleProperty(this, BackgroundNameProperty, Params);

	MenuBuilder.AddWidget(SinglePropertyView.ToSharedRef(), INVTEXT("Background Name"), true);

	// clang-format off
	MenuBuilder.AddMenuEntry(
		INVTEXT("Upload Background"),
		INVTEXT("Uploads the current viewport as a background"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateUObject(this, &UCtcAnalyticsBackgroundSubsystem::UploadCurrentViewportAsBackground))
	);
	// clang-format on
}

void UCtcAnalyticsBackgroundSubsystem::OnPostWorldInitialization(UWorld* World, const FWorldInitializationValues IVS)
{
	if (World->WorldType != EWorldType::Editor)
	{
		return;
	}

	BackgroundName = World->GetMapName();
}

void UCtcAnalyticsBackgroundSubsystem::UploadDataToBackend(UWorld* World, const FBox Bounds, const TArray<uint8> ImageData, const FString& Name)
{
	const FString EncodedImage = FBase64::Encode(ImageData);

	TSharedRef<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("imageData"), EncodedImage);
	PayloadObject->SetNumberField(TEXT("startX"), Bounds.Min.X);
	PayloadObject->SetNumberField(TEXT("startY"), Bounds.Min.Y);
	PayloadObject->SetNumberField(TEXT("startZ"), Bounds.Min.Z);
	PayloadObject->SetNumberField(TEXT("endX"), Bounds.Max.X);
	PayloadObject->SetNumberField(TEXT("endY"), Bounds.Max.Y);
	PayloadObject->SetNumberField(TEXT("endZ"), Bounds.Max.Z);
	PayloadObject->SetStringField(TEXT("engineVersion"), FString::Printf(TEXT("%d.%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
	PayloadObject->SetStringField(TEXT("assetPath"), *CastToCloudSharedHelpers::GetWorldPackage(World));
	PayloadObject->SetStringField(TEXT("assetName"), Name);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	ensure(FJsonSerializer::Serialize(PayloadObject, Writer));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Settings->ApiUrl / TEXT("events/gameplay/upload-background"));
	Request->SetHeader(TEXT("X-API-Key"), Settings->DeveloperApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonString);

	Request->ProcessRequestUntilComplete();

	FText NotificationTitle = INVTEXT("Background Upload");
	FNotificationInfo Info(NotificationTitle);
	Info.bUseSuccessFailIcons = true;
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 5.0f;
	Info.ExpireDuration = 5.0f;

	FHttpResponsePtr Response = Request->GetResponse();
	const bool bSuccess = Response && EHttpResponseCodes::IsOk(Response->GetResponseCode());
	if (bSuccess)
	{
		Info.HyperlinkText = INVTEXT("View in dashboard");
		Info.Hyperlink = FSimpleDelegate::CreateLambda(
			[]()
			{
				const UCtcSharedSettings* SharedSettings = GetDefault<UCtcSharedSettings>();
				const FString NewApiKeyRedirect = FString::Printf(TEXT("%s/organizations?redirectTo=/organizations/[slug]/explore-events"), *SharedSettings->DashboardUrl);
				FPlatformProcess::LaunchURL(*NewApiKeyRedirect, nullptr, nullptr);
			}
		);
		Info.SubText = INVTEXT("Upload successful.");
	}
	else
	{
		const FString Reason = Response ? Response->GetContentAsString() : TEXT("Unknown");
		const FString ErrorMessage = FString::Printf(TEXT("Upload failed. Response: %s"), *Reason);
		Info.SubText = FText::FromString(ErrorMessage);
	}

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
}

FBox UCtcAnalyticsBackgroundSubsystem::GetViewportBounds(TSharedPtr<SViewport> InViewport) const
{
	const FVector2D TopLeft = FVector2D::Zero();
	const FVector2D BottomRight = InViewport->GetCachedGeometry().GetAbsoluteSize();

	FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags));
	FSceneView* SceneView = ViewportClient->CalcSceneView(&ViewFamily);

	FVector StartPos, StartDir;
	SceneView->DeprojectFVector2D(TopLeft, StartPos, StartDir);

	FVector EndPos, EndDir;
	SceneView->DeprojectFVector2D(BottomRight, EndPos, EndDir);

	return FBox(TArray({StartPos, EndPos}));
}

TArray<uint8> UCtcAnalyticsBackgroundSubsystem::GetScreenshotImageData(TSharedPtr<SViewport> InViewport) const
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	const bool bInitialGameView = LevelEditorSubsystem->EditorGetGameView();
	LevelEditorSubsystem->EditorSetGameView(true);
	ON_SCOPE_EXIT
	{
		LevelEditorSubsystem->EditorSetGameView(bInitialGameView);
	};

	FEditorModeTools& EditorModeTools = GLevelEditorModeTools();
	const bool bInitialViewportUI = EditorModeTools.IsViewportUIHidden();
	EditorModeTools.SetHideViewportUI(true);
	ON_SCOPE_EXIT
	{
		EditorModeTools.SetHideViewportUI(bInitialViewportUI);
	};

	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	for (FEditorViewportClient* Viewport : GEditor->GetAllViewportClients())
	{
		Viewport->bNeedsRedraw = true;
		GEditor->UpdateSingleViewportClient(Viewport, true, false);
	}

	// TODO: It would be nice to give the user a selection tool so they can have a section of the viewport if they want.
	TArray<FColor> ColorData;
	FIntVector Size;
	FSlateApplication::Get().TakeScreenshot(InViewport.ToSharedRef(), ColorData, Size);

	IImageWrapperModule* ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);
	ImageWrapper->SetRaw(ColorData.GetData(), ColorData.GetAllocatedSize(), Size.X, Size.Y, ERGBFormat::BGRA, 8);

	TArray64<uint8> RawImageData = ImageWrapper->GetCompressed();

	TArray<uint8> ImageData;
	ImageData.Append(RawImageData.GetData(), RawImageData.Num());

	return ImageData;
}
