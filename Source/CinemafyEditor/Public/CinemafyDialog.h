#pragma once

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"

/**
 * Dialogs only for a person at the editor. Scripts, MCP tools, commandlets and -unattended runs get a log line and a
 * toast instead (and the default answer), so an automated call can never hang on a modal window.
 */
namespace CinemafyDialog
{
	/** A person is at the editor: not -unattended, not a commandlet or unattended script, not inside a quiet call. */
	CINEMAFYEDITOR_API bool IsInteractive();

	/** A modal dialog for a person; otherwise logged (and toasted) and answered with Unattended. */
	CINEMAFYEDITOR_API EAppReturnType::Type Open(EAppMsgType::Type Type, const FText& Message, EAppReturnType::Type Unattended = EAppReturnType::Ok);

	/** Calls made while one of these is alive never open dialogs (scripts and MCP tools hold one around each call). */
	struct CINEMAFYEDITOR_API FQuiet
	{
		FQuiet();
		~FQuiet();
	};

	/** For scripts: quiet on (true) / off (false), nesting. */
	CINEMAFYEDITOR_API void SetQuiet(bool bQuiet);
}
