//GlobalInputBridgeModule.h
//插件统一入口头文件

#pragma once

#include "Modules/ModuleManager.h"

/*声明自定义日志类别
  *参数 1: 类别名称 (通常以 Log 开头，如 LogGlobalInput)
  *参数 2: 默认的日志级别 (DefaultVerbosity)
  *参数 3: 编译时保留的最高日志级别 (CompileTimeVerbosity)
 */
DECLARE_LOG_CATEGORY_EXTERN(LogGlobalInput, Log, All);

class FGlobalInputBridgeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
