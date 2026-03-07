// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <HttpResultCallback.h>
#include <HttpServerRequest.h>
#include <Modules/ModuleManager.h>

class CASTTOCLOUDEDITOR_API FCtcEditorModule : public IModuleInterface
{
public:
	using FOnHttpRequestReceived = TDelegate<TUniquePtr<FHttpServerResponse>(const FHttpServerRequest& Request)>;
	static void RegisterHttpRequestHandler(const FString& Message, FOnHttpRequestReceived Delegate);
	static void UnregisterHttpRequestHandler(const FString& Message);

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void GeneratePreInitConfig();
	void RemovePublicKeyFromPackage();

	void StartHttpServer();
	void StopHttpServer();
	/*
	 * Callback executed when an HTTP Request is received by our endpoint
	 */
	bool HandleRequestReceived(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	static TMap<FString, FOnHttpRequestReceived> OnHttpRequestReceived;
};
