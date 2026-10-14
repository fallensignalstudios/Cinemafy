#include "CinemafyListenerComponent.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyGazeComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** How a temperament listens. Chances are per opportunity (phrase end, emphasis...). */
	struct FTemperamentParams
	{
		float NodChance;
		float ShakeChance;
		float SmileChance;
		float BrowChance;
		float BlinkOnEmphasis;
		float TiltChance;
		/** Chance to glance away per long-listen check (every ~2s of continuous talk). */
		float AversionChance;
		float Responsiveness;
		float HeadDrift;
		float BaseHeadPitch;
		float BaseSmile;
		float BaseFurrow;
	};

	FTemperamentParams GetTemperament(ECinemafyTemperament Temperament)
	{
		switch (Temperament)
		{
		//                                         Nod   Shake Smile Brow  Blink Tilt  Avert Resp  Drift Pitch  Smile Furrow
		case ECinemafyTemperament::Attentive: return { 0.65f, 0.0f, 0.10f, 0.35f, 0.5f, 0.20f, 0.15f, 1.0f, 0.8f,  0.f, 0.05f, 0.0f };
		case ECinemafyTemperament::Warm:      return { 0.60f, 0.0f, 0.45f, 0.30f, 0.4f, 0.30f, 0.15f, 0.9f, 0.9f,  0.f, 0.20f, 0.0f };
		case ECinemafyTemperament::Stoic:     return { 0.20f, 0.0f, 0.00f, 0.10f, 0.3f, 0.05f, 0.08f, 0.6f, 0.4f,  0.f, 0.00f, 0.05f };
		case ECinemafyTemperament::Skeptical: return { 0.20f, 0.2f, 0.05f, 0.15f, 0.4f, 0.45f, 0.25f, 0.9f, 0.8f,  0.f, 0.00f, 0.30f };
		case ECinemafyTemperament::Nervous:   return { 0.45f, 0.0f, 0.10f, 0.30f, 0.7f, 0.15f, 0.55f, 1.1f, 1.6f, -2.f, 0.00f, 0.15f };
		case ECinemafyTemperament::Hostile:   return { 0.05f, 0.25f, 0.0f, 0.05f, 0.2f, 0.10f, 0.05f, 0.8f, 0.5f,  3.f, 0.00f, 0.55f };
		}
		return GetTemperament(ECinemafyTemperament::Attentive);
	}

	/** Attack / hold / release envelope over a gesture's normalized time. */
	float Envelope(float T, float Attack, float Release)
	{
		if (T < Attack)
		{
			return FMath::SmoothStep(0.f, 1.f, T / Attack);
		}
		if (T > 1.f - Release)
		{
			return FMath::SmoothStep(0.f, 1.f, (1.f - T) / Release);
		}
		return 1.f;
	}

	float GestureDuration(ECinemafyReaction Reaction)
	{
		switch (Reaction)
		{
		case ECinemafyReaction::Nod:       return 0.5f;
		case ECinemafyReaction::SmallNod:  return 0.35f;
		case ECinemafyReaction::DoubleNod: return 0.85f;
		case ECinemafyReaction::HeadShake: return 0.9f;
		case ECinemafyReaction::Tilt:      return 1.8f;
		case ECinemafyReaction::BrowRaise: return 0.8f;
		case ECinemafyReaction::Smile:     return 2.2f;
		case ECinemafyReaction::Frown:     return 1.6f;
		case ECinemafyReaction::LookAway:  return 1.2f;
		default:                           return 0.2f;
		}
	}

	/** Minimum gap between automatic gestures, so reactions never feel like a metronome. */
	constexpr float MinGestureGap = 0.9f;
}

UCinemafyListenerComponent::UCinemafyListenerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Before animation, so the Anim Blueprints read this frame's values.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickInEditor = true;
}

void UCinemafyListenerComponent::OnRegister()
{
	Super::OnRegister();

	Random.Initialize(Seed);
	LocalTime = 0.f;
	Gestures.Reset();
	bInPhrase = false;
	NextIdleTime = Random.FRandRange(2.f, 5.f);
	SetSpeaker(Speaker);
}

UCinemafyGazeComponent* UCinemafyListenerComponent::GetGaze() const
{
	return (bDriveGaze && GetOwner()) ? GetOwner()->FindComponentByClass<UCinemafyGazeComponent>() : nullptr;
}

void UCinemafyListenerComponent::SetSpeaker(AActor* NewSpeaker)
{
	Speaker = NewSpeaker;
	if (UCinemafyGazeComponent* Gaze = GetGaze())
	{
		if (NewSpeaker)
		{
			Gaze->SetLookAtTarget(NewSpeaker);
		}
	}
}

float UCinemafyListenerComponent::ReadSpeechLevel(const AActor* Actor) const
{
	if (!Actor)
	{
		return 0.f;
	}

	float Level = 0.f;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		const UAnimInstance* Anim = Mesh->GetAnimInstance();
		if (!Anim)
		{
			continue;
		}
		for (const FName& Curve : SpeechCurves)
		{
			Level = FMath::Max(Level, Anim->GetCurveValue(Curve));
		}
	}
	return FMath::Clamp(Level, 0.f, 1.f);
}

float UCinemafyListenerComponent::RollStrength()
{
	return FMath::Clamp(Engagement, 0.f, 1.5f) * GetTemperament(Temperament).Responsiveness * Random.FRandRange(0.7f, 1.1f);
}

bool UCinemafyListenerComponent::TryGesture(ECinemafyReaction Reaction, float Chance, float Strength)
{
	if (LocalTime - LastGestureTime < MinGestureGap)
	{
		return false;
	}
	// Engagement scales how often a listener bothers to react at all.
	if (Random.FRand() >= Chance * FMath::Clamp(Engagement, 0.f, 1.5f))
	{
		return false;
	}

	React(Reaction, Strength);
	return true;
}

void UCinemafyListenerComponent::React(ECinemafyReaction Reaction, float Strength)
{
	LastGestureTime = LocalTime;
	OnReaction.Broadcast(Reaction, Strength);

	if (Reaction == ECinemafyReaction::Blink)
	{
		if (UCinemafyGazeComponent* Gaze = GetGaze())
		{
			Gaze->Blink();
		}
		return;
	}

	FGesture Gesture;
	Gesture.Type = Reaction;
	Gesture.StartTime = LocalTime;
	Gesture.Duration = GestureDuration(Reaction) * Random.FRandRange(0.85f, 1.15f);
	Gesture.Strength = Strength;
	Gesture.Sign = Random.FRand() < 0.5f ? -1.f : 1.f;
	Gestures.Add(Gesture);

	// People tend to blink as they look away.
	if (Reaction == ECinemafyReaction::LookAway)
	{
		if (UCinemafyGazeComponent* Gaze = GetGaze())
		{
			Gaze->Blink();
		}
	}
}

void UCinemafyListenerComponent::OnPhraseEnd(float PhraseLength)
{
	// Fragments aren't worth acknowledging.
	if (PhraseLength < 0.5f)
	{
		return;
	}

	// The classic backchannel: a nod in the gap, bigger after longer phrases.
	const FTemperamentParams P = GetTemperament(Temperament);
	const ECinemafyReaction NodType = (PhraseLength > 2.5f && Random.FRand() < 0.35f) ? ECinemafyReaction::DoubleNod : ECinemafyReaction::Nod;
	if (TryGesture(NodType, P.NodChance, RollStrength()))
	{
		// A warm nod often carries a little smile with it.
		if (Random.FRand() < P.SmileChance)
		{
			React(ECinemafyReaction::Smile, RollStrength() * 0.6f);
		}
		return;
	}

	if (TryGesture(ECinemafyReaction::HeadShake, P.ShakeChance, RollStrength() * 0.6f)) { return; }
	if (TryGesture(ECinemafyReaction::Smile, P.SmileChance, RollStrength())) { return; }
	TryGesture(ECinemafyReaction::BrowRaise, P.BrowChance, RollStrength() * 0.7f);
}

void UCinemafyListenerComponent::OnEmphasis()
{
	const FTemperamentParams P = GetTemperament(Temperament);

	// Blinks on stressed words are involuntary, so they skip the gesture cooldown.
	if (Random.FRand() < P.BlinkOnEmphasis * FMath::Clamp(Engagement, 0.f, 1.f))
	{
		if (UCinemafyGazeComponent* Gaze = GetGaze())
		{
			Gaze->Blink();
		}
	}

	if (!TryGesture(ECinemafyReaction::BrowRaise, P.BrowChance, RollStrength() * 0.5f))
	{
		TryGesture(ECinemafyReaction::SmallNod, P.NodChance * 0.4f, RollStrength());
	}
}

void UCinemafyListenerComponent::OnLongListen()
{
	// Processing a long stretch of talk: tilt the head or glance away to think.
	const FTemperamentParams P = GetTemperament(Temperament);
	if (!TryGesture(ECinemafyReaction::LookAway, P.AversionChance, 1.f))
	{
		TryGesture(ECinemafyReaction::Tilt, P.TiltChance, RollStrength());
	}
}

void UCinemafyListenerComponent::OnIdleSilence()
{
	// Nobody's talking: small signs of life.
	const FTemperamentParams P = GetTemperament(Temperament);
	TryGesture(ECinemafyReaction::LookAway, 0.3f + P.AversionChance * 0.5f, 0.6f);
}

void UCinemafyListenerComponent::UpdateHearing(float DeltaTime)
{
	const float Raw = bUseManualSpeechEnergy ? ManualSpeechEnergy : ReadSpeechLevel(Speaker);

	// Fast envelope: quick attack, slower release, so gaps between syllables don't count as pauses.
	FastEnvelope = FMath::FInterpTo(FastEnvelope, Raw, DeltaTime, Raw > FastEnvelope ? 20.f : 6.f);
	// Slow envelope: the running "loudness" that emphasis stands out against.
	SlowEnvelope = FMath::FInterpTo(SlowEnvelope, FastEnvelope, DeltaTime, 1.2f);
	HeardEnergy = FastEnvelope;

	bSpeakerTalking = FastEnvelope > SpeechThreshold;
	if (bSpeakerTalking)
	{
		if (!bInPhrase)
		{
			bInPhrase = true;
			PhraseStartTime = LocalTime;
			NextLongListenTime = LocalTime + 4.f;
		}
		LastVoiceTime = LocalTime;

		if (FastEnvelope > SlowEnvelope * 1.8f && FastEnvelope > 0.2f && LocalTime - LastEmphasisTime > 1.f)
		{
			LastEmphasisTime = LocalTime;
			OnEmphasis();
		}

		if (LocalTime >= NextLongListenTime)
		{
			NextLongListenTime = LocalTime + Random.FRandRange(1.8f, 3.f);
			OnLongListen();
		}
	}
	else if (bInPhrase && LocalTime - LastVoiceTime > PhrasePause)
	{
		bInPhrase = false;
		OnPhraseEnd(LastVoiceTime - PhraseStartTime);
	}

	if (!bInPhrase && LocalTime - LastVoiceTime > 3.f && LocalTime >= NextIdleTime)
	{
		NextIdleTime = LocalTime + Random.FRandRange(3.f, 7.f);
		OnIdleSilence();
	}
}

void UCinemafyListenerComponent::EvaluateOutputs(float DeltaTime)
{
	const FTemperamentParams P = GetTemperament(Temperament);
	const float SeedOffset = static_cast<float>(Seed) * 13.1f;

	// Continuous life: tiny head drift, breathing, weight shifts.
	float Pitch = P.BaseHeadPitch + FMath::PerlinNoise1D(LocalTime * 0.21f + SeedOffset) * P.HeadDrift;
	float Yaw = FMath::PerlinNoise1D(LocalTime * 0.17f + SeedOffset + 50.f) * P.HeadDrift;
	float Roll = FMath::PerlinNoise1D(LocalTime * 0.13f + SeedOffset + 90.f) * P.HeadDrift * 0.6f;
	float Brow = 0.f;
	float Furrow = P.BaseFurrow;
	float SmileOut = P.BaseSmile;
	float SquintOut = P.BaseFurrow * 0.3f;
	float Aversion = 0.f;
	float AversionSign = 1.f;

	BreathAlpha = 0.5f + 0.5f * FMath::Sin(LocalTime * 0.25f * UE_TWO_PI + SeedOffset);
	WeightShift = FMath::PerlinNoise1D(LocalTime * 0.07f + SeedOffset + 200.f);

	// Layer the active gestures.
	for (int32 Index = Gestures.Num() - 1; Index >= 0; --Index)
	{
		const FGesture& G = Gestures[Index];
		const float T = (LocalTime - G.StartTime) / FMath::Max(G.Duration, KINDA_SMALL_NUMBER);
		if (T >= 1.f)
		{
			Gestures.RemoveAtSwap(Index);
			continue;
		}

		const float S = G.Strength;
		switch (G.Type)
		{
		case ECinemafyReaction::Nod:
			Pitch -= 7.f * S * FMath::Sin(PI * T);
			break;
		case ECinemafyReaction::SmallNod:
			Pitch -= 3.5f * S * FMath::Sin(PI * T);
			break;
		case ECinemafyReaction::DoubleNod:
			// Two dips, the second one softer.
			Pitch -= 6.f * S * FMath::Abs(FMath::Sin(UE_TWO_PI * T)) * (T < 0.5f ? 1.f : 0.6f);
			break;
		case ECinemafyReaction::HeadShake:
			Yaw += 8.f * S * FMath::Sin(3.f * PI * T) * (1.f - T);
			break;
		case ECinemafyReaction::Tilt:
		{
			const float E = Envelope(T, 0.25f, 0.35f);
			Roll += 7.f * S * G.Sign * E;
			Pitch += 1.5f * S * E;
			Brow += 0.2f * S * E;
			break;
		}
		case ECinemafyReaction::BrowRaise:
			Brow += S * Envelope(T, 0.2f, 0.45f);
			break;
		case ECinemafyReaction::Smile:
		{
			const float E = Envelope(T, 0.3f, 0.4f);
			SmileOut += 0.6f * S * E;
			SquintOut += 0.25f * S * E;
			break;
		}
		case ECinemafyReaction::Frown:
			Furrow += 0.7f * S * Envelope(T, 0.25f, 0.4f);
			break;
		case ECinemafyReaction::LookAway:
		{
			const float E = Envelope(T, 0.15f, 0.3f);
			Aversion = FMath::Max(Aversion, E * S);
			AversionSign = G.Sign;
			// The head follows the eyes a little.
			Yaw += 4.f * S * G.Sign * E;
			Pitch -= 2.f * S * E;
			break;
		}
		default:
			break;
		}
	}

	// Hand off to the speaker: fade out while this character is the one talking.
	const float SelfLevel = bSuppressWhileSpeaking ? ReadSpeechLevel(GetOwner()) : 0.f;
	SelfSpeakingAlpha = FMath::FInterpTo(SelfSpeakingAlpha, SelfLevel > SpeechThreshold ? 1.f : 0.f, DeltaTime, SelfLevel > SpeechThreshold ? 8.f : 1.5f);
	const float W = FMath::Clamp(Weight, 0.f, 1.f) * (1.f - SelfSpeakingAlpha);

	HeadRotation = FRotator(Pitch * W, Yaw * W, Roll * W);
	BrowRaise = FMath::Clamp(Brow * W, 0.f, 1.f);
	BrowFurrow = FMath::Clamp(Furrow * W, 0.f, 1.f);
	Smile = FMath::Clamp(SmileOut * W, 0.f, 1.f);
	Squint = FMath::Clamp(SquintOut * W, 0.f, 1.f);

	// Thinking glance: eyes slide off the speaker to the side and a little down.
	if (UCinemafyGazeComponent* Gaze = GetGaze())
	{
		FVector Offset = FVector::ZeroVector;
		if (Speaker && Aversion > 0.f)
		{
			const FVector MyEyes = UCinemafyFramingLibrary::GetEyePoint(GetOwner());
			const FVector Theirs = UCinemafyFramingLibrary::GetEyePoint(Speaker);
			const float Distance = FVector::Dist(MyEyes, Theirs);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, (Theirs - MyEyes).GetSafeNormal2D());
			// About 15 degrees aside and 8 down.
			Offset = (Right * (0.27f * AversionSign) - FVector::UpVector * 0.14f) * Distance * Aversion * W;
		}
		Gaze->LookAtOffset = Offset;
	}
}

void UCinemafyListenerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LocalTime += DeltaTime;

	// Only listen while not talking ourselves.
	if (!bSuppressWhileSpeaking || SelfSpeakingAlpha < 0.5f)
	{
		UpdateHearing(DeltaTime);
	}
	EvaluateOutputs(DeltaTime);
}
