#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FCinemafyEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();

	FDelegateHandle DirectorRequestHandle;
	FDelegateHandle ChoreoRequestHandle;
	FDelegateHandle SequencerCreatedHandle;
};
