#include "AnimNode_CinemafyFace.h"
#include "CinemafyFaceComponent.h"
#include "CinemafyFaceProfile.h"
#include "CinemafyGazeComponent.h"
#include "CinemafyListenerComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"

namespace
{
	const FString ExpressionPrefix(TEXT("CTRL_expressions_"));

	/** A big DeltaTime means a scrub, a cut or a hitch: snap filters instead of smoothing across it. */
	constexpr float MaxFilterDeltaTime = 0.25f;

	FName Control(const TCHAR* Name)
	{
		return FName(*(ExpressionPrefix + Name));
	}

	/** Adds Value to both the L and R versions of a control. */
	void AddBoth(TMap<FName, float>& Additive, const TCHAR* Name, float Value)
	{
		Additive.FindOrAdd(Control(*(FString(Name) + TEXT("L")))) += Value;
		Additive.FindOrAdd(Control(*(FString(Name) + TEXT("R")))) += Value;
	}

	/** One Euro filter smoothing factor for a cutoff frequency (Hz). */
	float SmoothingFactor(float CutoffHz, float DeltaTime)
	{
		const float Tau = 1.f / (UE_TWO_PI * CutoffHz);
		return 1.f / (1.f + Tau / DeltaTime);
	}
}

void FAnimNode_CinemafyFace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	CurveStates.Reset();
	PendingDeltaTime = 0.f;
}

void FAnimNode_CinemafyFace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
}

void FAnimNode_CinemafyFace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);
	Source.Update(Context);
	PendingDeltaTime += Context.GetDeltaTime();
}

void FAnimNode_CinemafyFace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.2f, Face: %d, Gaze: %d, Listener: %d, Seal: %.2f, Tuck: %.2f, Gate: %.2f)"),
		Alpha, Input.bHasFace, Input.bHasGaze, Input.bHasListener, Seal, Tuck, SilenceGateAlpha);
	DebugData.AddDebugItem(DebugLine);
	Source.GatherDebugData(DebugData);
}

void FAnimNode_CinemafyFace::PreUpdate(const UAnimInstance* InAnimInstance)
{
	// Game thread: snapshot everything we need from the character's components.
	Input = FFrameInput();
	const AActor* Owner = InAnimInstance ? InAnimInstance->GetOwningActor() : nullptr;
	if (!Owner)
	{
		return;
	}

	Profile = nullptr;
	if (const UCinemafyFaceComponent* Face = Owner->FindComponentByClass<UCinemafyFaceComponent>())
	{
		Input.bHasFace = true;
		Input.PerformanceIntensity = Face->PerformanceIntensity;
		Input.EmotionIntensity = Face->EmotionIntensity;
		Input.FaceLife = Face->FaceLife;
		Input.bBreathing = Face->bBreathing;
		Input.BreathRate = Face->BreathRate;
		Input.bPupilResponse = Face->bPupilResponse;
		Input.EmotionWeights = Face->GetEmotionWeights();
		Input.Articulation = Face->Articulation;
		Input.LipSync = Face->LipSync;
		Profile = Face->Profile;
	}

	// Emotion poses only change with the profile, so build them here (game thread) and cache.
	if (EmotionPoses.Num() == 0 || EmotionPoseSource != Profile)
	{
		EmotionPoseSource = Profile;
		EmotionPoses.SetNum(static_cast<int32>(ECinemafyEmotion::Count));
		for (int32 Index = 0; Index < EmotionPoses.Num(); ++Index)
		{
			const ECinemafyEmotion Emotion = static_cast<ECinemafyEmotion>(Index);
			const FCinemafyEmotionPose* Custom = Profile ? Profile->CustomEmotions.Find(Emotion) : nullptr;
			EmotionPoses[Index] = Custom ? Custom->Curves : UCinemafyFaceComponent::GetBuiltInEmotionPose(Emotion);
		}
		RegionCache.Reset();
		FaceCurveCache.Reset();
	}

	if (const UCinemafyGazeComponent* Gaze = Owner->FindComponentByClass<UCinemafyGazeComponent>())
	{
		Input.bHasGaze = Gaze->IsActive();
		Input.EyeLook = Gaze->EyeLookNormalized;
		Input.GazeBlink = Gaze->BlinkAlpha;
		Input.GazeWeight = FMath::Clamp(Gaze->GazeWeight, 0.f, 1.f);
	}

	if (const UCinemafyListenerComponent* Listener = Owner->FindComponentByClass<UCinemafyListenerComponent>())
	{
		Input.bHasListener = Listener->IsActive();
		Input.BrowRaise = Listener->BrowRaise;
		Input.BrowFurrow = Listener->BrowFurrow;
		Input.Smile = Listener->Smile;
		Input.Squint = Listener->Squint;
	}
}

ECinemafyFaceRegion FAnimNode_CinemafyFace::ClassifyCurve(FName Curve, const FString& Prefix)
{
	if (const ECinemafyFaceRegion* Cached = RegionCache.Find(Curve))
	{
		return *Cached;
	}

	const FString Short = Curve.ToString().RightChop(Prefix.Len()).ToLower();
	ECinemafyFaceRegion Region = ECinemafyFaceRegion::Other;
	if (Short.StartsWith(TEXT("brow")))                                    { Region = ECinemafyFaceRegion::Brows; }
	else if (Short.Contains(TEXT("cheek")))                                { Region = ECinemafyFaceRegion::Cheeks; }
	else if (Short.StartsWith(TEXT("eye")))                                { Region = ECinemafyFaceRegion::Eyes; }
	else if (Short.StartsWith(TEXT("nose")))                               { Region = ECinemafyFaceRegion::Nose; }
	else if (Short.StartsWith(TEXT("jaw")))                                { Region = ECinemafyFaceRegion::Jaw; }
	else if (Short.StartsWith(TEXT("tongue")))                             { Region = ECinemafyFaceRegion::Tongue; }
	else if (Short.StartsWith(TEXT("mouth")) || Short.StartsWith(TEXT("lip")) || Short.StartsWith(TEXT("teeth"))) { Region = ECinemafyFaceRegion::Mouth; }

	RegionCache.Add(Curve, Region);
	return Region;
}

float FAnimNode_CinemafyFace::ProcessCurve(FName Curve, float Value, const UCinemafyFaceProfile& InProfile, float DeltaTime)
{
	const FCinemafyFaceRegionSettings& Region = InProfile.GetRegion(ClassifyCurve(Curve, InProfile.CurvePrefix));

	// 1) Gain / offset: boost or calm the performance.
	float V = Value * Region.Gain * Input.PerformanceIntensity + Region.Offset;
	if (const FCinemafyCurveOverride* Override = InProfile.CurveOverrides.Find(Curve))
	{
		V = V * Override->Gain + Override->Offset;
	}

	FCurveState& State = CurveStates.FindOrAdd(Curve);
	if (!State.bInitialized || DeltaTime <= 0.f || DeltaTime > MaxFilterDeltaTime)
	{
		State.bInitialized = true;
		State.Filtered = V;
		State.FilteredSpeed = 0.f;
		State.SpringPos = V;
		State.SpringVel = 0.f;
		return V;
	}

	// 2) One Euro filter: heavy smoothing when the curve is slow (that's where jitter shows),
	//    light smoothing when it moves fast (so blinks and plosives don't lag).
	if (Region.JitterReduction > 0.f)
	{
		const float MinCutoff = FMath::Lerp(30.f, 1.5f, Region.JitterReduction);
		constexpr float SpeedCoefficient = 4.f;
		const float Speed = (V - State.Filtered) / DeltaTime;
		State.FilteredSpeed = FMath::Lerp(State.FilteredSpeed, Speed, SmoothingFactor(1.f, DeltaTime));
		const float Cutoff = MinCutoff + SpeedCoefficient * FMath::Abs(State.FilteredSpeed);
		State.Filtered = FMath::Lerp(State.Filtered, V, SmoothingFactor(Cutoff, DeltaTime));
		V = State.Filtered;
	}
	else
	{
		State.Filtered = V;
	}

	// 3) Flesh: a damped spring chasing the curve gives a little overshoot and settle.
	if (Region.Springiness > 0.f)
	{
		const float Omega = FMath::Lerp(18.f, 5.f, Region.Springiness) * UE_TWO_PI;
		const float Zeta = FMath::Lerp(1.f, 0.4f, Region.Springiness);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime * 240.f));
		const float H = DeltaTime / Steps;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Accel = Omega * Omega * (V - State.SpringPos) - 2.f * Zeta * Omega * State.SpringVel;
			State.SpringVel += Accel * H;
			State.SpringPos += State.SpringVel * H;
		}
		V = State.SpringPos;
	}
	else
	{
		State.SpringPos = V;
		State.SpringVel = 0.f;
	}

	return V;
}

void FAnimNode_CinemafyFace::AddEmotions(TMap<FName, float>& Additive) const
{
	for (int32 Index = 0; Index < Input.EmotionWeights.Num() && Index < EmotionPoses.Num(); ++Index)
	{
		const float Weight = Input.EmotionWeights[Index] * Input.EmotionIntensity;
		if (Weight <= 0.f)
		{
			continue;
		}
		for (const TPair<FName, float>& Pair : EmotionPoses[Index])
		{
			Additive.FindOrAdd(Pair.Key) += Pair.Value * Weight;
		}
	}
}

void FAnimNode_CinemafyFace::AddLife(TMap<FName, float>& Additive) const
{
	// Slow, slightly asymmetric drift: real faces are never perfectly still or perfectly mirrored.
	struct FLifeChannel { const TCHAR* Control; float Amplitude; float Frequency; };
	static const FLifeChannel Channels[] = {
		{ TEXT("browRaiseIn"),     0.05f,  0.23f },
		{ TEXT("browRaiseOuter"),  0.03f,  0.19f },
		{ TEXT("mouthCornerPull"), 0.035f, 0.17f },
		{ TEXT("eyeSquintInner"),  0.03f,  0.29f },
		{ TEXT("eyeCheekRaise"),   0.02f,  0.21f },
		{ TEXT("noseWrinkle"),     0.015f, 0.13f },
	};

	const float Life = FMath::Max(Input.FaceLife, 0.f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Channels); ++Index)
	{
		const FLifeChannel& Channel = Channels[Index];
		const float Seed = Index * 17.3f;
		const float Left = 0.5f + 0.5f * FMath::PerlinNoise1D(Time * Channel.Frequency + Seed);
		const float Right = 0.5f + 0.5f * FMath::PerlinNoise1D(Time * Channel.Frequency * 1.07f + Seed + 5.1f);
		Additive.FindOrAdd(Control(*(FString(Channel.Control) + TEXT("L")))) += Left * Channel.Amplitude * Life;
		Additive.FindOrAdd(Control(*(FString(Channel.Control) + TEXT("R")))) += Right * Channel.Amplitude * Life;
	}
}

void FAnimNode_CinemafyFace::ApplyLipSync(const TMap<FName, float>& Raw, TMap<FName, float>& Result, const UCinemafyFaceProfile& InProfile, float DeltaTime)
{
	const FCinemafyLipSyncSettings& Settings = Input.LipSync;
	const float Articulation = FMath::Max(Input.Articulation, 0.f);
	if (!Settings.bEnabled || Articulation <= 0.f)
	{
		return;
	}

	const FCinemafyLipSyncCurves& Curves = InProfile.LipSyncCurves;
	const float Intensity = Input.PerformanceIntensity;

	// Detection reads the raw performance: smoothing rounds off exactly the moments we're looking for.
	auto Average = [&Raw](const TArray<FName>& Names)
	{
		float Sum = 0.f;
		int32 Found = 0;
		for (const FName& Name : Names)
		{
			if (const float* Value = Raw.Find(Name))
			{
				Sum += *Value;
				++Found;
			}
		}
		return Found > 0 ? Sum / Found : 0.f;
	};

	const float* RawJaw = Raw.Find(Curves.Jaw);
	const float Jaw = RawJaw ? *RawJaw * Intensity : 0.f;
	const float Together = Average(Curves.Closure) * Intensity;
	const float Bite = Average(Curves.Labiodental) * Intensity;

	// Consonants only count mid-speech: a performer resting with closed lips between lines isn't saying "b".
	if (Jaw > 0.06f)
	{
		LastJawActivityTime = Time;
	}
	const float InSpeech = (Time - LastJawActivityTime) < 0.5f ? 1.f : 0.f;

	// P / B / M: lips heading together while the jaw is nearly shut.
	const float ClosureScore = InSpeech * FMath::Clamp((Together - 0.2f) / 0.3f, 0.f, 1.f) * FMath::Clamp((0.3f - Jaw) / 0.2f, 0.f, 1.f);
	if (ClosureScore > 0.5f)
	{
		ClosureHoldUntil = Time + Settings.ClosureHoldMs / 1000.f;
	}
	const float SealTarget = Time < ClosureHoldUntil ? 1.f : 0.f;
	// Seal fast, release a little slower, so the "pop" of the plosive still reads.
	Seal = FMath::FInterpTo(Seal, SealTarget, DeltaTime, SealTarget > Seal ? 40.f : 18.f);

	// F / V: lower lip tucking under the teeth.
	const float BiteScore = InSpeech * FMath::Clamp((Bite - 0.15f) / 0.3f, 0.f, 1.f);
	if (BiteScore > 0.5f)
	{
		TuckHoldUntil = Time + Settings.LabiodentalHoldMs / 1000.f;
	}
	const float TuckTarget = Time < TuckHoldUntil ? 1.f : 0.f;
	Tuck = FMath::FInterpTo(Tuck, TuckTarget, DeltaTime, TuckTarget > Tuck ? 35.f : 15.f);

	// Talking = jaw moving or a consonant shape in the last moment (bridges gaps between words).
	if (Jaw > 0.04f || ClosureScore > 0.5f || BiteScore > 0.5f)
	{
		LastSpeechTime = Time;
	}
	const bool bSpeaking = Time - LastSpeechTime < 0.35f;
	SilenceGateAlpha = FMath::FInterpTo(SilenceGateAlpha, bSpeaking ? 0.f : 1.f, DeltaTime, 6.f);

	const float SealAmount = FMath::Clamp(Seal * Settings.ClosureAssist * Articulation, 0.f, 1.f);
	const float TuckAmount = FMath::Clamp(Tuck * Settings.LabiodentalAssist * Articulation, 0.f, 1.f);

	// Jaw: lift small openings (a gentle compressor), then shut it for seals and tucks.
	if (float* JawValue = Result.Find(Curves.Jaw))
	{
		float V = FMath::Max(*JawValue, 0.f);
		if (Settings.JawLift > 0.f)
		{
			V = FMath::Pow(V, 1.f / (1.f + Settings.JawLift * Articulation));
		}
		V = FMath::Lerp(V, FMath::Min(V, 0.02f), SealAmount);
		V = FMath::Lerp(V, FMath::Min(V, 0.08f), TuckAmount);
		*JawValue = V;
	}

	for (const FName& Name : Curves.Closure)
	{
		if (float* Value = Result.Find(Name))
		{
			*Value = FMath::Lerp(*Value, 1.f, SealAmount);
		}
	}
	for (const FName& Name : Curves.Labiodental)
	{
		if (float* Value = Result.Find(Name))
		{
			*Value = FMath::Lerp(*Value, FMath::Max(*Value, 0.75f), TuckAmount);
		}
	}
	for (const FName& Name : Curves.Rounding)
	{
		if (float* Value = Result.Find(Name))
		{
			*Value *= 1.f + Settings.RoundingBoost * Articulation;
		}
	}
	for (const FName& Name : Curves.Wide)
	{
		if (float* Value = Result.Find(Name))
		{
			*Value *= 1.f + Settings.WideBoost * Articulation;
		}
	}

	// Silence gate: squash small mouth values (mocap noise) with a soft knee, leave real expressions alone.
	const float Gate = FMath::Clamp(Settings.SilenceGate * Articulation, 0.f, 1.f) * SilenceGateAlpha;
	if (Gate > 0.f)
	{
		constexpr float Knee = 0.1f;
		for (TPair<FName, float>& Pair : Result)
		{
			const ECinemafyFaceRegion Region = ClassifyCurve(Pair.Key, InProfile.CurvePrefix);
			const bool bMouthArea = Region == ECinemafyFaceRegion::Mouth || Region == ECinemafyFaceRegion::Jaw || Region == ECinemafyFaceRegion::Tongue;
			if (bMouthArea && Pair.Value < Knee)
			{
				Pair.Value *= 1.f - Gate * (1.f - Pair.Value / Knee);
			}
		}
	}
}

void FAnimNode_CinemafyFace::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);

	const float DeltaTime = PendingDeltaTime;
	PendingDeltaTime = 0.f;
	Time += DeltaTime;

	const float Blend = FMath::Clamp(Alpha, 0.f, 1.f);
	if (Blend <= 0.f)
	{
		return;
	}

	const UCinemafyFaceProfile& ActiveProfile = Profile ? *Profile : *UCinemafyFaceProfile::GetDefaultProfile();
	const FString& Prefix = ActiveProfile.CurvePrefix;

	// The performance as it arrives, for every face control curve.
	TMap<FName, float> Original;
	Output.Curve.ForEachElement([this, &Original, &Prefix](const UE::Anim::FCurveElement& Element)
	{
		bool* bIsFaceCurve = FaceCurveCache.Find(Element.Name);
		if (!bIsFaceCurve)
		{
			bIsFaceCurve = &FaceCurveCache.Add(Element.Name, Element.Name.ToString().StartsWith(Prefix));
		}
		if (*bIsFaceCurve)
		{
			Original.Add(Element.Name, Element.Value);
		}
	});

	// 1) Process the performance: gain, jitter cleanup, flesh.
	TMap<FName, float> Result;
	Result.Reserve(Original.Num());
	for (const TPair<FName, float>& Pair : Original)
	{
		Result.Add(Pair.Key, bApplyFace ? ProcessCurve(Pair.Key, Pair.Value, ActiveProfile, DeltaTime) : Pair.Value);
	}

	// 2) Lip sync: crisp closures and tucks, clearer vowels, quiet mouth in silence.
	if (bApplyFace && Input.bHasFace)
	{
		ApplyLipSync(Original, Result, ActiveProfile, DeltaTime);
	}

	// 3) Layers on top: emotion, life, breathing, listening, pupils.
	TMap<FName, float> Additive;
	if (bApplyFace && Input.bHasFace)
	{
		AddEmotions(Additive);
		AddLife(Additive);

		if (Input.bBreathing)
		{
			const float Breath = 0.5f + 0.5f * FMath::Sin(Time * Input.BreathRate * UE_TWO_PI);
			AddBoth(Additive, TEXT("noseNostrilDilate"), 0.12f * Breath);
			Additive.FindOrAdd(Control(TEXT("jawOpen"))) += 0.015f * Breath;
		}
	}

	if (bApplyListener && Input.bHasListener)
	{
		AddBoth(Additive, TEXT("browRaiseIn"), Input.BrowRaise);
		AddBoth(Additive, TEXT("browRaiseOuter"), Input.BrowRaise * 0.7f);
		AddBoth(Additive, TEXT("browDown"), Input.BrowFurrow);
		AddBoth(Additive, TEXT("browLateral"), Input.BrowFurrow * 0.6f);
		AddBoth(Additive, TEXT("mouthCornerPull"), Input.Smile);
		AddBoth(Additive, TEXT("eyeCheekRaise"), Input.Smile * 0.5f);
		AddBoth(Additive, TEXT("eyeSquintInner"), Input.Squint);
	}

	for (const TPair<FName, float>& Pair : Additive)
	{
		Result.FindOrAdd(Pair.Key) += Pair.Value;
	}

	// 4) Gaze: eye look (blended over the performer's) and blinks (whichever is more closed wins).
	const FName BlinkL = Control(TEXT("eyeBlinkL"));
	const FName BlinkR = Control(TEXT("eyeBlinkR"));
	if (bApplyGaze && Input.bHasGaze)
	{
		const float LookBlend = GazeEyeLookBlend * Input.GazeWeight;
		const float Right = FMath::Max(Input.EyeLook.X, 0.f);
		const float Left = FMath::Max(-Input.EyeLook.X, 0.f);
		const float Up = FMath::Max(Input.EyeLook.Y, 0.f);
		const float Down = FMath::Max(-Input.EyeLook.Y, 0.f);
		const TPair<const TCHAR*, float> Looks[] = {
			{ TEXT("eyeLookRight"), Right }, { TEXT("eyeLookLeft"), Left }, { TEXT("eyeLookUp"), Up }, { TEXT("eyeLookDown"), Down } };
		for (const TPair<const TCHAR*, float>& Look : Looks)
		{
			for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
			{
				float& Value = Result.FindOrAdd(Control(*(FString(Look.Key) + Side)));
				Value = FMath::Lerp(Value, Look.Value, LookBlend);
			}
		}

		float& BL = Result.FindOrAdd(BlinkL);
		BL = FMath::Max(BL, Input.GazeBlink);
		float& BR = Result.FindOrAdd(BlinkR);
		BR = FMath::Max(BR, Input.GazeBlink);
	}

	// 5) Pupils briefly constrict as the lids reopen after any blink (performed or procedural).
	if (bApplyFace && Input.bHasFace && Input.bPupilResponse)
	{
		const float* BlinkValue = Result.Find(BlinkL);
		const float Blink = BlinkValue ? *BlinkValue : 0.f;
		if (PreviousBlink > 0.7f && Blink < 0.3f)
		{
			PupilConstriction = 1.f;
		}
		PreviousBlink = Blink;
		PupilConstriction *= FMath::Exp(-DeltaTime / 0.6f);
		AddBoth(Result, TEXT("eyePupilNarrow"), 0.35f * PupilConstriction);
	}

	// 6) Write back, clamped to the control range and blended by Alpha.
	for (const TPair<FName, float>& Pair : Result)
	{
		const float* Before = Original.Find(Pair.Key);
		const float From = Before ? *Before : 0.f;
		Output.Curve.Set(Pair.Key, FMath::Lerp(From, FMath::Clamp(Pair.Value, 0.f, 1.f), Blend));
	}
}
