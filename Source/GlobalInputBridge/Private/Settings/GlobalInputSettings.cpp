#include "Settings/GlobalInputSettings.h"

#if WITH_EDITOR
#include "Logging/GlobalInputLog.h"
#include "UObject/UnrealType.h"

void UGlobalInputSettings::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FGlobalInputLog::SetLevel(LogLevel);
}
#endif
