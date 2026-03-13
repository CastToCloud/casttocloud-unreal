// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsEditorModule.h"

#include "AssetCompilingManager.h"
#include "Commandlets/IChunkDataGenerator.h"
#include "Cooker/MPCollector.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IAssetCompilingManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "TickableEditorObject.h"
#include "UObject/ICookInfo.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCastToCloudMetricsEditor, Log, All);

namespace
{
	constexpr double CookHeartbeatIntervalSeconds = 30.0;
	constexpr int32 TraceProcessId = 1;
	bool GCastToCloudChunkFactoryRegistered = false;

	const TCHAR* BoolToString(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	const TCHAR* CookTypeToString(const UE::Cook::ECookType CookType)
	{
		switch (CookType)
		{
		case UE::Cook::ECookType::Unknown:
			return TEXT("Unknown");
		case UE::Cook::ECookType::OnTheFly:
			return TEXT("OnTheFly");
		case UE::Cook::ECookType::ByTheBook:
			return TEXT("ByTheBook");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* CookingDlcToString(const UE::Cook::ECookingDLC CookingDlc)
	{
		switch (CookingDlc)
		{
		case UE::Cook::ECookingDLC::Unknown:
			return TEXT("Unknown");
		case UE::Cook::ECookingDLC::Yes:
			return TEXT("Yes");
		case UE::Cook::ECookingDLC::No:
			return TEXT("No");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* ProcessTypeToString(const UE::Cook::EProcessType ProcessType)
	{
		switch (ProcessType)
		{
		case UE::Cook::EProcessType::SingleProcess:
			return TEXT("SingleProcess");
		case UE::Cook::EProcessType::Director:
			return TEXT("Director");
		case UE::Cook::EProcessType::Worker:
			return TEXT("Worker");
		case UE::Cook::EProcessType::AllMPCook:
			return TEXT("AllMPCook");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* CookResultToString(const UE::Cook::ECookResult CookResult)
	{
		switch (CookResult)
		{
		case UE::Cook::ECookResult::NotAttempted:
			return TEXT("NotAttempted");
		case UE::Cook::ECookResult::Succeeded:
			return TEXT("Succeeded");
		case UE::Cook::ECookResult::Failed:
			return TEXT("Failed");
		case UE::Cook::ECookResult::NeverCookPlaceholder:
			return TEXT("NeverCookPlaceholder");
		case UE::Cook::ECookResult::Invalid:
			return TEXT("Invalid");
		case UE::Cook::ECookResult::Count:
			return TEXT("Count");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* ServerEventTypeToString(const UE::Cook::FMPCollectorServerTickContext::EServerEventType EventType)
	{
		switch (EventType)
		{
		case UE::Cook::FMPCollectorServerTickContext::EServerEventType::WorkerStartup:
			return TEXT("WorkerStartup");
		default:
			return TEXT("Invalid");
		}
	}

	FString GetTargetPlatformName(const ITargetPlatform* TargetPlatform)
	{
		return TargetPlatform != nullptr ? TargetPlatform->PlatformName() : TEXT("None");
	}

	FString JoinTargetPlatforms(TConstArrayView<const ITargetPlatform*> Platforms)
	{
		TArray<FString> PlatformNames;
		PlatformNames.Reserve(Platforms.Num());

		for (const ITargetPlatform* Platform : Platforms)
		{
			PlatformNames.Add(GetTargetPlatformName(Platform));
		}

		return PlatformNames.Num() > 0 ? FString::Join(PlatformNames, TEXT(", ")) : TEXT("None");
	}

	FString JoinTargetPlatforms(const TArray<const ITargetPlatform*>& Platforms)
	{
		return JoinTargetPlatforms(TConstArrayView<const ITargetPlatform*>(Platforms));
	}

	FString JoinChunkIds(const TConstArrayView<int32> ChunkIds)
	{
		TArray<FString> ChunkIdStrings;
		ChunkIdStrings.Reserve(ChunkIds.Num());

		for (const int32 ChunkId : ChunkIds)
		{
			ChunkIdStrings.Add(LexToString(ChunkId));
		}

		return ChunkIdStrings.Num() > 0 ? FString::Join(ChunkIdStrings, TEXT(", ")) : TEXT("None");
	}

	FString JoinInstigatorChain(const TArray<UE::Cook::FInstigator>& InstigatorChain)
	{
		TArray<FString> InstigatorStrings;
		InstigatorStrings.Reserve(InstigatorChain.Num());

		for (const UE::Cook::FInstigator& Instigator : InstigatorChain)
		{
			InstigatorStrings.Add(Instigator.ToString());
		}

		return InstigatorStrings.Num() > 0 ? FString::Join(InstigatorStrings, TEXT(" -> ")) : TEXT("None");
	}

	FString BuildCookPackageKey(const FString& PackageName, const TCHAR* TargetFilename)
	{
		return FString::Printf(TEXT("%s@%s"), *PackageName, TargetFilename);
	}

	int64 SecondsToTraceMicroseconds(const double Seconds)
	{
		return static_cast<int64>(FMath::Max(0.0, Seconds) * 1000.0 * 1000.0);
	}

	const TCHAR* LoadTypeToString(const bool bSynchronous)
	{
		return bSynchronous ? TEXT("Sync") : TEXT("Async");
	}

	const TCHAR* ModuleChangeReasonToString(const EModuleChangeReason Reason)
	{
		switch (Reason)
		{
		case EModuleChangeReason::ModuleLoaded:
			return TEXT("ModuleLoaded");
		case EModuleChangeReason::ModuleUnloaded:
			return TEXT("ModuleUnloaded");
		case EModuleChangeReason::PluginDirectoryChanged:
			return TEXT("PluginDirectoryChanged");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* CompiledInStatusToString(const ECompiledInUObjectsRegisteredStatus Status)
	{
		switch (Status)
		{
		case ECompiledInUObjectsRegisteredStatus::Delayed:
			return TEXT("Delayed");
		case ECompiledInUObjectsRegisteredStatus::PreCDO:
			return TEXT("PreCDO");
		case ECompiledInUObjectsRegisteredStatus::PostCDO:
			return TEXT("PostCDO");
		default:
			return TEXT("Unknown");
		}
	}

	FString DescribeCookSession(UE::Cook::ICookInfo& CookInfo)
	{
		FString Summary = FString::Printf(
			TEXT("cookType=%s processType=%s incremental=%s dlc=%s platforms=[%s]"),
			CookTypeToString(CookInfo.GetCookType()),
			ProcessTypeToString(CookInfo.GetProcessType()),
			BoolToString(CookInfo.IsIncremental()),
			CookingDlcToString(CookInfo.GetCookingDLC()),
			*JoinTargetPlatforms(CookInfo.GetSessionPlatforms())
		);

		const FString DlcName = CookInfo.GetDLCName();
		if (!DlcName.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" dlcName=%s"), *DlcName);
		}

		const FString BasedOnReleaseVersion = CookInfo.GetBasedOnReleaseVersion();
		if (!BasedOnReleaseVersion.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" basedOnReleaseVersion=%s"), *BasedOnReleaseVersion);
		}

		const FString CreateReleaseVersion = CookInfo.GetCreateReleaseVersion();
		if (!CreateReleaseVersion.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" createReleaseVersion=%s"), *CreateReleaseVersion);
		}

		return Summary;
	}

	struct FCtcActivePackageTraceSpan
	{
		FString PackageName;
		FString PlatformName;
		FString TargetFilename;
		FString Instigator;
		FString InstigatorChain;
		FString CookType;
		bool bProceduralSave = false;
		double StartSeconds = 0.0;
	};

	struct FCtcCompletedPackageTraceSpan
	{
		FString PackageName;
		FString PlatformName;
		FString TargetFilename;
		FString Instigator;
		FString InstigatorChain;
		FString CookType;
		bool bProceduralSave = false;
		int64 TimestampUs = 0;
		int64 DurationUs = 0;
		int32 ThreadId = 0;
	};

	struct FCtcLoadBatchTraceEvent
	{
		FString PackagesSample;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 PackageCount = 0;
		int32 RecursiveDepth = 0;
		bool bSynchronous = false;
	};

	struct FCtcIdleTraceEvent
	{
		FString Name;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 QueueCount = 0;
		int32 PendingCookedPlatformData = 0;
		bool bExpectedDueToSlowBuildOperations = false;
	};

	struct FCtcBlockedObjectTraceEvent
	{
		FString ObjectName;
		FString ObjectClass;
		FString PackageName;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
	};

	struct FCtcProgressTraceEvent
	{
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 CookedPackagesCount = 0;
		int32 CookPendingCount = 0;
	};

	struct FCtcActivePackageLoadTraceSpan
	{
		FString PackageName;
		bool bSynchronousRequest = false;
		double StartSeconds = 0.0;
	};

	struct FCtcCompletedPackageLoadTraceSpan
	{
		FString PackageName;
		bool bSynchronousRequest = false;
		bool bSynchronousCompletion = false;
		int64 TimestampUs = 0;
		int64 DurationUs = 0;
		int32 ThreadId = 0;
		int32 RecursiveDepth = 0;
	};

	struct FCtcActivePackageCompileTraceSpan
	{
		FString PackageName;
		double StartSeconds = 0.0;
	};

	struct FCtcCompletedPackageCompileTraceSpan
	{
		FString PackageName;
		int64 TimestampUs = 0;
		int64 DurationUs = 0;
		int32 ThreadId = 0;
	};

	struct FCtcActiveModuleTraceSpan
	{
		FString ModuleName;
		bool bCanProcessLoadedObjects = false;
		double StartSeconds = 0.0;
	};

	struct FCtcCompletedModuleTraceSpan
	{
		FString ModuleName;
		bool bCanProcessLoadedObjects = false;
		int64 TimestampUs = 0;
		int64 DurationUs = 0;
		int32 ThreadId = 0;
	};

	struct FCtcModuleTraceEvent
	{
		FString Name;
		FString ModuleName;
		FString Detail;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		bool bCanProcessLoadedObjects = false;
	};

	struct FCtcAssetCompileTraceEvent
	{
		FString AssetsSample;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 AssetCount = 0;
	};

	struct FCtcCounterTraceEvent
	{
		FString Name;
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 Value = 0;
	};

	struct FCtcAsyncLoadingFlushTraceEvent
	{
		int64 TimestampUs = 0;
		int32 ThreadId = 0;
		int32 PendingAsyncPackages = 0;
	};

	class FCtcCookChunkDataGenerator final : public IChunkDataGenerator
	{
	public:
		explicit FCtcCookChunkDataGenerator(FString InSessionSummary)
			: SessionSummary(MoveTemp(InSessionSummary))
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkGeneratorCreate] %s"),
				*SessionSummary
			);
		}

		virtual void BeginGenerateChunkDataFiles(FBeginGenerateContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkBatchStart] platform=%s chunkIds=[%s] sandbox=%p"),
				*GetTargetPlatformName(Context.GetTargetPlatform()),
				*JoinChunkIds(Context.GetCurrentChunkIds()),
				Context.GetSandboxFile()
			);
		}

		virtual void GenerateChunkDataFiles(
			const int32 InChunkId,
			const TSet<FName>& InPackagesInChunk,
			const ITargetPlatform* TargetPlatform,
			FSandboxPlatformFile* InSandboxFile,
			TArray<FString>& OutChunkFilenames
		) override
		{
			const int32 InitialFilenameCount = OutChunkFilenames.Num();

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkCookStart] chunkId=%d platform=%s packageCount=%d sandbox=%p"),
				InChunkId,
				*GetTargetPlatformName(TargetPlatform),
				InPackagesInChunk.Num(),
				InSandboxFile
			);

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkCookEnd] chunkId=%d platform=%s addedFiles=%d totalFiles=%d"),
				InChunkId,
				*GetTargetPlatformName(TargetPlatform),
				OutChunkFilenames.Num() - InitialFilenameCount,
				OutChunkFilenames.Num()
			);
		}

	private:
		FString SessionSummary;
	};

	TSharedRef<IChunkDataGenerator> CreateCastToCloudChunkDataGenerator(const UE::Cook::ICookInfo& CookInfo)
	{
		// IChunkDataGenerator exposes a const ICookInfo&, but ICookInfo accessors are non-const in this engine branch.
		UE::Cook::ICookInfo& MutableCookInfo = const_cast<UE::Cook::ICookInfo&>(CookInfo);
		return StaticCastSharedRef<IChunkDataGenerator>(MakeShared<FCtcCookChunkDataGenerator>(DescribeCookSession(MutableCookInfo)));
	}

	class FCtcCookMpCollector final : public UE::Cook::IMPCollector
	{
	public:
		virtual FGuid GetMessageType() const override
		{
			return FGuid(0x4F052865, 0xD63442C4, 0xB89FBD18, 0x1AC2A831);
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("CastToCloudMetricsCookCollector");
		}

		virtual void ServerTick(UE::Cook::FMPCollectorServerTickContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookServerTick] event=%s platforms=[%s]"),
				ServerEventTypeToString(Context.GetEventType()),
				*JoinTargetPlatforms(Context.GetPlatforms())
			);
		}

		virtual void ClientTick(UE::Cook::FMPCollectorClientTickContext& Context) override
		{
			if (!Context.IsFlush())
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookClientFlush] platforms=[%s]"),
				*JoinTargetPlatforms(Context.GetPlatforms())
			);
		}

		virtual void ServerTickPackage(UE::Cook::FMPCollectorServerTickPackageContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookPackageAssign] package=%s"),
				*Context.GetPackageName().ToString()
			);
		}

		virtual void ClientTickPackage(UE::Cook::FMPCollectorClientTickPackageContext& Context) override
		{
			TArray<FString> PlatformResults;
			PlatformResults.Reserve(Context.GetPlatformDatas().Num());

			for (const UE::Cook::FMPCollectorClientTickPackageContext::FPlatformData& PlatformData : Context.GetPlatformDatas())
			{
				PlatformResults.Add(
					FString::Printf(
						TEXT("%s=%s"),
						*GetTargetPlatformName(PlatformData.TargetPlatform),
						CookResultToString(PlatformData.CookResults)
					)
				);
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookPackageResult] package=%s results=[%s]"),
				*Context.GetPackageName().ToString(),
				PlatformResults.Num() > 0 ? *FString::Join(PlatformResults, TEXT(", ")) : TEXT("None")
			);
		}
	};

	class FCtcCookObserver final : public FTickableCookObject
	{
	public:
		void Startup()
		{
			CookStartedHandle = UE::Cook::FDelegates::CookStarted.AddRaw(this, &FCtcCookObserver::HandleCookStarted);
			CookFinishedHandle = UE::Cook::FDelegates::CookFinished.AddRaw(this, &FCtcCookObserver::HandleCookFinished);
			CookUpdateDisplayHandle = UE::Cook::FDelegates::CookUpdateDisplay.AddRaw(this, &FCtcCookObserver::HandleCookUpdateDisplay);
			CookSaveIdleHandle = UE::Cook::FDelegates::CookSaveIdle.AddRaw(this, &FCtcCookObserver::HandleCookSaveIdle);
			CookLoadIdleHandle = UE::Cook::FDelegates::CookLoadIdle.AddRaw(this, &FCtcCookObserver::HandleCookLoadIdle);
			PackageBlockedHandle = UE::Cook::FDelegates::PackageBlocked.AddRaw(this, &FCtcCookObserver::HandlePackageBlocked);
			SyncLoadPackageHandle = FCoreDelegates::OnSyncLoadPackage.AddRaw(this, &FCtcCookObserver::HandleSyncLoadPackage);
			AsyncLoadPackageHandle = FCoreDelegates::GetOnAsyncLoadPackage().AddRaw(this, &FCtcCookObserver::HandleAsyncLoadPackage);
			AsyncLoadingFlushHandle = FCoreDelegates::OnAsyncLoadingFlush.AddRaw(this, &FCtcCookObserver::HandleAsyncLoadingFlush);
			AssetCompileScopeHandle = FAssetCompilingManager::Get().OnPackageScopeEvent().AddRaw(this, &FCtcCookObserver::HandleAssetCompilePackageScope);
			AssetPostCompileHandle = FAssetCompilingManager::Get().OnAssetPostCompileEvent().AddRaw(this, &FCtcCookObserver::HandleAssetPostCompile);
			ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FCtcCookObserver::HandleModulesChanged);
			ProcessLoadedObjectsHandle = FModuleManager::Get().OnProcessLoadedObjectsCallback().AddRaw(this, &FCtcCookObserver::HandleProcessLoadedObjects);
			CompiledInObjectsHandle = FCoreUObjectDelegates::CompiledInUObjectsRegisteredDelegate.AddRaw(this, &FCtcCookObserver::HandleCompiledInUObjectsRegistered);
			PreSavePackageHandle = UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePreSavePackage);
			PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePackageSaved);
			EndLoadPackageHandle = FCoreUObjectDelegates::OnEndLoadPackage.AddRaw(this, &FCtcCookObserver::HandleEndLoadPackage);

			RegisterChunkGeneratorFactory();

			UE_LOG(LogCastToCloudMetricsEditor, Display, TEXT("Cook observability hooks registered."));
		}

		void Shutdown()
		{
			if (CookStartedHandle.IsValid())
			{
				UE::Cook::FDelegates::CookStarted.Remove(CookStartedHandle);
				CookStartedHandle.Reset();
			}

			if (CookFinishedHandle.IsValid())
			{
				UE::Cook::FDelegates::CookFinished.Remove(CookFinishedHandle);
				CookFinishedHandle.Reset();
			}

			if (CookUpdateDisplayHandle.IsValid())
			{
				UE::Cook::FDelegates::CookUpdateDisplay.Remove(CookUpdateDisplayHandle);
				CookUpdateDisplayHandle.Reset();
			}

			if (CookSaveIdleHandle.IsValid())
			{
				UE::Cook::FDelegates::CookSaveIdle.Remove(CookSaveIdleHandle);
				CookSaveIdleHandle.Reset();
			}

			if (CookLoadIdleHandle.IsValid())
			{
				UE::Cook::FDelegates::CookLoadIdle.Remove(CookLoadIdleHandle);
				CookLoadIdleHandle.Reset();
			}

			if (PackageBlockedHandle.IsValid())
			{
				UE::Cook::FDelegates::PackageBlocked.Remove(PackageBlockedHandle);
				PackageBlockedHandle.Reset();
			}

			if (SyncLoadPackageHandle.IsValid())
			{
				FCoreDelegates::OnSyncLoadPackage.Remove(SyncLoadPackageHandle);
				SyncLoadPackageHandle.Reset();
			}

			if (AsyncLoadPackageHandle.IsValid())
			{
				FCoreDelegates::GetOnAsyncLoadPackage().Remove(AsyncLoadPackageHandle);
				AsyncLoadPackageHandle.Reset();
			}

			if (AsyncLoadingFlushHandle.IsValid())
			{
				FCoreDelegates::OnAsyncLoadingFlush.Remove(AsyncLoadingFlushHandle);
				AsyncLoadingFlushHandle.Reset();
			}

			if (AssetCompileScopeHandle.IsValid())
			{
				FAssetCompilingManager::Get().OnPackageScopeEvent().Remove(AssetCompileScopeHandle);
				AssetCompileScopeHandle.Reset();
			}

			if (AssetPostCompileHandle.IsValid())
			{
				FAssetCompilingManager::Get().OnAssetPostCompileEvent().Remove(AssetPostCompileHandle);
				AssetPostCompileHandle.Reset();
			}

			if (ModulesChangedHandle.IsValid())
			{
				FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
				ModulesChangedHandle.Reset();
			}

			if (ProcessLoadedObjectsHandle.IsValid())
			{
				FModuleManager::Get().OnProcessLoadedObjectsCallback().Remove(ProcessLoadedObjectsHandle);
				ProcessLoadedObjectsHandle.Reset();
			}

			if (CompiledInObjectsHandle.IsValid())
			{
				FCoreUObjectDelegates::CompiledInUObjectsRegisteredDelegate.Remove(CompiledInObjectsHandle);
				CompiledInObjectsHandle.Reset();
			}

			if (PreSavePackageHandle.IsValid())
			{
				UPackage::PreSavePackageWithContextEvent.Remove(PreSavePackageHandle);
				PreSavePackageHandle.Reset();
			}

			if (PackageSavedHandle.IsValid())
			{
				UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
				PackageSavedHandle.Reset();
			}

			if (EndLoadPackageHandle.IsValid())
			{
				FCoreUObjectDelegates::OnEndLoadPackage.Remove(EndLoadPackageHandle);
				EndLoadPackageHandle.Reset();
			}

			if (ActiveCookInfo != nullptr && MpCollector.IsValid())
			{
				ActiveCookInfo->UnregisterCollector(MpCollector.GetReference());
			}

			MpCollector.SafeRelease();
			FlushActivePackageSpans(TEXT("ModuleShutdown"));
			FlushActivePackageLoadSpans(TEXT("ModuleShutdown"));
			FlushActivePackageCompileSpans(TEXT("ModuleShutdown"));
			FlushActiveModuleLoadSpans(TEXT("ModuleShutdown"));
			ResetTraceSession();

			ActiveCookInfo = nullptr;
			bCookActive = false;
		}

		virtual void Tick(float DeltaTime) override
		{
		}

		virtual void TickCook(float DeltaTime, bool bCookComplete) override
		{
			if (!bCookActive)
			{
				return;
			}

			HeartbeatAccumulatorSeconds += DeltaTime;

			if (HeartbeatAccumulatorSeconds < CookHeartbeatIntervalSeconds && !(bCookComplete && !bLoggedCookCompleteTick))
			{
				return;
			}

			CaptureAssetCompilingCounters();

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookHeartbeat] cookComplete=%s trackedPackageSaves=%d"),
				BoolToString(bCookComplete),
				ActivePackageSaveSpans.Num()
			);

			HeartbeatAccumulatorSeconds = 0.0;
			bLoggedCookCompleteTick |= bCookComplete;
		}

		virtual bool IsTickable() const override
		{
			return bCookActive;
		}

		virtual TStatId GetStatId() const override
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FCtcCookObserver, STATGROUP_Tickables);
		}

	private:
		int64 GetRelativeTraceTimestampUs(const double EventTimeSeconds) const
		{
			const double TraceSessionReferenceSeconds = SessionStartSeconds > 0.0 ? SessionStartSeconds : EventTimeSeconds;
			return SecondsToTraceMicroseconds(EventTimeSeconds - TraceSessionReferenceSeconds);
		}

		void CaptureAssetCompilingCounters()
		{
			if (!bCookActive)
			{
				return;
			}

			const int64 TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			const int32 ThreadId = GetOrAddTraceThreadId(TEXT("CookCompile"));
			TSet<FString> ObservedManagerNames;

			for (IAssetCompilingManager* Manager : FAssetCompilingManager::Get().GetRegisteredManagers())
			{
				if (Manager == nullptr)
				{
					continue;
				}

				FString ManagerName = Manager->GetAssetTypeName().ToString();
				if (ManagerName.IsEmpty())
				{
					ManagerName = TEXT("Unknown");
				}

				ObservedManagerNames.Add(ManagerName);

				const int32 RemainingAssets = Manager->GetNumRemainingAssets();
				const int32* ExistingCount = LastAssetCompileCounts.Find(ManagerName);
				if (ExistingCount != nullptr && *ExistingCount == RemainingAssets)
				{
					continue;
				}

				LastAssetCompileCounts.Add(ManagerName, RemainingAssets);

				FCtcCounterTraceEvent& CounterEvent = AssetCompileCounterTraceEvents.AddDefaulted_GetRef();
				CounterEvent.Name = FString::Printf(TEXT("AssetCompileRemaining/%s"), *ManagerName);
				CounterEvent.TimestampUs = TimestampUs;
				CounterEvent.ThreadId = ThreadId;
				CounterEvent.Value = RemainingAssets;
			}

			for (auto It = LastAssetCompileCounts.CreateIterator(); It; ++It)
			{
				if (ObservedManagerNames.Contains(It.Key()))
				{
					continue;
				}

				FCtcCounterTraceEvent& CounterEvent = AssetCompileCounterTraceEvents.AddDefaulted_GetRef();
				CounterEvent.Name = FString::Printf(TEXT("AssetCompileRemaining/%s"), *It.Key());
				CounterEvent.TimestampUs = TimestampUs;
				CounterEvent.ThreadId = ThreadId;
				CounterEvent.Value = 0;
				It.RemoveCurrent();
			}
		}

		void BeginPackageLoadSpan(const FString& PackageName, const bool bSynchronousRequest)
		{
			if (!bCookActive || PackageName.IsEmpty())
			{
				return;
			}

			FCtcActivePackageLoadTraceSpan& ActiveSpan = ActivePackageLoadSpans.FindOrAdd(PackageName).AddDefaulted_GetRef();
			ActiveSpan.PackageName = PackageName;
			ActiveSpan.bSynchronousRequest = bSynchronousRequest;
			ActiveSpan.StartSeconds = FPlatformTime::Seconds();

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageLoadStart] package=%s type=%s"),
				*PackageName,
				LoadTypeToString(bSynchronousRequest)
			);
		}

		void FlushActivePackageLoadSpans(const TCHAR* Reason)
		{
			int32 ActiveCount = 0;
			for (const TPair<FString, TArray<FCtcActivePackageLoadTraceSpan>>& ActiveLoad : ActivePackageLoadSpans)
			{
				ActiveCount += ActiveLoad.Value.Num();
			}

			if (ActiveCount == 0)
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageLoadUnpairedStartSummary] reason=%s count=%d"),
				Reason,
				ActiveCount
			);

			int32 SampleCount = 0;
			for (const TPair<FString, TArray<FCtcActivePackageLoadTraceSpan>>& ActiveLoad : ActivePackageLoadSpans)
			{
				for (const FCtcActivePackageLoadTraceSpan& ActiveSpan : ActiveLoad.Value)
				{
					if (SampleCount >= 5)
					{
						break;
					}

					UE_LOG(
						LogCastToCloudMetricsEditor,
						Display,
						TEXT("[PackageLoadUnpairedStartSample] reason=%s package=%s type=%s ageMs=%.2f"),
						Reason,
						*ActiveSpan.PackageName,
						LoadTypeToString(ActiveSpan.bSynchronousRequest),
						(FPlatformTime::Seconds() - ActiveSpan.StartSeconds) * 1000.0
					);

					++SampleCount;
				}
			}

			ActivePackageLoadSpans.Empty();
		}

		void FlushActivePackageCompileSpans(const TCHAR* Reason)
		{
			if (ActivePackageCompileSpans.IsEmpty())
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCompileScopeUnpairedSummary] reason=%s count=%d"),
				Reason,
				ActivePackageCompileSpans.Num()
			);

			ActivePackageCompileSpans.Empty();
		}

		void FlushActiveModuleLoadSpans(const TCHAR* Reason)
		{
			int32 ActiveCount = 0;
			for (const TPair<FString, TArray<FCtcActiveModuleTraceSpan>>& ActiveModule : ActiveModuleLoadSpans)
			{
				ActiveCount += ActiveModule.Value.Num();
			}

			if (ActiveCount == 0)
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ModuleLoadUnpairedStartSummary] reason=%s count=%d"),
				Reason,
				ActiveCount
			);

			ActiveModuleLoadSpans.Empty();
		}

		void HandleSyncLoadPackage(const FString& PackageName)
		{
			BeginPackageLoadSpan(PackageName, true);
		}

		void HandleAsyncLoadPackage(FStringView PackageName)
		{
			BeginPackageLoadSpan(FString(PackageName), false);
		}

		void HandleAsyncLoadingFlush()
		{
			if (!bCookActive)
			{
				return;
			}

			FCtcAsyncLoadingFlushTraceEvent& FlushEvent = AsyncLoadingFlushTraceEvents.AddDefaulted_GetRef();
			FlushEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			FlushEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookLoad"));
			FlushEvent.PendingAsyncPackages = GetNumAsyncPackages();

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[AsyncLoadingFlushStart] pendingAsyncPackages=%d"),
				FlushEvent.PendingAsyncPackages
			);
		}

		void HandleAssetCompilePackageScope(UPackage* Package, bool bEntering)
		{
			if (!bCookActive || Package == nullptr)
			{
				return;
			}

			CaptureAssetCompilingCounters();

			const FString PackageName = Package->GetName();
			if (bEntering)
			{
				FCtcActivePackageCompileTraceSpan& ActiveSpan = ActivePackageCompileSpans.FindOrAdd(PackageName);
				ActiveSpan.PackageName = PackageName;
				ActiveSpan.StartSeconds = FPlatformTime::Seconds();

				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[PackageCompileScopeStart] package=%s"),
					*PackageName
				);
				return;
			}

			FCtcActivePackageCompileTraceSpan* ActiveSpan = ActivePackageCompileSpans.Find(PackageName);
			if (ActiveSpan == nullptr)
			{
				return;
			}

			const double EndSeconds = FPlatformTime::Seconds();
			FCtcCompletedPackageCompileTraceSpan& CompletedSpan = CompletedPackageCompileTraceSpans.AddDefaulted_GetRef();
			CompletedSpan.PackageName = ActiveSpan->PackageName;
			CompletedSpan.TimestampUs = GetRelativeTraceTimestampUs(ActiveSpan->StartSeconds);
			CompletedSpan.DurationUs = SecondsToTraceMicroseconds(EndSeconds - ActiveSpan->StartSeconds);
			CompletedSpan.ThreadId = GetOrAddTraceThreadId(TEXT("CookCompile"));

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCompileScopeEnd] package=%s durationMs=%.2f"),
				*ActiveSpan->PackageName,
				(EndSeconds - ActiveSpan->StartSeconds) * 1000.0
			);

			ActivePackageCompileSpans.Remove(PackageName);
		}

		void HandleAssetPostCompile(const TArray<FAssetCompileData>& CompiledAssets)
		{
			if (!bCookActive)
			{
				return;
			}

			CaptureAssetCompilingCounters();
			if (CompiledAssets.IsEmpty())
			{
				return;
			}

			TArray<FString> AssetNames;
			AssetNames.Reserve(FMath::Min(CompiledAssets.Num(), 8));

			for (const FAssetCompileData& AssetData : CompiledAssets)
			{
				if (AssetNames.Num() >= 8)
				{
					break;
				}

				if (UObject* Asset = AssetData.Asset.Get())
				{
					AssetNames.Add(Asset->GetPathName());
				}
			}

			FCtcAssetCompileTraceEvent& CompileEvent = AssetCompileTraceEvents.AddDefaulted_GetRef();
			CompileEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			CompileEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookCompile"));
			CompileEvent.AssetCount = CompiledAssets.Num();
			CompileEvent.AssetsSample = AssetNames.Num() > 0 ? FString::Join(AssetNames, TEXT(", ")) : TEXT("None");

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[AssetCompileComplete] count=%d assets=%s"),
				CompileEvent.AssetCount,
				*CompileEvent.AssetsSample
			);
		}

		void HandleProcessLoadedObjects(FName ModuleName, bool bCanProcessNewlyLoadedObjects)
		{
			if (!bCookActive)
			{
				return;
			}

			const FString ModuleNameString = ModuleName.IsNone() ? TEXT("None") : ModuleName.ToString();
			FCtcModuleTraceEvent& ModuleEvent = ModuleTraceEvents.AddDefaulted_GetRef();
			ModuleEvent.Name = TEXT("ProcessLoadedObjects");
			ModuleEvent.ModuleName = ModuleNameString;
			ModuleEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			ModuleEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookModule"));
			ModuleEvent.bCanProcessLoadedObjects = bCanProcessNewlyLoadedObjects;

			if (!ModuleName.IsNone())
			{
				FCtcActiveModuleTraceSpan& ActiveSpan = ActiveModuleLoadSpans.FindOrAdd(ModuleNameString).AddDefaulted_GetRef();
				ActiveSpan.ModuleName = ModuleNameString;
				ActiveSpan.bCanProcessLoadedObjects = bCanProcessNewlyLoadedObjects;
				ActiveSpan.StartSeconds = FPlatformTime::Seconds();
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ProcessLoadedObjects] module=%s canProcessLoadedObjects=%s"),
				*ModuleNameString,
				BoolToString(bCanProcessNewlyLoadedObjects)
			);
		}

		void HandleModulesChanged(FName ModuleName, EModuleChangeReason Reason)
		{
			if (!bCookActive)
			{
				return;
			}

			const FString ModuleNameString = ModuleName.IsNone() ? TEXT("None") : ModuleName.ToString();
			const double NowSeconds = FPlatformTime::Seconds();

			FCtcModuleTraceEvent& ModuleEvent = ModuleTraceEvents.AddDefaulted_GetRef();
			ModuleEvent.Name = TEXT("ModuleChanged");
			ModuleEvent.ModuleName = ModuleNameString;
			ModuleEvent.Detail = ModuleChangeReasonToString(Reason);
			ModuleEvent.TimestampUs = GetRelativeTraceTimestampUs(NowSeconds);
			ModuleEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookModule"));

			if (Reason == EModuleChangeReason::ModuleLoaded)
			{
				if (TArray<FCtcActiveModuleTraceSpan>* ActiveSpans = ActiveModuleLoadSpans.Find(ModuleNameString))
				{
					if (!ActiveSpans->IsEmpty())
					{
						const FCtcActiveModuleTraceSpan ActiveSpan = (*ActiveSpans)[0];
						ActiveSpans->RemoveAt(0);
						if (ActiveSpans->IsEmpty())
						{
							ActiveModuleLoadSpans.Remove(ModuleNameString);
						}

						FCtcCompletedModuleTraceSpan& CompletedSpan = CompletedModuleTraceSpans.AddDefaulted_GetRef();
						CompletedSpan.ModuleName = ActiveSpan.ModuleName;
						CompletedSpan.bCanProcessLoadedObjects = ActiveSpan.bCanProcessLoadedObjects;
						CompletedSpan.TimestampUs = GetRelativeTraceTimestampUs(ActiveSpan.StartSeconds);
						CompletedSpan.DurationUs = SecondsToTraceMicroseconds(NowSeconds - ActiveSpan.StartSeconds);
						CompletedSpan.ThreadId = GetOrAddTraceThreadId(TEXT("CookModule"));

						UE_LOG(
							LogCastToCloudMetricsEditor,
							Display,
							TEXT("[ModuleLoadEnd] module=%s durationMs=%.2f"),
							*CompletedSpan.ModuleName,
							(NowSeconds - ActiveSpan.StartSeconds) * 1000.0
						);
					}
				}
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ModuleChanged] module=%s reason=%s"),
				*ModuleNameString,
				ModuleChangeReasonToString(Reason)
			);
		}

		void HandleCompiledInUObjectsRegistered(FName ModuleName, ECompiledInUObjectsRegisteredStatus Status)
		{
			if (!bCookActive)
			{
				return;
			}

			const FString ModuleNameString = ModuleName.IsNone() ? TEXT("None") : ModuleName.ToString();
			FCtcModuleTraceEvent& ModuleEvent = ModuleTraceEvents.AddDefaulted_GetRef();
			ModuleEvent.Name = TEXT("CompiledInUObjectsRegistered");
			ModuleEvent.ModuleName = ModuleNameString;
			ModuleEvent.Detail = CompiledInStatusToString(Status);
			ModuleEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			ModuleEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookModule"));

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CompiledInUObjectsRegistered] module=%s status=%s"),
				*ModuleNameString,
				CompiledInStatusToString(Status)
			);
		}

		void HandleCookStarted(UE::Cook::ICookInfo& CookInfo)
		{
			if (ActiveCookInfo != nullptr && ActiveCookInfo != &CookInfo && MpCollector.IsValid())
			{
				ActiveCookInfo->UnregisterCollector(MpCollector.GetReference());
			}

			ActiveCookInfo = &CookInfo;
			bCookActive = true;
			bLoggedCookCompleteTick = false;
			HeartbeatAccumulatorSeconds = 0.0;
			LastCookedPackagesCount = INDEX_NONE;
			LastCookPendingCount = INDEX_NONE;
			ResetTraceSession();
			SessionStartSeconds = FPlatformTime::Seconds();
			CurrentCookSessionSummary = DescribeCookSession(CookInfo);
			GetOrAddTraceThreadId(TEXT("CookMain"));
			GetOrAddTraceThreadId(TEXT("CookLoad"));
			GetOrAddTraceThreadId(TEXT("CookLoadSync"));
			GetOrAddTraceThreadId(TEXT("CookLoadAsync"));
			GetOrAddTraceThreadId(TEXT("CookCompile"));
			GetOrAddTraceThreadId(TEXT("CookModule"));
			GetOrAddTraceThreadId(TEXT("CookWait"));
			CaptureAssetCompilingCounters();

			if (!MpCollector.IsValid())
			{
				MpCollector = new FCtcCookMpCollector();
			}

			CookInfo.RegisterCollector(MpCollector.GetReference(), UE::Cook::EProcessType::AllMPCook);

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookSessionStart] %s"),
				*CurrentCookSessionSummary
			);
		}

		void HandleCookFinished(UE::Cook::ICookInfo& CookInfo)
		{
			SessionEndSeconds = FPlatformTime::Seconds();
			CaptureAssetCompilingCounters();
			const int32 UnpairedPackageSaveCount = ActivePackageSaveSpans.Num();
			FlushActivePackageSpans(TEXT("CookFinished"));
			FlushActivePackageLoadSpans(TEXT("CookFinished"));
			FlushActivePackageCompileSpans(TEXT("CookFinished"));
			FlushActiveModuleLoadSpans(TEXT("CookFinished"));

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookSessionEnd] %s trackedPackageSaves=%d"),
				*DescribeCookSession(CookInfo),
				UnpairedPackageSaveCount
			);

			if (MpCollector.IsValid())
			{
				CookInfo.UnregisterCollector(MpCollector.GetReference());
			}

			ActiveCookInfo = nullptr;
			bCookActive = false;
			bLoggedCookCompleteTick = false;
			HeartbeatAccumulatorSeconds = 0.0;
			LastCookedPackagesCount = INDEX_NONE;
			LastCookPendingCount = INDEX_NONE;

			WriteTraceFile();
			ResetTraceSession();
		}

		void HandleCookUpdateDisplay(UE::Cook::ICookInfo& CookInfo, int32 CookedPackagesCount, int32 CookPendingCount)
		{
			if (CookedPackagesCount == LastCookedPackagesCount && CookPendingCount == LastCookPendingCount)
			{
				return;
			}

			LastCookedPackagesCount = CookedPackagesCount;
			LastCookPendingCount = CookPendingCount;
			CaptureAssetCompilingCounters();

			FCtcProgressTraceEvent& ProgressEvent = ProgressTraceEvents.AddDefaulted_GetRef();
			ProgressEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			ProgressEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookMain"));
			ProgressEvent.CookedPackagesCount = CookedPackagesCount;
			ProgressEvent.CookPendingCount = CookPendingCount;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookProgress] cookType=%s cooked=%d pending=%d"),
				CookTypeToString(CookInfo.GetCookType()),
				CookedPackagesCount,
				CookPendingCount
			);
		}

		void HandleCookSaveIdle(
			UE::Cook::ICookInfo& CookInfo,
			int32 NumPackagesInSaveQueue,
			int32 NumPendingCookedPlatformData,
			bool bExpectedDueToSlowBuildOperations
		)
		{
			CaptureAssetCompilingCounters();

			FCtcIdleTraceEvent& IdleEvent = IdleTraceEvents.AddDefaulted_GetRef();
			IdleEvent.Name = TEXT("CookSaveIdle");
			IdleEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			IdleEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookWait"));
			IdleEvent.QueueCount = NumPackagesInSaveQueue;
			IdleEvent.PendingCookedPlatformData = NumPendingCookedPlatformData;
			IdleEvent.bExpectedDueToSlowBuildOperations = bExpectedDueToSlowBuildOperations;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Warning,
				TEXT("[CookSaveIdle] cookType=%s saveQueue=%d pendingPlatformData=%d expectedSlowBuild=%s"),
				CookTypeToString(CookInfo.GetCookType()),
				NumPackagesInSaveQueue,
				NumPendingCookedPlatformData,
				BoolToString(bExpectedDueToSlowBuildOperations)
			);
		}

		void HandleCookLoadIdle(UE::Cook::ICookInfo& CookInfo, int32 NumPackagesInLoadQueue)
		{
			CaptureAssetCompilingCounters();

			FCtcIdleTraceEvent& IdleEvent = IdleTraceEvents.AddDefaulted_GetRef();
			IdleEvent.Name = TEXT("CookLoadIdle");
			IdleEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			IdleEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookWait"));
			IdleEvent.QueueCount = NumPackagesInLoadQueue;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Warning,
				TEXT("[CookLoadIdle] cookType=%s loadQueue=%d"),
				CookTypeToString(CookInfo.GetCookType()),
				NumPackagesInLoadQueue
			);
		}

		void HandlePackageBlocked(const UObject* Object, FStringBuilderBase& OutDebugInfo)
		{
			if (!bCookActive || Object == nullptr)
			{
				return;
			}

			CaptureAssetCompilingCounters();

			FCtcBlockedObjectTraceEvent& BlockedEvent = BlockedObjectTraceEvents.AddDefaulted_GetRef();
			BlockedEvent.ObjectName = Object->GetFullName();
			BlockedEvent.ObjectClass = Object->GetClass() != nullptr ? Object->GetClass()->GetName() : TEXT("Unknown");
			BlockedEvent.PackageName = Object->GetPackage() != nullptr ? Object->GetPackage()->GetName() : TEXT("None");
			BlockedEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			BlockedEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookWait"));

			OutDebugInfo.Append(TEXT("CastToCloudMetrics captured this blocked object for external trace export.\n"));
		}

		void HandleEndLoadPackage(const FEndLoadPackageContext& Context)
		{
			if (!bCookActive || Context.LoadedPackages.Num() == 0)
			{
				return;
			}

			const double EndSeconds = FPlatformTime::Seconds();
			TArray<FString> PackageNames;
			PackageNames.Reserve(FMath::Min(Context.LoadedPackages.Num(), 8));

			int32 Index = 0;
			for (UPackage* LoadedPackage : Context.LoadedPackages)
			{
				if (LoadedPackage != nullptr)
				{
					const FString PackageName = LoadedPackage->GetName();
					if (Index < 8)
					{
						PackageNames.Add(PackageName);
					}

					if (TArray<FCtcActivePackageLoadTraceSpan>* ActiveSpans = ActivePackageLoadSpans.Find(PackageName))
					{
						if (!ActiveSpans->IsEmpty())
						{
							const FCtcActivePackageLoadTraceSpan ActiveSpan = (*ActiveSpans)[0];
							ActiveSpans->RemoveAt(0);
							if (ActiveSpans->IsEmpty())
							{
								ActivePackageLoadSpans.Remove(PackageName);
							}

							FCtcCompletedPackageLoadTraceSpan& CompletedSpan = CompletedPackageLoadTraceSpans.AddDefaulted_GetRef();
							CompletedSpan.PackageName = ActiveSpan.PackageName;
							CompletedSpan.bSynchronousRequest = ActiveSpan.bSynchronousRequest;
							CompletedSpan.bSynchronousCompletion = Context.bSynchronous;
							CompletedSpan.TimestampUs = GetRelativeTraceTimestampUs(ActiveSpan.StartSeconds);
							CompletedSpan.DurationUs = SecondsToTraceMicroseconds(EndSeconds - ActiveSpan.StartSeconds);
							CompletedSpan.ThreadId = GetOrAddTraceThreadId(
								ActiveSpan.bSynchronousRequest ? TEXT("CookLoadSync") : TEXT("CookLoadAsync")
							);
							CompletedSpan.RecursiveDepth = Context.RecursiveDepth;

							UE_LOG(
								LogCastToCloudMetricsEditor,
								Display,
								TEXT("[PackageLoadEnd] package=%s type=%s durationMs=%.2f endSynchronous=%s recursiveDepth=%d"),
								*CompletedSpan.PackageName,
								LoadTypeToString(CompletedSpan.bSynchronousRequest),
								(EndSeconds - ActiveSpan.StartSeconds) * 1000.0,
								BoolToString(CompletedSpan.bSynchronousCompletion),
								CompletedSpan.RecursiveDepth
							);
						}
					}
				}
				++Index;
			}

			FCtcLoadBatchTraceEvent& LoadEvent = LoadBatchTraceEvents.AddDefaulted_GetRef();
			LoadEvent.TimestampUs = GetRelativeTraceTimestampUs(EndSeconds);
			LoadEvent.ThreadId = GetOrAddTraceThreadId(Context.bSynchronous ? TEXT("CookLoadSync") : TEXT("CookLoadAsync"));
			LoadEvent.PackageCount = Context.LoadedPackages.Num();
			LoadEvent.RecursiveDepth = Context.RecursiveDepth;
			LoadEvent.bSynchronous = Context.bSynchronous;
			LoadEvent.PackagesSample = PackageNames.Num() > 0 ? FString::Join(PackageNames, TEXT(", ")) : TEXT("None");
		}

		void HandlePreSavePackage(UPackage* Package, FObjectPreSaveContext SaveContext)
		{
			if (Package == nullptr || !SaveContext.IsCooking())
			{
				return;
			}

			const FString PackageName = Package->GetName();
			const FString PackageKey = BuildCookPackageKey(PackageName, SaveContext.GetTargetFilename());
			const double NowSeconds = FPlatformTime::Seconds();

			if (ActivePackageSaveSpans.Contains(PackageKey))
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[PackageCookStartDuplicate] package=%s platform=%s"),
					*PackageName,
					*GetTargetPlatformName(SaveContext.GetTargetPlatform())
				);
			}

			FString Instigator = TEXT("Unavailable");
			FString InstigatorChain = TEXT("Unavailable");

			if (ActiveCookInfo != nullptr)
			{
				const TArray<UE::Cook::FInstigator> CookInstigatorChain = ActiveCookInfo->GetInstigatorChain(Package->GetFName());

				if (CookInstigatorChain.Num() > 0)
				{
					Instigator = CookInstigatorChain[0].ToString();
					InstigatorChain = JoinInstigatorChain(CookInstigatorChain);
				}
				else
				{
					Instigator = ActiveCookInfo->GetInstigator(Package->GetFName()).ToString();
					InstigatorChain = Instigator;
				}
			}

			FCtcActivePackageTraceSpan& ActiveSpan = ActivePackageSaveSpans.FindOrAdd(PackageKey);
			ActiveSpan.PackageName = PackageName;
			ActiveSpan.PlatformName = GetTargetPlatformName(SaveContext.GetTargetPlatform());
			ActiveSpan.TargetFilename = SaveContext.GetTargetFilename();
			ActiveSpan.Instigator = Instigator;
			ActiveSpan.InstigatorChain = InstigatorChain;
			ActiveSpan.CookType = CookTypeToString(SaveContext.GetCookType());
			ActiveSpan.bProceduralSave = SaveContext.IsProceduralSave();
			ActiveSpan.StartSeconds = NowSeconds;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookStart] package=%s platform=%s cookType=%s procedural=%s target=%s instigator=%s instigatorChain=%s"),
				*PackageName,
				*GetTargetPlatformName(SaveContext.GetTargetPlatform()),
				CookTypeToString(SaveContext.GetCookType()),
				BoolToString(SaveContext.IsProceduralSave()),
				SaveContext.GetTargetFilename(),
				*Instigator,
				*InstigatorChain
			);
		}

		void HandlePackageSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext SaveContext)
		{
			if (Package == nullptr || !SaveContext.IsCooking())
			{
				return;
			}

			const FString PackageName = Package->GetName();
			const FString PackageKey = BuildCookPackageKey(PackageName, *Filename);

			double DurationMilliseconds = -1.0;
			if (FCtcActivePackageTraceSpan* ActiveSpan = ActivePackageSaveSpans.Find(PackageKey))
			{
				const double EndSeconds = FPlatformTime::Seconds();
				const double TraceSessionReferenceSeconds = SessionStartSeconds > 0.0 ? SessionStartSeconds : ActiveSpan->StartSeconds;
				DurationMilliseconds = (EndSeconds - ActiveSpan->StartSeconds) * 1000.0;

				FCtcCompletedPackageTraceSpan& CompletedSpan = CompletedPackageTraceSpans.AddDefaulted_GetRef();
				CompletedSpan.PackageName = ActiveSpan->PackageName;
				CompletedSpan.PlatformName = ActiveSpan->PlatformName;
				CompletedSpan.TargetFilename = ActiveSpan->TargetFilename;
				CompletedSpan.Instigator = ActiveSpan->Instigator;
				CompletedSpan.InstigatorChain = ActiveSpan->InstigatorChain;
				CompletedSpan.CookType = ActiveSpan->CookType;
				CompletedSpan.bProceduralSave = ActiveSpan->bProceduralSave;
				CompletedSpan.TimestampUs = SecondsToTraceMicroseconds(ActiveSpan->StartSeconds - TraceSessionReferenceSeconds);
				CompletedSpan.DurationUs = SecondsToTraceMicroseconds(EndSeconds - ActiveSpan->StartSeconds);
				CompletedSpan.ThreadId = GetOrAddTraceThreadId(ActiveSpan->PlatformName);

				ActivePackageSaveSpans.Remove(PackageKey);
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookEnd] package=%s platform=%s cookType=%s procedural=%s durationMs=%.2f file=%s"),
				*PackageName,
				*GetTargetPlatformName(SaveContext.GetTargetPlatform()),
				CookTypeToString(SaveContext.GetCookType()),
				BoolToString(SaveContext.IsProceduralSave()),
				DurationMilliseconds,
				*Filename
			);
		}

		void FlushActivePackageSpans(const TCHAR* Reason)
		{
			if (ActivePackageSaveSpans.IsEmpty())
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookUnpairedStartSummary] reason=%s count=%d"),
				Reason,
				ActivePackageSaveSpans.Num()
			);

			int32 SampleCount = 0;
			for (const TPair<FString, FCtcActivePackageTraceSpan>& ActiveSave : ActivePackageSaveSpans)
			{
				if (SampleCount >= 5)
				{
					break;
				}

				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[PackageCookUnpairedStartSample] reason=%s packageKey=%s ageMs=%.2f"),
					Reason,
					*ActiveSave.Key,
					(FPlatformTime::Seconds() - ActiveSave.Value.StartSeconds) * 1000.0
				);

				++SampleCount;
			}

			ActivePackageSaveSpans.Empty();
		}

		int32 GetOrAddTraceThreadId(const FString& PlatformName)
		{
			if (const int32* ExistingThreadId = TraceThreadIds.Find(PlatformName))
			{
				return *ExistingThreadId;
			}

			const int32 NewThreadId = NextTraceThreadId++;
			TraceThreadIds.Add(PlatformName, NewThreadId);
			return NewThreadId;
		}

		void WriteTraceFile()
		{
			const bool bHasAnyTraceEvents =
				!CompletedPackageTraceSpans.IsEmpty() ||
				!CompletedPackageLoadTraceSpans.IsEmpty() ||
				!CompletedPackageCompileTraceSpans.IsEmpty() ||
				!CompletedModuleTraceSpans.IsEmpty() ||
				!ModuleTraceEvents.IsEmpty() ||
				!AssetCompileTraceEvents.IsEmpty() ||
				!AssetCompileCounterTraceEvents.IsEmpty() ||
				!AsyncLoadingFlushTraceEvents.IsEmpty() ||
				!LoadBatchTraceEvents.IsEmpty() ||
				!IdleTraceEvents.IsEmpty() ||
				!BlockedObjectTraceEvents.IsEmpty() ||
				!ProgressTraceEvents.IsEmpty();

			if (!bHasAnyTraceEvents)
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[CookTraceWriteSkipped] reason=NoObservedTraceEvents")
				);
				return;
			}

			CompletedPackageTraceSpans.Sort(
				[](const FCtcCompletedPackageTraceSpan& Left, const FCtcCompletedPackageTraceSpan& Right)
				{
					if (Left.TimestampUs != Right.TimestampUs)
					{
						return Left.TimestampUs < Right.TimestampUs;
					}

					if (Left.ThreadId != Right.ThreadId)
					{
						return Left.ThreadId < Right.ThreadId;
					}

					return Left.PackageName < Right.PackageName;
				}
			);
			CompletedPackageLoadTraceSpans.Sort(
				[](const FCtcCompletedPackageLoadTraceSpan& Left, const FCtcCompletedPackageLoadTraceSpan& Right)
				{
					if (Left.TimestampUs != Right.TimestampUs)
					{
						return Left.TimestampUs < Right.TimestampUs;
					}

					if (Left.ThreadId != Right.ThreadId)
					{
						return Left.ThreadId < Right.ThreadId;
					}

					return Left.PackageName < Right.PackageName;
				}
			);
			CompletedPackageCompileTraceSpans.Sort(
				[](const FCtcCompletedPackageCompileTraceSpan& Left, const FCtcCompletedPackageCompileTraceSpan& Right)
				{
					if (Left.TimestampUs != Right.TimestampUs)
					{
						return Left.TimestampUs < Right.TimestampUs;
					}

					return Left.PackageName < Right.PackageName;
				}
			);
			CompletedModuleTraceSpans.Sort(
				[](const FCtcCompletedModuleTraceSpan& Left, const FCtcCompletedModuleTraceSpan& Right)
				{
					if (Left.TimestampUs != Right.TimestampUs)
					{
						return Left.TimestampUs < Right.TimestampUs;
					}

					return Left.ModuleName < Right.ModuleName;
				}
			);
			ModuleTraceEvents.Sort([](const FCtcModuleTraceEvent& Left, const FCtcModuleTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			AssetCompileTraceEvents.Sort([](const FCtcAssetCompileTraceEvent& Left, const FCtcAssetCompileTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			AssetCompileCounterTraceEvents.Sort([](const FCtcCounterTraceEvent& Left, const FCtcCounterTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			AsyncLoadingFlushTraceEvents.Sort([](const FCtcAsyncLoadingFlushTraceEvent& Left, const FCtcAsyncLoadingFlushTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			LoadBatchTraceEvents.Sort([](const FCtcLoadBatchTraceEvent& Left, const FCtcLoadBatchTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			IdleTraceEvents.Sort([](const FCtcIdleTraceEvent& Left, const FCtcIdleTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			BlockedObjectTraceEvents.Sort([](const FCtcBlockedObjectTraceEvent& Left, const FCtcBlockedObjectTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			ProgressTraceEvents.Sort([](const FCtcProgressTraceEvent& Left, const FCtcProgressTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });

			const FString TraceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CastToCloud"), TEXT("CookTraces"));
			IFileManager::Get().MakeDirectory(*TraceDirectory, true);

			const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
			const FString TraceFilename = FString::Printf(TEXT("CookTrace-%s-%03d.json"), *Timestamp, ++TraceFileSequence);
			const FString TracePath = FPaths::Combine(TraceDirectory, TraceFilename);

			FString TraceJson;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&TraceJson);

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("displayTimeUnit"), TEXT("ms"));
			Writer->WriteArrayStart(TEXT("traceEvents"));

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), TEXT("process_name"));
			Writer->WriteValue(TEXT("ph"), TEXT("M"));
			Writer->WriteValue(TEXT("pid"), TraceProcessId);
			Writer->WriteValue(TEXT("tid"), 0);
			Writer->WriteObjectStart(TEXT("args"));
			Writer->WriteValue(TEXT("name"), TEXT("CastToCloud Cook"));
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();

			for (const TPair<FString, int32>& ThreadName : TraceThreadIds)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("thread_name"));
				Writer->WriteValue(TEXT("ph"), TEXT("M"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), ThreadName.Value);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("name"), ThreadName.Key);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			if (SessionEndSeconds > SessionStartSeconds)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("CookSession"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.session"));
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), GetOrAddTraceThreadId(TEXT("CookMain")));
				Writer->WriteValue(TEXT("ts"), 0);
				Writer->WriteValue(TEXT("dur"), SecondsToTraceMicroseconds(SessionEndSeconds - SessionStartSeconds));
				Writer->WriteObjectStart(TEXT("args"));
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcCompletedPackageLoadTraceSpan& CompletedSpan : CompletedPackageLoadTraceSpans)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("PackageLoad"));
				Writer->WriteValue(TEXT("cat"), CompletedSpan.bSynchronousRequest ? TEXT("cook.load.sync") : TEXT("cook.load.async"));
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CompletedSpan.ThreadId);
				Writer->WriteValue(TEXT("ts"), CompletedSpan.TimestampUs);
				Writer->WriteValue(TEXT("dur"), CompletedSpan.DurationUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("package"), CompletedSpan.PackageName);
				Writer->WriteValue(TEXT("requestedType"), LoadTypeToString(CompletedSpan.bSynchronousRequest));
				Writer->WriteValue(TEXT("endSynchronous"), CompletedSpan.bSynchronousCompletion);
				Writer->WriteValue(TEXT("recursiveDepth"), CompletedSpan.RecursiveDepth);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcCompletedPackageCompileTraceSpan& CompletedSpan : CompletedPackageCompileTraceSpans)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("PackageCompileScope"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.compile.package"));
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CompletedSpan.ThreadId);
				Writer->WriteValue(TEXT("ts"), CompletedSpan.TimestampUs);
				Writer->WriteValue(TEXT("dur"), CompletedSpan.DurationUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("package"), CompletedSpan.PackageName);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcCompletedModuleTraceSpan& CompletedSpan : CompletedModuleTraceSpans)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("ModuleLoad"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.module"));
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CompletedSpan.ThreadId);
				Writer->WriteValue(TEXT("ts"), CompletedSpan.TimestampUs);
				Writer->WriteValue(TEXT("dur"), CompletedSpan.DurationUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("module"), CompletedSpan.ModuleName);
				Writer->WriteValue(TEXT("canProcessLoadedObjects"), CompletedSpan.bCanProcessLoadedObjects);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcCompletedPackageTraceSpan& CompletedSpan : CompletedPackageTraceSpans)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("PackageCook"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook"));
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CompletedSpan.ThreadId);
				Writer->WriteValue(TEXT("ts"), CompletedSpan.TimestampUs);
				Writer->WriteValue(TEXT("dur"), CompletedSpan.DurationUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("package"), CompletedSpan.PackageName);
				Writer->WriteValue(TEXT("platform"), CompletedSpan.PlatformName);
				Writer->WriteValue(TEXT("target"), CompletedSpan.TargetFilename);
				Writer->WriteValue(TEXT("cookType"), CompletedSpan.CookType);
				Writer->WriteValue(TEXT("procedural"), CompletedSpan.bProceduralSave);
				Writer->WriteValue(TEXT("instigator"), CompletedSpan.Instigator);
				Writer->WriteValue(TEXT("instigatorChain"), CompletedSpan.InstigatorChain);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcAsyncLoadingFlushTraceEvent& FlushEvent : AsyncLoadingFlushTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("AsyncLoadingFlushStart"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.load.flush"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), FlushEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), FlushEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("pendingAsyncPackages"), FlushEvent.PendingAsyncPackages);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcModuleTraceEvent& ModuleEvent : ModuleTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), ModuleEvent.Name);
				Writer->WriteValue(TEXT("cat"), TEXT("cook.module"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), ModuleEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), ModuleEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("module"), ModuleEvent.ModuleName);
				if (!ModuleEvent.Detail.IsEmpty())
				{
					Writer->WriteValue(TEXT("detail"), ModuleEvent.Detail);
				}
				Writer->WriteValue(TEXT("canProcessLoadedObjects"), ModuleEvent.bCanProcessLoadedObjects);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcAssetCompileTraceEvent& CompileEvent : AssetCompileTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("AssetCompileComplete"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.compile"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CompileEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), CompileEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("assetCount"), CompileEvent.AssetCount);
				Writer->WriteValue(TEXT("assets"), CompileEvent.AssetsSample);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcLoadBatchTraceEvent& LoadEvent : LoadBatchTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("PackageLoadBatchEnd"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.load"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), LoadEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), LoadEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("packageCount"), LoadEvent.PackageCount);
				Writer->WriteValue(TEXT("recursiveDepth"), LoadEvent.RecursiveDepth);
				Writer->WriteValue(TEXT("synchronous"), LoadEvent.bSynchronous);
				Writer->WriteValue(TEXT("packages"), LoadEvent.PackagesSample);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcIdleTraceEvent& IdleEvent : IdleTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), IdleEvent.Name);
				Writer->WriteValue(TEXT("cat"), TEXT("cook.wait"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), IdleEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), IdleEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("queueCount"), IdleEvent.QueueCount);
				Writer->WriteValue(TEXT("pendingCookedPlatformData"), IdleEvent.PendingCookedPlatformData);
				Writer->WriteValue(TEXT("expectedDueToSlowBuildOperations"), IdleEvent.bExpectedDueToSlowBuildOperations);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcBlockedObjectTraceEvent& BlockedEvent : BlockedObjectTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("CookBlockedObject"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.wait"));
				Writer->WriteValue(TEXT("ph"), TEXT("i"));
				Writer->WriteValue(TEXT("s"), TEXT("g"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), BlockedEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), BlockedEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("object"), BlockedEvent.ObjectName);
				Writer->WriteValue(TEXT("class"), BlockedEvent.ObjectClass);
				Writer->WriteValue(TEXT("package"), BlockedEvent.PackageName);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcProgressTraceEvent& ProgressEvent : ProgressTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), TEXT("CookProgress"));
				Writer->WriteValue(TEXT("cat"), TEXT("cook.progress"));
				Writer->WriteValue(TEXT("ph"), TEXT("C"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), ProgressEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), ProgressEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("CookedPackages"), ProgressEvent.CookedPackagesCount);
				Writer->WriteValue(TEXT("PendingPackages"), ProgressEvent.CookPendingCount);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			for (const FCtcCounterTraceEvent& CounterEvent : AssetCompileCounterTraceEvents)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), CounterEvent.Name);
				Writer->WriteValue(TEXT("cat"), TEXT("cook.compile.remaining"));
				Writer->WriteValue(TEXT("ph"), TEXT("C"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), CounterEvent.ThreadId);
				Writer->WriteValue(TEXT("ts"), CounterEvent.TimestampUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("Remaining"), CounterEvent.Value);
				Writer->WriteObjectEnd();
				Writer->WriteObjectEnd();
			}

			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
			Writer->Close();

			if (!FFileHelper::SaveStringToFile(
				TraceJson,
				*TracePath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
			))
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[CookTraceWriteFailed] path=%s packageSpans=%d loadSpans=%d"),
					*TracePath,
					CompletedPackageTraceSpans.Num(),
					CompletedPackageLoadTraceSpans.Num()
				);
				return;
			}

			LastTraceOutputPath = TracePath;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookTraceWrite] path=%s packageSpans=%d loadSpans=%d compileSpans=%d moduleSpans=%d"),
				*TracePath,
				CompletedPackageTraceSpans.Num(),
				CompletedPackageLoadTraceSpans.Num(),
				CompletedPackageCompileTraceSpans.Num(),
				CompletedModuleTraceSpans.Num()
			);
		}

		void ResetTraceSession()
		{
			ActivePackageSaveSpans.Empty();
			CompletedPackageTraceSpans.Empty();
			ActivePackageLoadSpans.Empty();
			CompletedPackageLoadTraceSpans.Empty();
			ActivePackageCompileSpans.Empty();
			CompletedPackageCompileTraceSpans.Empty();
			ActiveModuleLoadSpans.Empty();
			CompletedModuleTraceSpans.Empty();
			ModuleTraceEvents.Empty();
			AssetCompileTraceEvents.Empty();
			AssetCompileCounterTraceEvents.Empty();
			AsyncLoadingFlushTraceEvents.Empty();
			LoadBatchTraceEvents.Empty();
			IdleTraceEvents.Empty();
			BlockedObjectTraceEvents.Empty();
			ProgressTraceEvents.Empty();
			LastAssetCompileCounts.Empty();
			TraceThreadIds.Empty();
			CurrentCookSessionSummary.Empty();
			SessionStartSeconds = 0.0;
			SessionEndSeconds = 0.0;
			NextTraceThreadId = 1;
		}

		void RegisterChunkGeneratorFactory()
		{
			if (GCastToCloudChunkFactoryRegistered)
			{
				return;
			}

			IChunkDataGenerator::AddChunkDataGeneratorFactory(&CreateCastToCloudChunkDataGenerator);
			GCastToCloudChunkFactoryRegistered = true;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("Registered chunk data generator factory. Unreal does not expose an unregister API for this hook, so it remains registered for the lifetime of the process.")
			);
		}

	private:
		FDelegateHandle CookStartedHandle;
		FDelegateHandle CookFinishedHandle;
		FDelegateHandle CookUpdateDisplayHandle;
		FDelegateHandle CookSaveIdleHandle;
		FDelegateHandle CookLoadIdleHandle;
		FDelegateHandle PackageBlockedHandle;
		FDelegateHandle SyncLoadPackageHandle;
		FDelegateHandle AsyncLoadPackageHandle;
		FDelegateHandle AsyncLoadingFlushHandle;
		FDelegateHandle AssetCompileScopeHandle;
		FDelegateHandle AssetPostCompileHandle;
		FDelegateHandle ModulesChangedHandle;
		FDelegateHandle ProcessLoadedObjectsHandle;
		FDelegateHandle CompiledInObjectsHandle;
		FDelegateHandle PreSavePackageHandle;
		FDelegateHandle PackageSavedHandle;
		FDelegateHandle EndLoadPackageHandle;

		UE::Cook::ICookInfo* ActiveCookInfo = nullptr;
		TRefCountPtr<FCtcCookMpCollector> MpCollector;
		TMap<FString, FCtcActivePackageTraceSpan> ActivePackageSaveSpans;
		TArray<FCtcCompletedPackageTraceSpan> CompletedPackageTraceSpans;
		TMap<FString, TArray<FCtcActivePackageLoadTraceSpan>> ActivePackageLoadSpans;
		TArray<FCtcCompletedPackageLoadTraceSpan> CompletedPackageLoadTraceSpans;
		TMap<FString, FCtcActivePackageCompileTraceSpan> ActivePackageCompileSpans;
		TArray<FCtcCompletedPackageCompileTraceSpan> CompletedPackageCompileTraceSpans;
		TMap<FString, TArray<FCtcActiveModuleTraceSpan>> ActiveModuleLoadSpans;
		TArray<FCtcCompletedModuleTraceSpan> CompletedModuleTraceSpans;
		TArray<FCtcModuleTraceEvent> ModuleTraceEvents;
		TArray<FCtcAssetCompileTraceEvent> AssetCompileTraceEvents;
		TArray<FCtcCounterTraceEvent> AssetCompileCounterTraceEvents;
		TArray<FCtcAsyncLoadingFlushTraceEvent> AsyncLoadingFlushTraceEvents;
		TArray<FCtcLoadBatchTraceEvent> LoadBatchTraceEvents;
		TArray<FCtcIdleTraceEvent> IdleTraceEvents;
		TArray<FCtcBlockedObjectTraceEvent> BlockedObjectTraceEvents;
		TArray<FCtcProgressTraceEvent> ProgressTraceEvents;
		TMap<FString, int32> LastAssetCompileCounts;
		TMap<FString, int32> TraceThreadIds;
		FString CurrentCookSessionSummary;
		FString LastTraceOutputPath;
		double SessionStartSeconds = 0.0;
		double SessionEndSeconds = 0.0;
		double HeartbeatAccumulatorSeconds = 0.0;
		int32 LastCookedPackagesCount = INDEX_NONE;
		int32 LastCookPendingCount = INDEX_NONE;
		int32 NextTraceThreadId = 1;
		int32 TraceFileSequence = 0;
		bool bCookActive = false;
		bool bLoggedCookCompleteTick = false;
	};

	TUniquePtr<FCtcCookObserver> GCookObserver;
} // namespace

void FCtcMetricsEditorModule::StartupModule()
{
	if (!GCookObserver.IsValid())
	{
		GCookObserver = MakeUnique<FCtcCookObserver>();
		GCookObserver->Startup();
	}
}

void FCtcMetricsEditorModule::ShutdownModule()
{
	if (GCookObserver.IsValid())
	{
		GCookObserver->Shutdown();
		GCookObserver.Reset();
	}
}

IMPLEMENT_MODULE(FCtcMetricsEditorModule, CastToCloudMetricsEditor)
