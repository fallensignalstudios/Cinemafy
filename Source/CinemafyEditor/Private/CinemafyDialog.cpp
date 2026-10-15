#include "CinemafyDialog.h"
#include "CinemafyModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace CinemafyDialog
{
	namespace
	{
		int32 QuietDepth = 0;
	}

	bool IsInteractive()
	{
		return QuietDepth == 0 && !FApp::IsUnattended() && !IsRunningCommandlet() && !GIsRunningUnattendedScript && FSlateApplication::IsInitialized();
	}

	EAppReturnType::Type Open(EAppMsgType::Type Type, const FText& Message, EAppReturnType::Type Unattended)
	{
		if (IsInteractive())
		{
			return FMessageDialog::Open(Type, Message);
		}
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: %s"), *Message.ToString());
		if (FSlateApplication::IsInitialized())
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 6.f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		return Unattended;
	}

	FQuiet::FQuiet()
	{
		++QuietDepth;
	}

	FQuiet::~FQuiet()
	{
		QuietDepth = FMath::Max(0, QuietDepth - 1);
	}

	void SetQuiet(bool bQuiet)
	{
		QuietDepth = FMath::Max(0, QuietDepth + (bQuiet ? 1 : -1));
	}
}
