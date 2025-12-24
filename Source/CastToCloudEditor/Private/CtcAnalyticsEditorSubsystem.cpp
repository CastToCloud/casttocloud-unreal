// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsEditorSubsystem.h"

#include <AutomationBlueprintFunctionLibrary.h>
#include <Dom/JsonObject.h>
#include <EditorModeManager.h>
#include <Framework/Application/SlateApplication.h>
#include <HttpModule.h>
#include <IImageWrapper.h>
#include <IImageWrapperModule.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <LevelEditorSubsystem.h>
#include <Misc/Base64.h>
#include <Modules/ModuleManager.h>
#include <Runtime/Launch/Resources/Version.h>
#include <SEditorViewport.h>
#include <SceneView.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>
#include <Slate/SceneViewport.h>

#include "CtcSharedSettings.h"

static FAutoConsoleCommandWithWorldAndArgs UploadAnalyticsBackgroundCommand(
	TEXT("CastToCloud.UploadAnalyticsBackground"),
	TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			GEditor->GetEditorSubsystem<UCtcAnalyticsEditorSubsystem>()->UploadEventsBackground(InWorld);
		}
	)
);

void UCtcAnalyticsEditorSubsystem::UploadEventsBackground(UWorld* World)
{
	if (!World)
	{
		World = GWorld;
	}

	TSharedPtr<SViewport> SceneViewportWidget = GetScreenshotViewport();

	const FBox Bounds = GetViewportBounds(SceneViewportWidget);
	const TArray<uint8> ImageData = GetScreenshotImageData(SceneViewportWidget);

	UploadDataToBackend(World, Bounds, ImageData);
}

TSharedPtr<SViewport> UCtcAnalyticsEditorSubsystem::GetScreenshotViewport() const
{
	FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
	TWeakPtr<SViewport> SceneViewportWidget = EditorViewportWidget->GetSceneViewport()->GetViewportWidget();

	return SceneViewportWidget.Pin();
}

FBox UCtcAnalyticsEditorSubsystem::GetViewportBounds(TSharedPtr<SViewport> InViewport) const
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

TArray<uint8> UCtcAnalyticsEditorSubsystem::GetScreenshotImageData(TSharedPtr<SViewport> InViewport) const
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

void UCtcAnalyticsEditorSubsystem::UploadDataToBackend(UWorld* World, const FBox Bounds, const TArray<uint8> ImageData)
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
	PayloadObject->SetStringField(TEXT("assetPath"), World->GetPackage()->GetPathName());
	PayloadObject->SetStringField(TEXT("assetName"), World->GetMapName());

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	ensure(FJsonSerializer::Serialize(PayloadObject, Writer));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Settings->ApiUrl / TEXT("events/upload-background"));
	Request->SetHeader(TEXT("X-API-Key"), Settings->DeveloperApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonString);

	Request->ProcessRequestUntilComplete();

	// TODO: Improve status reporting.
	if (FHttpResponsePtr Response = Request->GetResponse())
	{
		UE_LOG(LogTemp, Warning, TEXT("Response: %s"), *Response->GetContentAsString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No response received."));
	}
}
