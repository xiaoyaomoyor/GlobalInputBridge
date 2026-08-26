#include "Logging/GlobalInputLog.h"

#include "GlobalInputBridgeModule.h"

TAtomic<uint8> FGlobalInputLog::Level(
	static_cast<uint8>(EGlobalInputLogLevel::Warning));

void FGlobalInputLog::SetLevel(
	EGlobalInputLogLevel InLevel)
{
	Level.Store(
		static_cast<uint8>(InLevel),
		EMemoryOrder::Relaxed);

	switch (InLevel)
	{
	case EGlobalInputLogLevel::None:
		UE_SET_LOG_VERBOSITY(LogGlobalInput, NoLogging);
		break;
	case EGlobalInputLogLevel::Error:
		UE_SET_LOG_VERBOSITY(LogGlobalInput, Error);
		break;
	case EGlobalInputLogLevel::Warning:
		UE_SET_LOG_VERBOSITY(LogGlobalInput, Warning);
		break;
	case EGlobalInputLogLevel::Verbose:
		UE_SET_LOG_VERBOSITY(LogGlobalInput, VeryVerbose);
		break;
	default:
		UE_SET_LOG_VERBOSITY(LogGlobalInput, Warning);
		break;
	}
}

bool FGlobalInputLog::ShouldLog(
	EGlobalInputLogLevel RequiredLevel)
{
	return RequiredLevel != EGlobalInputLogLevel::None &&
		Level.Load(EMemoryOrder::Relaxed) >=
			static_cast<uint8>(RequiredLevel);
}
