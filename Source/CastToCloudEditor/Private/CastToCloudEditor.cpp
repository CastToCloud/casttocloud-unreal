// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CastToCloudEditor.h"

#include <Editor.h>
#include <HttpRequestHandler.h>
#include <HttpServerModule.h>
#include <IHttpRouter.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/EngineVersionComparison.h>
#include <PropertyEditorModule.h>
#include <Settings/ProjectPackagingSettings.h>
#include <ToolMenus.h>

#include "CtcAnalyticsEditorSubsystem.h"
#include "CtcApiKeyCustomization.h"
#include "CtcConfigurationSettings.h"
#include "CtcConfigurationSettingsCustomization.h"
#include "CtcSharedLog.h"
#include "CtcSharedSettings.h"
#include "CtcSharedSettingsDetailsCustomization.h"


TMap<FString, FCastToCloudEditorModule::FOnHttpRequestReceived> FCastToCloudEditorModule::OnHttpRequestReceived;

void FCastToCloudEditorModule::RegisterHttpRequestHandler(const FString& Message, FOnHttpRequestReceived Delegate)
{
	OnHttpRequestReceived.Emplace(Message, Delegate);
}

void FCastToCloudEditorModule::UnregisterHttpRequestHandler(const FString& Message)
{
	OnHttpRequestReceived.Remove(Message);
}

void FCastToCloudEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FCtcConfigurationSettings::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCtcConfigurationSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FCtcApiKey::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCtcApiKeyCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCtcSharedSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCtcSharedSettingsDetailsCustomization::MakeInstance));

	RemovePublicKeyFromPackage();

	RegisterToolbarExtension();

	if (!IsRunningCookCommandlet())
	{
		StartHttpServer();
	}
}

void FCastToCloudEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FCtcConfigurationSettings::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomPropertyTypeLayout(FCtcApiKey::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(UCtcSharedSettings::StaticClass()->GetFName());
	}

	UnregisterToolbarExtension();

	if (!IsRunningCookCommandlet())
	{
		StopHttpServer();
	}
}

void FCastToCloudEditorModule::RemovePublicKeyFromPackage()
{
	const FName PrivateApiKeyVariable = GET_MEMBER_NAME_CHECKED(UCtcSharedSettings, DeveloperApiKey);
	const FString PrivateApiKey = PrivateApiKeyVariable.ToString();

	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
	if (!PackagingSettings->IniKeyDenylist.Contains(PrivateApiKey))
	{
		PackagingSettings->IniKeyDenylist.Add(PrivateApiKey);
		PackagingSettings->TryUpdateDefaultConfigFile();

		UE_LOG(LogCtcShared, Display, TEXT("Added %s to IniKeyDenylist. Please submit the file"), *PrivateApiKey);
	}
}

void FCastToCloudEditorModule::RegisterToolbarExtension()
{
	FToolMenuOwnerScoped OwnerScoped(this);

#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 6, 0)
	const FName MenuToExtend = "LevelEditor.ViewportToolbar.Settings";
#else
	const FName MenuToExtend = "LevelEditor.LevelEditorToolBar.LevelToolbarQuickSettings";
#endif

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuToExtend);
	FToolMenuSection& SettingsSection = Menu->AddSection(FName("CastToCloud"), INVTEXT("Cast To Cloud"));

	FToolMenuEntry& Entry = SettingsSection.AddMenuEntry(
		FName("UploadBackground"),
		INVTEXT("Upload Background"),
		INVTEXT("Upload the current viewport as a screenshot to be used as an analytics background."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Plus")),
		FExecuteAction::CreateLambda(
			[]()
			{
				GEditor->GetEditorSubsystem<UCtcAnalyticsEditorSubsystem>()->UploadEventsBackground();
			}
		)
	);
}

void FCastToCloudEditorModule::UnregisterToolbarExtension()
{
	UToolMenus::UnregisterOwner(this);
}

void FCastToCloudEditorModule::StartHttpServer()
{
	static const FString Endpoint = TEXT("/casttocloud");
	static constexpr int32 Port = 9998;

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	TSharedPtr<IHttpRouter> HttpRouter = HttpServerModule.GetHttpRouter(Port, true);

	EHttpServerRequestVerbs Verbs = EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_OPTIONS;
	FHttpRequestHandler RequestCallback = FHttpRequestHandler::CreateRaw(this, &FCastToCloudEditorModule::HandleRequestReceived);
	HttpRouter->BindRoute(FHttpPath(Endpoint), Verbs, RequestCallback);

	HttpServerModule.StartAllListeners();
}

void FCastToCloudEditorModule::StopHttpServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpServerModule.StopAllListeners();
}

bool FCastToCloudEditorModule::HandleRequestReceived(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	auto SendResponse = [OnComplete](TUniquePtr<FHttpServerResponse> Response)
	{
		const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {Settings->DashboardUrl});
		Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), {TEXT("GET, POST, OPTIONS")});
		Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), {TEXT("Content-Type")});

		OnComplete(MoveTemp(Response));
	};

	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		SendResponse(FHttpServerResponse::Ok());
		return true;
	}

	const FString* Message = Request.QueryParams.Find("message");
	if (!Message || *Message == TEXT("version"))
	{
		const FString PluginVersion = IPluginManager::Get().FindPlugin(TEXT("CastToCloud"))->GetDescriptor().VersionName;
		const FString VersionPayload = FString::Printf(TEXT("{\"version\": \"%s\"}"), *PluginVersion);

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(VersionPayload, FString(TEXT("application/json")));
		SendResponse(MoveTemp(Response));
		return true;
	}

	if (FOnHttpRequestReceived* Handler = OnHttpRequestReceived.Find(*Message))
	{
		TUniquePtr<FHttpServerResponse> Response = Handler->Execute(Request);
		SendResponse(MoveTemp(Response));
		return true;
	}

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Error(EHttpServerResponseCodes::NotSupported, TEXT("Message not bound."));
	SendResponse(MoveTemp(Response));
	return true;
}

IMPLEMENT_MODULE(FCastToCloudEditorModule, CastToCloudEditor)