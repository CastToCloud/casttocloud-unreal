// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedEditorModule.h"

#include <Editor.h>
#include <HttpRequestHandler.h>
#include <HttpServerModule.h>
#include <IHttpRouter.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/ConfigCacheIni.h>
#include <PropertyEditorModule.h>
#include <Settings/ProjectPackagingSettings.h>
#include <ToolMenus.h>

#include "CtcSharedApiKeyCustomization.h"
#include "CtcSharedConfigurationSettings.h"
#include "CtcSharedConfigurationSettingsCustomization.h"
#include "CtcSharedLog.h"
#include "CtcSharedPathReference.h"
#include "CtcSharedPathReferenceCustomization.h"
#include "CtcSharedSettings.h"
#include "CtcSharedSettingsDetailsCustomization.h"


TMap<FString, FCtcSharedEditorModule::FOnHttpRequestReceived> FCtcSharedEditorModule::OnHttpRequestReceived;

void FCtcSharedEditorModule::RegisterHttpRequestHandler(const FString& Message, FOnHttpRequestReceived Delegate)
{
	OnHttpRequestReceived.Emplace(Message, Delegate);
}

void FCtcSharedEditorModule::UnregisterHttpRequestHandler(const FString& Message)
{
	OnHttpRequestReceived.Remove(Message);
}

void FCtcSharedEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FCtcSharedConfigurationSettings::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCtcSharedConfigurationSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FCtcSharedPathReference::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCtcSharedPathReferenceCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FCtcSharedApiKey::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCtcSharedApiKeyCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UCtcSharedSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCtcSharedSettingsDetailsCustomization::MakeInstance));

	GeneratePreInitConfig();

	RemovePublicKeyFromPackage();

	if (!IsRunningCookCommandlet())
	{
		StartHttpServer();
	}
}

void FCtcSharedEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FCtcSharedConfigurationSettings::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomPropertyTypeLayout(FCtcSharedPathReference::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomPropertyTypeLayout(FCtcSharedApiKey::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(UCtcSharedSettings::StaticClass()->GetFName());
	}

	if (!IsRunningCookCommandlet())
	{
		StopHttpServer();
	}
}

// Backwards-compatible equivalent of UObject::SaveConfig(FSaveConfigContext) with per-property filtering (introduced in UE 5.6).
static void SaveTaggedConfigProperties(UObject* Object, const FName MetaDataTag, const FString& Filename)
{
	FConfigCacheIni TempConfig(EConfigCacheType::DiskBacked);
	TempConfig.AddNewBranch(Filename);

	const FString Section = Object->GetClass()->GetPathName();
	const int32 PortFlags = EPropertyPortFlags::PPF_SerializedAsImportText;

	for (FProperty* Property = Object->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config) || !Property->HasMetaData(MetaDataTag))
		{
			continue;
		}

		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FString Key = Property->GetName();
			if (Property->ArrayDim != 1)
			{
				Key = FString::Printf(TEXT("%s[%d]"), *Property->GetName(), Index);
			}

			FString Value;
			Property->ExportText_InContainer(Index, Value, Object, Object, Object, PortFlags);
			TempConfig.SetString(*Section, *Key, *Value, *Filename);
		}
	}

	TempConfig.Flush(false, *Filename);
}

void FCtcSharedEditorModule::GeneratePreInitConfig()
{
	if (!IsRunningCookCommandlet())
	{
		UE_LOG(LogCtcShared, Verbose, TEXT("Editor instance is not cooking, no need to generate PreInit configs."));
		return;
	}

	FString ConfigForPreInit = FPaths::ProjectIntermediateDir() / TEXT("CastToCloud") / TEXT("PreInitCastToCloud.ini");
	UE_LOG(LogCtcShared, Display, TEXT("Editor instance is cooking, generating PreInit configs at %s."), *ConfigForPreInit);

	UCtcSharedSettings* SharedSettings = GetMutableDefault<UCtcSharedSettings>();

	SaveTaggedConfigProperties(SharedSettings, TEXT("CopyToPreInitIni"), ConfigForPreInit);
}

void FCtcSharedEditorModule::RemovePublicKeyFromPackage()
{
	const FName PrivateApiKeyVariable = GET_MEMBER_NAME_CHECKED(UCtcSharedSettings, DeveloperApiKey);
	const FString PrivateApiKey = PrivateApiKeyVariable.ToString();

	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
	if (!PackagingSettings->IniKeyDenylist.Contains(PrivateApiKey))
	{
		// TODO: This should also check out this file for submission too.
		PackagingSettings->IniKeyDenylist.Add(PrivateApiKey);
		PackagingSettings->TryUpdateDefaultConfigFile();

		UE_LOG(LogCtcShared, Display, TEXT("Added %s to IniKeyDenylist. Please submit the file"), *PrivateApiKey);
	}
}

void FCtcSharedEditorModule::StartHttpServer()
{
	static const FString Endpoint = TEXT("/casttocloud");
	static constexpr int32 Port = 9998;

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	TSharedPtr<IHttpRouter> HttpRouter = HttpServerModule.GetHttpRouter(Port, true);

	EHttpServerRequestVerbs Verbs = EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_OPTIONS;
	FHttpRequestHandler RequestCallback = FHttpRequestHandler::CreateRaw(this, &FCtcSharedEditorModule::HandleRequestReceived);
	HttpRouter->BindRoute(FHttpPath(Endpoint), Verbs, RequestCallback);

	HttpServerModule.StartAllListeners();
}

void FCtcSharedEditorModule::StopHttpServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpServerModule.StopAllListeners();
}

bool FCtcSharedEditorModule::HandleRequestReceived(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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

IMPLEMENT_MODULE(FCtcSharedEditorModule, CastToCloudSharedEditor)
