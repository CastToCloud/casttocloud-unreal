// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsEditorModule.h"

#include "Commandlets/IChunkDataGenerator.h"
#include "Cooker/MPCollector.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "TickableEditorObject.h"
#include "Trace/Analyzer.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ModuleService.h"
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

	FString NormalizeTracePath(const FString& TracePath)
	{
		FString AbsolutePath = FPaths::ConvertRelativePathToFull(TracePath);
		FPaths::NormalizeFilename(AbsolutePath);
		return AbsolutePath;
	}

	int64 SecondsToTraceMicroseconds(const double Seconds)
	{
		return static_cast<int64>(FMath::Max(0.0, Seconds) * 1000.0 * 1000.0);
	}

	const TCHAR* PackageEventStatTypeToString(const EPackageEventStatType StatType)
	{
		switch (StatType)
		{
		case EPackageEventStatType::LoadPackage:
			return TEXT("LoadPackage");
		case EPackageEventStatType::SavePackage:
			return TEXT("SavePackage");
		case EPackageEventStatType::BeginCacheForCookedPlatformData:
			return TEXT("BeginCacheForCookedPlatformData");
		case EPackageEventStatType::IsCachedCookedPlatformDataLoaded:
			return TEXT("IsCachedCookedPlatformDataLoaded");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* PackageEventStatCategoryToString(const EPackageEventStatType StatType)
	{
		switch (StatType)
		{
		case EPackageEventStatType::LoadPackage:
			return TEXT("cook.native.load");
		case EPackageEventStatType::SavePackage:
			return TEXT("cook.native.save");
		case EPackageEventStatType::BeginCacheForCookedPlatformData:
			return TEXT("cook.native.cache.begin");
		case EPackageEventStatType::IsCachedCookedPlatformDataLoaded:
			return TEXT("cook.native.cache.poll");
		default:
			return TEXT("cook.native.unknown");
		}
	}

	FString BuildNativeCookTraceThreadName(const FString& ThreadName)
	{
		return FString::Printf(TEXT("CookNative/%s"), ThreadName.IsEmpty() ? TEXT("Unknown") : *ThreadName);
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

	struct FCtcNativeCookScopeTraceSpan
	{
		FString PackageName;
		FString AssetClass;
		FString Name;
		FString Category;
		FString ThreadName;
		int64 TimestampUs = 0;
		int64 DurationUs = 0;
		uint32 SourceThreadId = 0;
	};

	struct FCtcCookNativeTraceAnalysisResults
	{
		TArray<FCtcNativeCookScopeTraceSpan> ScopeSpans;
		int32 UnmatchedScopeEnds = 0;
		int32 UnclosedScopeBegins = 0;
		bool bSawCookPackageEvent = false;
		bool bSawCookScopeEvent = false;
	};

	class FCtcCookNativeScopeAnalyzer final : public UE::Trace::IAnalyzer
	{
	public:
		explicit FCtcCookNativeScopeAnalyzer(TSharedRef<FCtcCookNativeTraceAnalysisResults> InResults)
			: Results(MoveTemp(InResults))
		{
		}

		virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
		{
			Context.InterfaceBuilder.RouteEvent(RouteId_Package, "CookTrace", "Package");
			Context.InterfaceBuilder.RouteEvent(RouteId_PackageAssetClass, "CookTrace", "PackageAssetClass");
			Context.InterfaceBuilder.RouteEvent(RouteId_PackageStatBeginScope, "CookTrace", "PackageStatBeginScope");
			Context.InterfaceBuilder.RouteEvent(RouteId_PackageStatEndScope, "CookTrace", "PackageStatEndScope");
		}

		virtual void OnThreadInfo(const FThreadInfo& ThreadInfo) override
		{
			FString ThreadName = FString(ANSI_TO_TCHAR(ThreadInfo.GetName()));
			if (ThreadName.IsEmpty())
			{
				ThreadName = FString::Printf(TEXT("Thread %u"), ThreadInfo.GetId());
			}

			ThreadNames.Add(ThreadInfo.GetId(), MoveTemp(ThreadName));
		}

		virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
		{
			if (Style == EStyle::LeaveScope)
			{
				return true;
			}

			const auto& EventData = Context.EventData;
			switch (RouteId)
			{
			case RouteId_Package:
			{
				uint64 PackageId = EventData.GetValue<uint64>("Id");
				FString PackageName;
				EventData.GetString("Name", PackageName);
				PackageNames.Add(PackageId, MoveTemp(PackageName));
				Results->bSawCookPackageEvent = true;
				break;
			}
			case RouteId_PackageAssetClass:
			{
				uint64 PackageId = EventData.GetValue<uint64>("Id");
				FString AssetClass;
				EventData.GetString("ClassName", AssetClass);
				PackageAssetClasses.Add(PackageId, MoveTemp(AssetClass));
				break;
			}
			case RouteId_PackageStatBeginScope:
			{
				FPendingScope& PendingScope = PendingScopesByThread.FindOrAdd(Context.ThreadInfo.GetId()).AddDefaulted_GetRef();
				PendingScope.PackageId = EventData.GetValue<uint64>("Id");
				PendingScope.StatType = static_cast<EPackageEventStatType>(EventData.GetValue<uint8>("StatType"));
				PendingScope.StartSeconds = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Time"));
				Results->bSawCookScopeEvent = true;
				break;
			}
			case RouteId_PackageStatEndScope:
			{
				const uint32 ThreadId = Context.ThreadInfo.GetId();
				const uint64 PackageId = EventData.GetValue<uint64>("Id");
				const EPackageEventStatType StatType = static_cast<EPackageEventStatType>(EventData.GetValue<uint8>("StatType"));
				const double EndSeconds = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Time"));
				TArray<FPendingScope>* PendingScopes = PendingScopesByThread.Find(ThreadId);

				if (PendingScopes == nullptr)
				{
					++Results->UnmatchedScopeEnds;
					break;
				}

				for (int32 PendingIndex = PendingScopes->Num() - 1; PendingIndex >= 0; --PendingIndex)
				{
					const FPendingScope& PendingScope = (*PendingScopes)[PendingIndex];
					if (PendingScope.PackageId != PackageId || PendingScope.StatType != StatType)
					{
						continue;
					}

					const double DurationSeconds = EndSeconds - PendingScope.StartSeconds;
					if (DurationSeconds > 0.0)
					{
						FCtcNativeCookScopeTraceSpan& CompletedScope = Results->ScopeSpans.AddDefaulted_GetRef();
						CompletedScope.PackageName = PackageNames.FindRef(PackageId);
						if (CompletedScope.PackageName.IsEmpty())
						{
							CompletedScope.PackageName = TEXT("Package_") + LexToString(PackageId);
						}
						CompletedScope.AssetClass = PackageAssetClasses.FindRef(PackageId);
						CompletedScope.Name = PackageEventStatTypeToString(StatType);
						CompletedScope.Category = PackageEventStatCategoryToString(StatType);
						CompletedScope.ThreadName = ResolveThreadName(Context.ThreadInfo);
						CompletedScope.TimestampUs = SecondsToTraceMicroseconds(PendingScope.StartSeconds);
						CompletedScope.DurationUs = SecondsToTraceMicroseconds(DurationSeconds);
						CompletedScope.SourceThreadId = ThreadId;
					}

					PendingScopes->RemoveAtSwap(PendingIndex);
					Results->bSawCookScopeEvent = true;
					return true;
				}

				++Results->UnmatchedScopeEnds;
				break;
			}
			default:
				break;
			}

			return true;
		}

		virtual void OnAnalysisEnd() override
		{
			for (const TPair<uint32, TArray<FPendingScope>>& PendingThreadScopes : PendingScopesByThread)
			{
				Results->UnclosedScopeBegins += PendingThreadScopes.Value.Num();
			}
		}

	private:
		enum : uint16
		{
			RouteId_Package = 1,
			RouteId_PackageAssetClass,
			RouteId_PackageStatBeginScope,
			RouteId_PackageStatEndScope
		};

		struct FPendingScope
		{
			uint64 PackageId = 0;
			EPackageEventStatType StatType = EPackageEventStatType::LoadPackage;
			double StartSeconds = 0.0;
		};

		FString ResolveThreadName(const FThreadInfo& ThreadInfo) const
		{
			if (const FString* ExistingThreadName = ThreadNames.Find(ThreadInfo.GetId()))
			{
				return *ExistingThreadName;
			}

			const FString EventThreadName = FString(ANSI_TO_TCHAR(ThreadInfo.GetName()));
			return EventThreadName.IsEmpty()
				? FString::Printf(TEXT("Thread %u"), ThreadInfo.GetId())
				: EventThreadName;
		}

		TSharedRef<FCtcCookNativeTraceAnalysisResults> Results;
		TMap<uint64, FString> PackageNames;
		TMap<uint64, FString> PackageAssetClasses;
		TMap<uint32, FString> ThreadNames;
		TMap<uint32, TArray<FPendingScope>> PendingScopesByThread;
	};

	class FCtcCookNativeTraceAnalysisModule final : public TraceServices::IModule
	{
	public:
		virtual void GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo) override
		{
			OutModuleInfo.Name = TEXT("CastToCloudCookNativeTrace");
			OutModuleInfo.DisplayName = TEXT("CastToCloud Cook Native Trace");
		}

		virtual bool ShouldBeEnabledByDefault() const override
		{
			return true;
		}

		virtual void OnAnalysisBegin(TraceServices::IAnalysisSession& Session) override
		{
			if (PendingResults.IsValid())
			{
				Session.AddAnalyzer(MakeShared<FCtcCookNativeScopeAnalyzer>(PendingResults.ToSharedRef()));
			}
		}

		TSharedRef<FCtcCookNativeTraceAnalysisResults> BeginAnalysis()
		{
			check(!PendingResults.IsValid());
			PendingResults = MakeShared<FCtcCookNativeTraceAnalysisResults>();
			return PendingResults.ToSharedRef();
		}

		void EndAnalysis()
		{
			PendingResults.Reset();
		}

	private:
		TSharedPtr<FCtcCookNativeTraceAnalysisResults> PendingResults;
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
			IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &NativeTraceAnalysisModule);
			FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices")).GetModuleService();

			TraceStoppedHandle = FTraceAuxiliary::OnTraceStopped.AddRaw(this, &FCtcCookObserver::HandleNativeTraceStopped);
			CookStartedHandle = UE::Cook::FDelegates::CookStarted.AddRaw(this, &FCtcCookObserver::HandleCookStarted);
			CookFinishedHandle = UE::Cook::FDelegates::CookFinished.AddRaw(this, &FCtcCookObserver::HandleCookFinished);
			CookUpdateDisplayHandle = UE::Cook::FDelegates::CookUpdateDisplay.AddRaw(this, &FCtcCookObserver::HandleCookUpdateDisplay);
			CookSaveIdleHandle = UE::Cook::FDelegates::CookSaveIdle.AddRaw(this, &FCtcCookObserver::HandleCookSaveIdle);
			CookLoadIdleHandle = UE::Cook::FDelegates::CookLoadIdle.AddRaw(this, &FCtcCookObserver::HandleCookLoadIdle);
			PackageBlockedHandle = UE::Cook::FDelegates::PackageBlocked.AddRaw(this, &FCtcCookObserver::HandlePackageBlocked);
			PreSavePackageHandle = UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePreSavePackage);
			PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePackageSaved);
			EndLoadPackageHandle = FCoreUObjectDelegates::OnEndLoadPackage.AddRaw(this, &FCtcCookObserver::HandleEndLoadPackage);

			RegisterChunkGeneratorFactory();

			UE_LOG(LogCastToCloudMetricsEditor, Display, TEXT("Cook observability hooks registered."));
		}

		void Shutdown()
		{
			if (bOwnsNativeCookTrace)
			{
				bPendingTraceWriteAfterNativeStop = false;
				FTraceAuxiliary::Stop();
				bOwnsNativeCookTrace = false;
			}

			if (TraceStoppedHandle.IsValid())
			{
				FTraceAuxiliary::OnTraceStopped.Remove(TraceStoppedHandle);
				TraceStoppedHandle.Reset();
			}

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
			ResetTraceSession();
			IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &NativeTraceAnalysisModule);

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

		bool StartNativeCookTraceCapture()
		{
			NativeCookScopeTraceSpans.Empty();
			NativeCookTracePath.Empty();
			bOwnsNativeCookTrace = false;
			bPendingTraceWriteAfterNativeStop = false;

			if (FTraceAuxiliary::GetTraceSystemStatus() == FTraceAuxiliary::ETraceSystemStatus::NotAvailable)
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[NativeCookTraceStartSkipped] reason=TraceUnavailable")
				);
				return false;
			}

			if (FTraceAuxiliary::IsConnected())
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[NativeCookTraceStartSkipped] reason=TraceAlreadyConnected destination=%s"),
					*FTraceAuxiliary::GetTraceDestinationString()
				);
				return false;
			}

			const FString TraceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CastToCloud"), TEXT("CookNativeTraces"));
			IFileManager::Get().MakeDirectory(*TraceDirectory, true);

			const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
			const FString TraceFilename = FString::Printf(TEXT("CookNativeTrace-%s-%03d.utrace"), *Timestamp, ++NativeCookTraceFileSequence);
			NativeCookTracePath = NormalizeTracePath(FPaths::Combine(TraceDirectory, TraceFilename));

			FTraceAuxiliary::FOptions TraceOptions;
			TraceOptions.bExcludeTail = true;
			TraceOptions.bTruncateFile = true;

			if (!FTraceAuxiliary::Start(
				FTraceAuxiliary::EConnectionType::File,
				*NativeCookTracePath,
				TEXT("default,cook"),
				&TraceOptions,
				LogCastToCloudMetricsEditor
			))
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[NativeCookTraceStartFailed] path=%s"),
					*NativeCookTracePath
				);
				NativeCookTracePath.Empty();
				return false;
			}

			bOwnsNativeCookTrace = true;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[NativeCookTraceStart] path=%s channels=default,cook"),
				*NativeCookTracePath
			);

			return true;
		}

		bool StopNativeCookTraceCapture()
		{
			if (!bOwnsNativeCookTrace || NativeCookTracePath.IsEmpty())
			{
				return false;
			}

			bPendingTraceWriteAfterNativeStop = true;
			if (!FTraceAuxiliary::Stop())
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[NativeCookTraceStopFailed] path=%s"),
					*NativeCookTracePath
				);
				bPendingTraceWriteAfterNativeStop = false;
				bOwnsNativeCookTrace = false;
				NativeCookTracePath.Empty();
				return false;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[NativeCookTraceStopRequested] path=%s"),
				*NativeCookTracePath
			);

			return true;
		}

		bool AnalyzeNativeCookTraceFile(const FString& TracePath)
		{
			NativeCookScopeTraceSpans.Empty();

			if (TracePath.IsEmpty())
			{
				return false;
			}

			TSharedPtr<TraceServices::IAnalysisService> AnalysisService =
				FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices")).GetAnalysisService();
			if (!AnalysisService.IsValid())
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[NativeCookTraceAnalysisFailed] path=%s reason=NoAnalysisService"),
					*TracePath
				);
				return false;
			}

			TSharedRef<FCtcCookNativeTraceAnalysisResults> AnalysisResults = NativeTraceAnalysisModule.BeginAnalysis();
			const TSharedPtr<const TraceServices::IAnalysisSession> AnalysisSession = AnalysisService->Analyze(*TracePath);
			NativeTraceAnalysisModule.EndAnalysis();

			if (!AnalysisSession.IsValid())
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[NativeCookTraceAnalysisFailed] path=%s reason=AnalyzeReturnedNull"),
					*TracePath
				);
				return false;
			}

			NativeCookScopeTraceSpans = MoveTemp(AnalysisResults->ScopeSpans);
			NativeCookScopeTraceSpans.Sort(
				[](const FCtcNativeCookScopeTraceSpan& Left, const FCtcNativeCookScopeTraceSpan& Right)
				{
					if (Left.TimestampUs != Right.TimestampUs)
					{
						return Left.TimestampUs < Right.TimestampUs;
					}

					if (Left.ThreadName != Right.ThreadName)
					{
						return Left.ThreadName < Right.ThreadName;
					}

					return Left.Name < Right.Name;
				}
			);

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[NativeCookTraceAnalysis] path=%s scopes=%d sawScopes=%s unmatchedEnds=%d unclosedBegins=%d"),
				*TracePath,
				NativeCookScopeTraceSpans.Num(),
				BoolToString(AnalysisResults->bSawCookScopeEvent),
				AnalysisResults->UnmatchedScopeEnds,
				AnalysisResults->UnclosedScopeBegins
			);

			return AnalysisResults->bSawCookScopeEvent;
		}

		void HandleNativeTraceStopped(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
		{
			if (!bPendingTraceWriteAfterNativeStop || TraceType != FTraceAuxiliary::EConnectionType::File)
			{
				return;
			}

			const FString StoppedTracePath = NormalizeTracePath(TraceDestination);
			if (!StoppedTracePath.Equals(NativeCookTracePath, ESearchCase::IgnoreCase))
			{
				return;
			}

			bPendingTraceWriteAfterNativeStop = false;
			bOwnsNativeCookTrace = false;

			AnalyzeNativeCookTraceFile(StoppedTracePath);
			WriteTraceFile();
			ResetTraceSession();
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
			CurrentCookSessionSummary = DescribeCookSession(CookInfo);
			StartNativeCookTraceCapture();
			SessionStartSeconds = FPlatformTime::Seconds();
			GetOrAddTraceThreadId(TEXT("CookMain"));
			GetOrAddTraceThreadId(TEXT("CookLoad"));
			GetOrAddTraceThreadId(TEXT("CookWait"));

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
			const int32 UnpairedPackageSaveCount = ActivePackageSaveSpans.Num();
			FlushActivePackageSpans(TEXT("CookFinished"));

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

			if (StopNativeCookTraceCapture())
			{
				return;
			}

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

			TArray<FString> PackageNames;
			PackageNames.Reserve(FMath::Min(Context.LoadedPackages.Num(), 8));

			int32 Index = 0;
			for (UPackage* LoadedPackage : Context.LoadedPackages)
			{
				if (LoadedPackage != nullptr && Index < 8)
				{
					PackageNames.Add(LoadedPackage->GetName());
				}
				++Index;
			}

			FCtcLoadBatchTraceEvent& LoadEvent = LoadBatchTraceEvents.AddDefaulted_GetRef();
			LoadEvent.TimestampUs = GetRelativeTraceTimestampUs(FPlatformTime::Seconds());
			LoadEvent.ThreadId = GetOrAddTraceThreadId(TEXT("CookLoad"));
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
				!NativeCookScopeTraceSpans.IsEmpty() ||
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
			LoadBatchTraceEvents.Sort([](const FCtcLoadBatchTraceEvent& Left, const FCtcLoadBatchTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			IdleTraceEvents.Sort([](const FCtcIdleTraceEvent& Left, const FCtcIdleTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			BlockedObjectTraceEvents.Sort([](const FCtcBlockedObjectTraceEvent& Left, const FCtcBlockedObjectTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });
			ProgressTraceEvents.Sort([](const FCtcProgressTraceEvent& Left, const FCtcProgressTraceEvent& Right) { return Left.TimestampUs < Right.TimestampUs; });

			for (const FCtcNativeCookScopeTraceSpan& NativeScopeSpan : NativeCookScopeTraceSpans)
			{
				GetOrAddTraceThreadId(BuildNativeCookTraceThreadName(NativeScopeSpan.ThreadName));
			}

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

			for (const FCtcNativeCookScopeTraceSpan& NativeScopeSpan : NativeCookScopeTraceSpans)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), NativeScopeSpan.Name);
				Writer->WriteValue(TEXT("cat"), NativeScopeSpan.Category);
				Writer->WriteValue(TEXT("ph"), TEXT("X"));
				Writer->WriteValue(TEXT("pid"), TraceProcessId);
				Writer->WriteValue(TEXT("tid"), GetOrAddTraceThreadId(BuildNativeCookTraceThreadName(NativeScopeSpan.ThreadName)));
				Writer->WriteValue(TEXT("ts"), NativeScopeSpan.TimestampUs);
				Writer->WriteValue(TEXT("dur"), NativeScopeSpan.DurationUs);
				Writer->WriteObjectStart(TEXT("args"));
				Writer->WriteValue(TEXT("package"), NativeScopeSpan.PackageName);
				Writer->WriteValue(TEXT("assetClass"), NativeScopeSpan.AssetClass);
				Writer->WriteValue(TEXT("sourceThreadId"), NativeScopeSpan.SourceThreadId);
				if (!CurrentCookSessionSummary.IsEmpty())
				{
					Writer->WriteValue(TEXT("session"), CurrentCookSessionSummary);
				}
				if (!NativeCookTracePath.IsEmpty())
				{
					Writer->WriteValue(TEXT("nativeTrace"), NativeCookTracePath);
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
					TEXT("[CookTraceWriteFailed] path=%s spans=%d"),
					*TracePath,
					CompletedPackageTraceSpans.Num()
				);
				return;
			}

			LastTraceOutputPath = TracePath;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookTraceWrite] path=%s spans=%d"),
				*TracePath,
				CompletedPackageTraceSpans.Num()
			);
		}

		void ResetTraceSession()
		{
			ActivePackageSaveSpans.Empty();
			CompletedPackageTraceSpans.Empty();
			NativeCookScopeTraceSpans.Empty();
			LoadBatchTraceEvents.Empty();
			IdleTraceEvents.Empty();
			BlockedObjectTraceEvents.Empty();
			ProgressTraceEvents.Empty();
			TraceThreadIds.Empty();
			CurrentCookSessionSummary.Empty();
			NativeCookTracePath.Empty();
			SessionStartSeconds = 0.0;
			SessionEndSeconds = 0.0;
			NextTraceThreadId = 1;
			bOwnsNativeCookTrace = false;
			bPendingTraceWriteAfterNativeStop = false;
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
		FCtcCookNativeTraceAnalysisModule NativeTraceAnalysisModule;
		FDelegateHandle TraceStoppedHandle;
		FDelegateHandle CookStartedHandle;
		FDelegateHandle CookFinishedHandle;
		FDelegateHandle CookUpdateDisplayHandle;
		FDelegateHandle CookSaveIdleHandle;
		FDelegateHandle CookLoadIdleHandle;
		FDelegateHandle PackageBlockedHandle;
		FDelegateHandle PreSavePackageHandle;
		FDelegateHandle PackageSavedHandle;
		FDelegateHandle EndLoadPackageHandle;

		UE::Cook::ICookInfo* ActiveCookInfo = nullptr;
		TRefCountPtr<FCtcCookMpCollector> MpCollector;
		TMap<FString, FCtcActivePackageTraceSpan> ActivePackageSaveSpans;
		TArray<FCtcCompletedPackageTraceSpan> CompletedPackageTraceSpans;
		TArray<FCtcNativeCookScopeTraceSpan> NativeCookScopeTraceSpans;
		TArray<FCtcLoadBatchTraceEvent> LoadBatchTraceEvents;
		TArray<FCtcIdleTraceEvent> IdleTraceEvents;
		TArray<FCtcBlockedObjectTraceEvent> BlockedObjectTraceEvents;
		TArray<FCtcProgressTraceEvent> ProgressTraceEvents;
		TMap<FString, int32> TraceThreadIds;
		FString CurrentCookSessionSummary;
		FString NativeCookTracePath;
		FString LastTraceOutputPath;
		double SessionStartSeconds = 0.0;
		double SessionEndSeconds = 0.0;
		double HeartbeatAccumulatorSeconds = 0.0;
		int32 LastCookedPackagesCount = INDEX_NONE;
		int32 LastCookPendingCount = INDEX_NONE;
		int32 NextTraceThreadId = 1;
		int32 NativeCookTraceFileSequence = 0;
		int32 TraceFileSequence = 0;
		bool bCookActive = false;
		bool bLoggedCookCompleteTick = false;
		bool bOwnsNativeCookTrace = false;
		bool bPendingTraceWriteAfterNativeStop = false;
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
