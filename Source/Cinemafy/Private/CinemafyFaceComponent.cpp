#include "CinemafyFaceComponent.h"

namespace
{
	enum class ESide : uint8 { Both, Left, Right };

	struct FPoseEntry
	{
		const TCHAR* Control;
		float Value;
		ESide Side;
	};

	/** Built-in emotion poses, as MetaHuman CTRL_expressions_ controls (without the L/R suffix). */
	TArrayView<const FPoseEntry> GetPoseTable(ECinemafyEmotion Emotion)
	{
		static const FPoseEntry Happy[] = {
			{ TEXT("mouthCornerPull"), 0.55f, ESide::Both }, { TEXT("eyeCheekRaise"), 0.45f, ESide::Both },
			{ TEXT("eyeSquintInner"), 0.25f, ESide::Both }, { TEXT("browRaiseOuter"), 0.05f, ESide::Both } };
		static const FPoseEntry Sad[] = {
			{ TEXT("browRaiseIn"), 0.6f, ESide::Both }, { TEXT("mouthCornerDepress"), 0.45f, ESide::Both },
			{ TEXT("mouthLowerLipDepress"), 0.1f, ESide::Both }, { TEXT("eyeSquintInner"), 0.15f, ESide::Both } };
		static const FPoseEntry Angry[] = {
			{ TEXT("browDown"), 0.65f, ESide::Both }, { TEXT("browLateral"), 0.45f, ESide::Both },
			{ TEXT("eyeSquintInner"), 0.35f, ESide::Both }, { TEXT("noseWrinkle"), 0.25f, ESide::Both },
			{ TEXT("mouthUpperLipRaise"), 0.15f, ESide::Both }, { TEXT("jawClench"), 0.3f, ESide::Both } };
		static const FPoseEntry Fear[] = {
			{ TEXT("browRaiseIn"), 0.7f, ESide::Both }, { TEXT("browRaiseOuter"), 0.35f, ESide::Both },
			{ TEXT("browLateral"), 0.3f, ESide::Both }, { TEXT("eyeWiden"), 0.55f, ESide::Both },
			{ TEXT("mouthStretch"), 0.35f, ESide::Both } };
		static const FPoseEntry Surprise[] = {
			{ TEXT("browRaiseIn"), 0.75f, ESide::Both }, { TEXT("browRaiseOuter"), 0.75f, ESide::Both },
			{ TEXT("eyeWiden"), 0.6f, ESide::Both } };
		static const FPoseEntry Disgust[] = {
			{ TEXT("noseWrinkle"), 0.6f, ESide::Both }, { TEXT("mouthUpperLipRaise"), 0.45f, ESide::Both },
			{ TEXT("browDown"), 0.3f, ESide::Both }, { TEXT("eyeSquintInner"), 0.3f, ESide::Both },
			{ TEXT("eyeCheekRaise"), 0.2f, ESide::Both } };
		// Contempt is famously one-sided.
		static const FPoseEntry Contempt[] = {
			{ TEXT("mouthCornerPull"), 0.35f, ESide::Right }, { TEXT("mouthDimple"), 0.3f, ESide::Right },
			{ TEXT("eyeSquintInner"), 0.15f, ESide::Right } };
		static const FPoseEntry Determined[] = {
			{ TEXT("browDown"), 0.35f, ESide::Both }, { TEXT("browLateral"), 0.2f, ESide::Both },
			{ TEXT("eyeSquintInner"), 0.2f, ESide::Both }, { TEXT("jawClench"), 0.35f, ESide::Both } };
		static const FPoseEntry Worried[] = {
			{ TEXT("browRaiseIn"), 0.5f, ESide::Both }, { TEXT("browLateral"), 0.35f, ESide::Both },
			{ TEXT("mouthStretch"), 0.15f, ESide::Both }, { TEXT("eyeSquintInner"), 0.1f, ESide::Both } };

		switch (Emotion)
		{
		case ECinemafyEmotion::Happy:      return Happy;
		case ECinemafyEmotion::Sad:        return Sad;
		case ECinemafyEmotion::Angry:      return Angry;
		case ECinemafyEmotion::Fear:       return Fear;
		case ECinemafyEmotion::Surprise:   return Surprise;
		case ECinemafyEmotion::Disgust:    return Disgust;
		case ECinemafyEmotion::Contempt:   return Contempt;
		case ECinemafyEmotion::Determined: return Determined;
		case ECinemafyEmotion::Worried:    return Worried;
		default:                           return {};
		}
	}
}

UCinemafyFaceComponent::UCinemafyFaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickInEditor = true;

	EmotionWeights.Init(0.f, static_cast<int32>(ECinemafyEmotion::Count));
	EmotionWeights[static_cast<int32>(ECinemafyEmotion::Neutral)] = 1.f;
}

TMap<FName, float> UCinemafyFaceComponent::GetBuiltInEmotionPose(ECinemafyEmotion InEmotion)
{
	TMap<FName, float> Pose;
	for (const FPoseEntry& Entry : GetPoseTable(InEmotion))
	{
		const FString Base = FString(TEXT("CTRL_expressions_")) + Entry.Control;
		if (Entry.Side != ESide::Right)
		{
			Pose.Add(FName(*(Base + TEXT("L"))), Entry.Value);
		}
		if (Entry.Side != ESide::Left)
		{
			Pose.Add(FName(*(Base + TEXT("R"))), Entry.Value);
		}
	}
	return Pose;
}

void UCinemafyFaceComponent::SetEmotion(ECinemafyEmotion NewEmotion, float BlendTime)
{
	Emotion = NewEmotion;
	EmotionBlendTime = BlendTime;
}

void UCinemafyFaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Crossfade every emotion toward 1 (the current one) or 0 (the rest).
	const float Step = EmotionBlendTime > 0.f ? DeltaTime / EmotionBlendTime : 1.f;
	for (int32 Index = 0; Index < EmotionWeights.Num(); ++Index)
	{
		const float Target = (Index == static_cast<int32>(Emotion)) ? 1.f : 0.f;
		EmotionWeights[Index] = FMath::Clamp(EmotionWeights[Index] + FMath::Sign(Target - EmotionWeights[Index]) * Step, 0.f, 1.f);
		if (FMath::Abs(EmotionWeights[Index] - Target) < Step)
		{
			EmotionWeights[Index] = Target;
		}
	}
}
