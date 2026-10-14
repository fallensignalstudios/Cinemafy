#include "CinemafySettings.h"
#include "CinemafyChoreographyTypes.h"

const UCinemafyMoveLibrary* UCinemafySettings::GetDefaultMoveLibrary()
{
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	return Settings ? Settings->DefaultMoveLibrary.LoadSynchronous() : nullptr;
}

float UCinemafySettings::GetFolderBias(const FString& PackagePath)
{
	const UCinemafySettings* Settings = GetDefault<UCinemafySettings>();
	if (!Settings)
	{
		return 0.f;
	}
	int32 Longest = -1;
	float Bias = 0.f;
	for (const TPair<FString, float>& Entry : Settings->FolderBias)
	{
		FString Folder = Entry.Key;
		Folder.RemoveFromEnd(TEXT("/"));
		if (Folder.Len() > Longest && (PackagePath == Folder || PackagePath.StartsWith(Folder + TEXT("/"))))
		{
			Longest = Folder.Len();
			Bias = Entry.Value;
		}
	}
	return Bias;
}
