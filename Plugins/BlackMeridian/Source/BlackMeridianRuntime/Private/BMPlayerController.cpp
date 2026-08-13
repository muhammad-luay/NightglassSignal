// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMPlayerController.h"

#include "Algo/Sort.h"
#include "BMGameMode.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMAudioDataAsset.h"
#include "BMCampaignLevel.h"
#include "BMHUD.h"
#include "BMNativeUIWidget.h"
#include "BMGameUserSettings.h"
#include "BMInputRemap.h"
#include "BMMissionDirector.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerState.h"
#include "BMSignalControlGameMode.h"
#include "BMWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Containers/StringConv.h"
#include "Containers/Ticker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    /** Generated cues are project-owned presentation assets, not a substitute
     * for a future recorded mix. They sit between configured audio profiles
     * and the procedural source-only safety net. */
    USoundBase* LoadGeneratedCombatCue(const TCHAR* CueName)
    {
        if (!CueName || !*CueName)
        {
            return nullptr;
        }
        const FString AssetPath = FString::Printf(
            TEXT("/Game/Audio/Generated/%s.%s"),
            CueName,
            CueName);
        return LoadObject<USoundBase>(nullptr, *AssetPath);
    }

    const TArray<FName>& GetRemappableActions()
    {
        return BMInputRemap::GetRemappableActions();
    }

    const TArray<FString>& GetRemappableLabels()
    {
        static const TArray<FString> Labels = {
            TEXT("FIRE"),
            TEXT("AIM"),
            TEXT("RELOAD"),
            TEXT("INTERACT"),
            TEXT("SPRINT"),
            TEXT("CROUCH"),
            TEXT("PRONE"),
            TEXT("JUMP / MANTLE"),
            TEXT("MELEE"),
            TEXT("FRAG"),
            TEXT("TACTICAL"),
            TEXT("PAUSE"),
            TEXT("SCOREBOARD")};
        return Labels;
    }

    const TArray<TArray<FKey>>& GetRemappableKeyPresets()
    {
        static const TArray<TArray<FKey>> Presets = {
            {EKeys::LeftMouseButton, EKeys::MiddleMouseButton},
            {EKeys::RightMouseButton, EKeys::LeftAlt},
            {EKeys::R, EKeys::T},
            {EKeys::E, EKeys::F},
            {EKeys::LeftShift, EKeys::LeftControl},
            {EKeys::C, EKeys::LeftControl},
            {EKeys::Z, EKeys::X},
            {EKeys::SpaceBar, EKeys::RightAlt},
            {EKeys::V, EKeys::B},
            {EKeys::G, EKeys::H},
            {EKeys::X, EKeys::T},
            {EKeys::Escape, EKeys::P},
            {EKeys::Tab, EKeys::O}};
        return Presets;
    }

    FName GetMusicStateForMissionPhase(const EBMMissionPhase MissionPhase)
    {
        switch (MissionPhase)
        {
        case EBMMissionPhase::Checkpoint:
        case EBMMissionPhase::OpenCombat:
        case EBMMissionPhase::FinalDefense:
            return TEXT("Combat");
        case EBMMissionPhase::Failed:
            return TEXT("Failure");
        case EBMMissionPhase::Complete:
            return TEXT("Complete");
        case EBMMissionPhase::Intro:
            return TEXT("Mission");
        case EBMMissionPhase::Insertion:
        case EBMMissionPhase::QuietApproach:
        case EBMMissionPhase::InteriorBreach:
        case EBMMissionPhase::Intelligence:
        case EBMMissionPhase::Extraction:
        default:
            return TEXT("Suspense");
        }
    }

    /**
     * The generated music beds are imported as direct SoundWave assets. Their
     * content is intentionally finite so it can be cooked cheaply, therefore
     * the state router must opt them into looping at runtime. Failure and
     * Complete are deliberately left one-shot so the result screen retains
     * its ending cadence.
     */
    bool ConfigureMusicLoop(USoundBase* Music, const bool bShouldLoop)
    {
        if (USoundWave* SoundWave = Cast<USoundWave>(Music))
        {
            SoundWave->bLooping = bShouldLoop;
            return true;
        }
        return false;
    }

    bool IsNativeMenuSelectionScreen(const EBMHUDScreen Screen)
    {
        return Screen == EBMHUDScreen::MainMenu
            || Screen == EBMHUDScreen::MultiplayerMenu
            || Screen == EBMHUDScreen::Pause
            || Screen == EBMHUDScreen::Death
            || Screen == EBMHUDScreen::Results;
    }

    FKey GetCurrentKeyboardBinding(const FName ActionName)
    {
        const UInputSettings* InputSettings = GetDefault<UInputSettings>();
        if (!InputSettings)
        {
            return EKeys::Invalid;
        }
        for (const FInputActionKeyMapping& Mapping : InputSettings->GetActionMappings())
        {
            if (Mapping.ActionName == ActionName && !Mapping.Key.IsGamepadKey())
            {
                return Mapping.Key;
            }
        }
        return EKeys::Invalid;
    }

    /** Short local confirmation cues keep the combat loop legible when the
     * optional authored UI/audio assets are not present yet. */
    void PlayRuntimeCombatCue(
        const UObject* WorldContextObject,
        const UBMAudioDataAsset* AudioProfile,
        const bool bElimination,
        const bool bHeadshot = false)
    {
        if (!WorldContextObject || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
        {
            return;
        }

        USoundBase* ConfiguredCue = nullptr;
        if (AudioProfile)
        {
            ConfiguredCue = (bElimination
                ? AudioProfile->Elimination
                : AudioProfile->HitConfirm).LoadSynchronous();
        }

        USoundBase* Cue = ConfiguredCue;
        const TCHAR* CueSource = TEXT("profile");
        if (!Cue)
        {
            Cue = LoadGeneratedCombatCue(bElimination ? TEXT("BM_Elimination") : TEXT("BM_Hit_Confirm"));
            CueSource = TEXT("generated_asset");
        }

        if (Cue)
        {
            UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(
                WorldContextObject,
                Cue,
                bElimination ? 0.72f : 0.48f,
                bHeadshot ? 1.16f : (bElimination ? 1.0f : 1.08f));
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_AudioProbe")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian audio probe: combat cue=%s source=%s asset=%s started=%s headshot=%s"),
                    bElimination ? TEXT("elimination") : TEXT("hit_confirm"),
                    CueSource,
                    *GetNameSafe(Cue),
                    AudioComponent && AudioComponent->IsPlaying() ? TEXT("yes") : TEXT("no"),
                    bHeadshot ? TEXT("yes") : TEXT("no"));
            }
#endif
            return;
        }

        constexpr int32 SampleRate = 22050;
        // Headshot eliminations get a separate rising confirmation so the
        // audio agrees with the HUD's HEADSHOT // ELIMINATION callout.
        const float Duration = bElimination
            ? (bHeadshot ? 0.44f : 0.34f)
            : (bHeadshot ? 0.105f : 0.075f);
        const int32 SampleCount = FMath::Max(1, FMath::RoundToInt(Duration * SampleRate));
        TArray<int16> Samples;
        Samples.SetNumUninitialized(SampleCount);

        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float Time = static_cast<float>(Index) / static_cast<float>(SampleRate);
            const float NormalizedTime = Time / Duration;
            const float Envelope = FMath::Pow(FMath::Clamp(1.0f - NormalizedTime, 0.0f, 1.0f), 1.4f);
            float Signal = 0.0f;
            if (!bElimination)
            {
                const float PrimaryFrequency = bHeadshot ? 1980.0f : 1450.0f;
                const float SecondaryFrequency = bHeadshot ? 990.0f : 730.0f;
                Signal = FMath::Sin(Time * 2.0f * PI * PrimaryFrequency) * (bHeadshot ? 0.62f : 0.54f)
                    + FMath::Sin(Time * 2.0f * PI * SecondaryFrequency) * 0.16f;
            }
            else
            {
                const float SecondNoteStart = bHeadshot ? 0.145f : 0.13f;
                const bool bSecondNote = Time > SecondNoteStart;
                const float NoteTime = bSecondNote ? Time - SecondNoteStart : Time;
                const float NoteFrequency = bSecondNote
                    ? (bHeadshot ? 1240.0f : 980.0f)
                    : (bHeadshot ? 780.0f : 620.0f);
                const float NoteEnvelope = bSecondNote
                    ? FMath::Clamp((Time - SecondNoteStart) / (bHeadshot ? 0.055f : 0.045f), 0.0f, 1.0f)
                    : 1.0f;
                Signal = FMath::Sin(NoteTime * 2.0f * PI * NoteFrequency)
                    * (bHeadshot ? 0.48f : 0.42f)
                    * NoteEnvelope
                    + FMath::Sin(NoteTime * 2.0f * PI * (NoteFrequency * 2.01f)) * 0.11f;
                if (bHeadshot && Time > 0.285f)
                {
                    const float AccentTime = Time - 0.285f;
                    const float AccentFade = FMath::Clamp((Duration - Time) / 0.15f, 0.0f, 1.0f);
                    Signal += FMath::Sin(AccentTime * 2.0f * PI * 1560.0f) * 0.16f * AccentFade;
                }
            }
            Samples[Index] = static_cast<int16>(FMath::Clamp(Signal * Envelope, -1.0f, 1.0f) * 32767.0f);
        }

        USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
        if (!Wave)
        {
            return;
        }
        Wave->NumChannels = 1;
        Wave->SetSampleRate(SampleRate);
        Wave->Duration = Duration;
        Wave->SoundGroup = SOUNDGROUP_UI;
        Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(
            WorldContextObject,
            Wave,
            bElimination ? 0.68f : (bHeadshot ? 0.5f : 0.42f),
            bElimination ? 1.0f : (bHeadshot ? 1.16f : 1.08f));
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_AudioProbe")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian audio probe: combat cue=%s source=procedural_fallback asset=%s started=%s headshot=%s"),
                bElimination ? TEXT("elimination") : TEXT("hit_confirm"),
                *GetNameSafe(Wave),
                AudioComponent && AudioComponent->IsPlaying() ? TEXT("yes") : TEXT("no"),
                bHeadshot ? TEXT("yes") : TEXT("no"));
        }
#endif
    }

    /**
     * The source-only build has no licensed music assets yet. Keep the game
     * playable and emotionally legible by generating a small, loop-safe
     * two-channel synth bed at runtime. Authored MusicStates still take
     * precedence when a UBMAudioDataAsset is supplied.
     */
    TArray<uint8> BuildProceduralMusicSamples(const FName StateId, float& OutDuration, bool& bOutLooping)
    {
        constexpr int32 SampleRate = 22050;
        const bool bCombat = StateId == TEXT("Combat") || StateId == TEXT("SignalControl");
        const bool bSuspense = StateId == TEXT("Suspense");
        const bool bFailure = StateId == TEXT("Failure");
        const bool bComplete = StateId == TEXT("Complete");
        const bool bOneShot = bFailure || bComplete;

        // Keep the source-only fallback musical rather than silent. Suspense
        // gets a slower, dissonant loop so the approach/breach/extraction
        // phases feel intentionally different from the neutral mission bed.
        // The durations land on whole beat counts to keep the procedural loop
        // from restarting in the middle of a pulse pattern.
        OutDuration = bOneShot ? 4.5f : (bCombat ? 10.08f : (bSuspense ? 14.4f : 16.8f));
        bOutLooping = !bOneShot;

        const int32 FrameCount = FMath::Max(1, FMath::RoundToInt(OutDuration * SampleRate));
        TArray<int16> Samples;
        Samples.SetNumUninitialized(FrameCount * 2);

        for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
        {
            const float Time = static_cast<float>(FrameIndex) / static_cast<float>(SampleRate);
            const float BeatPeriod = bCombat ? 0.72f : (bSuspense ? 1.2f : 2.4f);
            const float BeatPhase = FMath::Fmod(Time, BeatPeriod) / BeatPeriod;
            const float BeatEnvelope = FMath::Pow(
                FMath::Max(0.0f, 1.0f - BeatPhase),
                bCombat ? 5.0f : (bSuspense ? 8.0f : 7.0f));
            const float SecondBeatPhase = FMath::Fmod(Time + BeatPeriod * 0.5f, BeatPeriod) / BeatPeriod;
            const float SecondBeatEnvelope = FMath::Pow(
                FMath::Max(0.0f, 1.0f - SecondBeatPhase),
                bCombat ? 6.0f : (bSuspense ? 9.0f : 8.0f));

            float Signal = 0.0f;
            if (bFailure)
            {
                const float FadeIn = FMath::Clamp(Time / 0.18f, 0.0f, 1.0f);
                const float FadeOut = FMath::Clamp((OutDuration - Time) / 1.2f, 0.0f, 1.0f);
                const float Envelope = FadeIn * FadeOut;
                Signal = Envelope * (
                    FMath::Sin(Time * 2.0f * PI * 73.42f) * 0.34f
                    + FMath::Sin(Time * 2.0f * PI * 110.0f) * 0.12f
                    + FMath::Sin(Time * 2.0f * PI * 146.83f) * 0.08f);
            }
            else if (bComplete)
            {
                const float FadeIn = FMath::Clamp(Time / 0.35f, 0.0f, 1.0f);
                const float FadeOut = FMath::Clamp((OutDuration - Time) / 1.0f, 0.0f, 1.0f);
                const float Envelope = FadeIn * FadeOut;
                Signal = Envelope * (
                    FMath::Sin(Time * 2.0f * PI * 220.0f) * 0.16f
                    + FMath::Sin(Time * 2.0f * PI * 277.18f) * 0.13f
                    + FMath::Sin(Time * 2.0f * PI * 329.63f) * 0.11f
                    + FMath::Sin(Time * 2.0f * PI * 440.0f) * 0.07f);
            }
            else
            {
                if (bSuspense)
                {
                    // G1 against a nearly adjacent low tone creates the
                    // uneasy beating texture, while the sparse heartbeat and
                    // high signal ping keep the bed readable under dialogue
                    // and combat audio.
                    const float Breath = 0.88f + FMath::Sin(Time * 2.0f * PI * 0.075f) * 0.12f;
                    const float Drone =
                        FMath::Sin(Time * 2.0f * PI * 49.0f) * 0.22f
                        + FMath::Sin(Time * 2.0f * PI * 50.5f + 0.4f) * 0.085f
                        + FMath::Sin(Time * 2.0f * PI * 73.42f + 0.9f) * 0.075f
                        + FMath::Sin(Time * 2.0f * PI * 77.78f + 1.2f) * 0.045f;
                    const float Pulse = FMath::Sin(Time * 2.0f * PI * 98.0f) * BeatEnvelope * 0.17f;
                    const float CounterPulse = FMath::Sin(Time * 2.0f * PI * 103.83f + 0.7f) * SecondBeatEnvelope * 0.085f;
                    const float SignalPing = FMath::Sin(Time * 2.0f * PI * 293.66f + 0.35f) * BeatEnvelope * 0.032f;
                    Signal = Drone * Breath + Pulse + CounterPulse + SignalPing;
                }
                else
                {
                    const float Root = bCombat ? 65.41f : 55.0f;
                    const float Fifth = bCombat ? 98.0f : 82.41f;
                    const float Tension = bCombat ? 123.47f : 116.54f;
                    const float Drone =
                        FMath::Sin(Time * 2.0f * PI * Root) * (bCombat ? 0.24f : 0.21f)
                        + FMath::Sin(Time * 2.0f * PI * Fifth + 0.35f) * (bCombat ? 0.12f : 0.10f)
                        + FMath::Sin(Time * 2.0f * PI * Tension + 1.1f) * (bCombat ? 0.06f : 0.045f);
                    const float Pulse = FMath::Sin(Time * 2.0f * PI * (bCombat ? 130.81f : 110.0f)) * BeatEnvelope * (bCombat ? 0.25f : 0.13f);
                    const float CounterPulse = FMath::Sin(Time * 2.0f * PI * (bCombat ? 196.0f : 146.83f)) * SecondBeatEnvelope * (bCombat ? 0.14f : 0.07f);
                    const float HighTension = FMath::Sin(Time * 2.0f * PI * (bCombat ? 392.0f : 233.08f)) * (BeatEnvelope * 0.045f);
                    Signal = Drone + Pulse + CounterPulse + HighTension;
                }
            }

            const float StereoMotion = FMath::Sin(Time * 2.0f * PI * (bCombat ? 0.19f : (bSuspense ? 0.13f : 0.11f))) * 0.035f;
            Samples[FrameIndex * 2] = static_cast<int16>(FMath::Clamp(Signal - StereoMotion, -0.92f, 0.92f) * 32767.0f);
            Samples[FrameIndex * 2 + 1] = static_cast<int16>(FMath::Clamp(Signal + StereoMotion, -0.92f, 0.92f) * 32767.0f);
        }

        TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        return Bytes;
    }
}

ABMPlayerController::ABMPlayerController()
{
    bReplicates = true;

    static ConstructorHelpers::FObjectFinder<UBMAudioDataAsset> CombatAudioProfile(
        TEXT("/Game/Data/Audio/DA_BM_CombatAudio.DA_BM_CombatAudio"));
    if (CombatAudioProfile.Succeeded())
    {
        AudioProfile = CombatAudioProfile.Object;
    }
}

void ABMPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // Applying saved resolution during BeginPlay can synchronously wait for
    // the macOS window-server fullscreen transition before the game viewport
    // has finished opening. Keep the initial launch path responsive; the
    // command-line resolution is already applied by the game window, and
    // settings changes later use the full display-settings path.
    ApplyLocalSettings(false);
    if (IsLocalController())
    {
#if !UE_BUILD_SHIPPING
        InitializeRemoteInputBridge();
#endif
        FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &ABMPlayerController::HandleInitialApplicationReactivation);
        StartAmbientAudio();
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
        // A packaged Mac launch can leave the game window behind the shell
        // that launched it. Defer the native activation until AppKit and the
        // first Slate window are fully initialized; doing this inline during
        // engine startup can crash UE5.8's macOS LaunchServices path.
        if (!FParse::Param(FCommandLine::Get(), TEXT("unattended")))
        {
            GetWorldTimerManager().SetTimer(
                NativeFocusTimer,
                this,
                &ABMPlayerController::ActivateInteractiveGameWindow,
                0.75f,
                false);
        }
        // PostLogin can arrive before AHUD has been created. Defer the first
        // presentation decision until the HUD exists so auto-start sessions
        // cannot remain on an unrendered/default screen.
        GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::InitializeLocalPresentation);
    }
}

void ABMPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
    ShutdownRemoteInputBridge();
#endif
    FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
    GetWorldTimerManager().ClearTimer(NativeFocusTimer);
    if (AmbientAudioComponent)
    {
        AmbientAudioComponent->Stop();
        AmbientAudioComponent->DestroyComponent();
        AmbientAudioComponent = nullptr;
    }
    if (MusicAudioComponent)
    {
        MusicAudioComponent->Stop();
        MusicAudioComponent->DestroyComponent();
        MusicAudioComponent = nullptr;
    }
    ProceduralMusicWave = nullptr;
    Super::EndPlay(EndPlayReason);
}

void ABMPlayerController::PlayerTick(const float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

#if !UE_BUILD_SHIPPING
    TickAudioProbe(DeltaTime);
    TickPerformanceProbe(DeltaTime);
#endif

    if (!IsLocalController() || IsPaused())
    {
        return;
    }

    // A focus handoff can leave the legacy controller ignore counters or the
    // visible-cursor state behind without dispatching another MouseX/MouseY
    // event. In that case the pawn's Turn()/LookUp() recovery cannot run
    // because the input is filtered before it reaches the pawn. Repair only
    // the active gameplay surfaces; death/results/menu states intentionally
    // keep look disabled and cinematic look locks are owned by the pawn.
    const ABMHUD* HUD = GetBlackMeridianHUD();
    if (!HUD || (HUD->GetScreen() != EBMHUDScreen::InGame && HUD->GetScreen() != EBMHUDScreen::Scoreboard))
    {
        return;
    }

    const bool bCrosshairProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CrosshairProbe"));
    if ((bShowMouseCursor || IsLookInputIgnored() || IsMoveInputIgnored()) && !bCrosshairProbe)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian input: recovered stale gameplay capture cursor=%s look_ignored=%s move_ignored=%s"),
            bShowMouseCursor ? TEXT("visible") : TEXT("hidden"),
            IsLookInputIgnored() ? TEXT("yes") : TEXT("no"),
            IsMoveInputIgnored() ? TEXT("yes") : TEXT("no"));
        RestoreInteractiveInput();
    }
}

#if !UE_BUILD_SHIPPING
void ABMPlayerController::InitializeRemoteInputBridge()
{
    if (!IsLocalController() || RemoteInputListenSocket)
    {
        return;
    }

    int32 RequestedPort = 0;
    if (!FParse::Value(FCommandLine::Get(), TEXT("BM_RemoteInputPort="), RequestedPort)
        || RequestedPort < 1024
        || RequestedPort > 65535)
    {
        return;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian remote input: socket subsystem unavailable"));
        return;
    }

    RemoteInputListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("BlackMeridianRemoteInput"), false);
    if (!RemoteInputListenSocket)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian remote input: could not create listener"));
        return;
    }

    TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->CreateInternetAddr();
    LocalAddress->SetIp(0x7F000001); // Loopback only; never expose the bridge on a LAN/Tailscale interface.
    LocalAddress->SetPort(RequestedPort);
    RemoteInputListenSocket->SetReuseAddr(true);
    RemoteInputListenSocket->SetNonBlocking(true);

    if (!RemoteInputListenSocket->Bind(*LocalAddress) || !RemoteInputListenSocket->Listen(1))
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian remote input: bind/listen failed port=%d"), RequestedPort);
        SocketSubsystem->DestroySocket(RemoteInputListenSocket);
        RemoteInputListenSocket = nullptr;
        return;
    }

    RemoteInputPort = RequestedPort;
    // PlayerTick is not guaranteed to run after SetPause(true) on every
    // packaged platform. Use the core ticker for the development-only bridge
    // so a paused/death screen can still receive the next command and resume
    // or restart; the listener remains loopback-only and shipping-disabled.
    RemoteInputTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ABMPlayerController::TickRemoteInputBridge));
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian remote input: READY loopback=127.0.0.1 port=%d shipping=no commands=KEY LOOK STATE PING CLOSE"),
        RemoteInputPort);
}

void ABMPlayerController::ShutdownRemoteInputBridge()
{
    if (RemoteInputTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(RemoteInputTickerHandle);
        RemoteInputTickerHandle.Reset();
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    DisconnectRemoteInputClient();
    if (RemoteInputListenSocket)
    {
        RemoteInputListenSocket->Close();
        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(RemoteInputListenSocket);
        }
        RemoteInputListenSocket = nullptr;
    }
    RemoteInputReceiveBuffer.Reset();
    RemoteInputPort = 0;
}

bool ABMPlayerController::TickRemoteInputBridge(float)
{
    if (!RemoteInputListenSocket)
    {
        return false;
    }

    PollRemoteInputBridge();
    return true;
}

void ABMPlayerController::DisconnectRemoteInputClient()
{
    ReleaseRemoteInputKeys();
    if (!RemoteInputClientSocket)
    {
        return;
    }

    RemoteInputClientSocket->Close();
    if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
    {
        SocketSubsystem->DestroySocket(RemoteInputClientSocket);
    }
    RemoteInputClientSocket = nullptr;
    RemoteInputReceiveBuffer.Reset();
}

void ABMPlayerController::ReleaseRemoteInputKeys()
{
    for (const FKey& Key : RemoteInputPressedKeys)
    {
        InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 1.0f));
    }
    RemoteInputPressedKeys.Reset();
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        // A disconnected remote client may disappear between its mouse-down
        // and mouse-up packets. Do not leave the local automatic weapon in a
        // firing state just because the bridge had to synthesize key releases.
        Player->ResetFireInputState();
    }
}

void ABMPlayerController::SendRemoteInputResponse(const FString& Response)
{
    if (!RemoteInputClientSocket)
    {
        return;
    }

    const FString Line = Response + TEXT("\n");
    FTCHARToUTF8 Utf8Line(*Line);
    int32 BytesSent = 0;
    if (!RemoteInputClientSocket->Send(
            reinterpret_cast<const uint8*>(Utf8Line.Get()),
            Utf8Line.Length(),
            BytesSent))
    {
        DisconnectRemoteInputClient();
    }
}

void ABMPlayerController::ProcessRemoteInputCommand(const FString& CommandLine)
{
    TArray<FString> Tokens;
    CommandLine.ParseIntoArrayWS(Tokens);
    if (Tokens.Num() == 0)
    {
        return;
    }

    const FString Command = Tokens[0].ToUpper();
    if (Command == TEXT("PING"))
    {
        SendRemoteInputResponse(FString::Printf(TEXT("PONG port=%d"), RemoteInputPort));
        return;
    }

    if (Command == TEXT("HELP"))
    {
        SendRemoteInputResponse(TEXT("HELP KEY <name> <DOWN|UP> | LOOK <yaw> <pitch> | STATE | PING | CLOSE"));
        return;
    }

    if (Command == TEXT("CLOSE"))
    {
        SendRemoteInputResponse(TEXT("OK CLOSE"));
        ShutdownRemoteInputBridge();
        return;
    }

    if (Command == TEXT("STATE"))
    {
        const APawn* Pawn = GetPawn();
        const ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Pawn);
        const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
        const FRotator Rotation = GetControlRotation();
        const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.0f;
        const UBMHealthComponent* Health = Player ? Player->HealthComponent : nullptr;
        const ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;
        const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        const FString ObjectiveId = GameState
            ? GameState->CurrentObjective.ObjectiveId.ToString()
            : TEXT("None");
        const ABMHUD* HUD = GetBlackMeridianHUD();
        SendRemoteInputResponse(FString::Printf(
            TEXT("STATE pawn=%s x=%.2f y=%.2f z=%.2f yaw=%.2f pitch=%.2f speed=%.2f screen=%d paused=%s health=%.1f eliminated=%s weapon=%d magazine=%d reserve=%d reloading=%s phase=%d mission_elapsed=%.2f hostile=%d objective=%s"),
            *GetNameSafe(Pawn),
            Location.X,
            Location.Y,
            Location.Z,
            Rotation.Yaw,
            Rotation.Pitch,
            Speed,
            HUD ? static_cast<int32>(HUD->GetScreen()) : -1,
            IsPaused() ? TEXT("yes") : TEXT("no"),
            Health ? Health->Health : -1.0f,
            Player && Player->IsEliminated() ? TEXT("yes") : TEXT("no"),
            Weapon ? static_cast<int32>(Weapon->GetWeaponId()) : -1,
            Weapon ? Weapon->GetMagazineAmmo() : -1,
            Weapon ? Weapon->GetReserveAmmo() : -1,
            Weapon && Weapon->IsReloading() ? TEXT("yes") : TEXT("no"),
            GameState ? static_cast<int32>(GameState->MissionPhase) : -1,
            GameState ? GameState->MissionElapsedSeconds : 0.0f,
            GameState ? GameState->HostileCount : 0,
            *ObjectiveId));
        return;
    }

    if (Command == TEXT("LOOK") && Tokens.Num() >= 3)
    {
        const float YawDelta = FCString::Atof(*Tokens[1]);
        const float PitchDelta = FCString::Atof(*Tokens[2]);
        if (!FMath::IsFinite(YawDelta) || !FMath::IsFinite(PitchDelta)
            || FMath::Abs(YawDelta) > 90.0f || FMath::Abs(PitchDelta) > 90.0f)
        {
            SendRemoteInputResponse(TEXT("ERR LOOK range"));
            return;
        }

        // The development bridge expresses LOOK in degrees, while
        // AddYawInput/AddPitchInput apply the controller's configured axis
        // scales.  Feeding those methods directly made a small remote pitch
        // command become a much larger camera jump and bypassed the pawn's
        // gameplay pitch clamp.  That left the rendered test camera at
        // +/-89.9 degrees and made a manual aim result untrustworthy.  Apply
        // the same bounded view state that a real player is allowed to use,
        // then mirror it onto the camera immediately for the next fire/input
        // command.
        const FRotator CurrentRotation = GetControlRotation();
        FRotator NewRotation = CurrentRotation;
        NewRotation.Yaw = FMath::UnwindDegrees(CurrentRotation.Yaw + YawDelta);
        NewRotation.Pitch = FMath::Clamp(
            FMath::UnwindDegrees(CurrentRotation.Pitch) + PitchDelta,
            -82.0f,
            82.0f);
        NewRotation.Roll = 0.0f;
        SetControlRotation(NewRotation);
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn());
            Player && Player->GetFirstPersonCamera())
        {
            Player->GetFirstPersonCamera()->SetWorldRotation(NewRotation);
        }
        SendRemoteInputResponse(FString::Printf(
            TEXT("OK LOOK yaw=%.2f pitch=%.2f resulting_yaw=%.2f resulting_pitch=%.2f"),
            YawDelta,
            PitchDelta,
            NewRotation.Yaw,
            NewRotation.Pitch));
        return;
    }

    if (Command == TEXT("KEY") && Tokens.Num() >= 3)
    {
        const FString KeyName = Tokens[1].ToUpper();
        const FString EventName = Tokens[2].ToUpper();
        const FKey Key{FName(*KeyName)};
        if (!Key.IsValid())
        {
            SendRemoteInputResponse(FString::Printf(TEXT("ERR KEY unknown=%s"), *KeyName));
            return;
        }

        EInputEvent Event = IE_Repeat;
        if (EventName == TEXT("DOWN") || EventName == TEXT("PRESS"))
        {
            Event = IE_Pressed;
        }
        else if (EventName == TEXT("UP") || EventName == TEXT("RELEASE"))
        {
            Event = IE_Released;
        }
        else if (EventName != TEXT("REPEAT"))
        {
            SendRemoteInputResponse(TEXT("ERR KEY event=DOWN|UP|REPEAT"));
            return;
        }

        const bool bHandled = InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, 1.0f));
        if (Event == IE_Pressed)
        {
            RemoteInputPressedKeys.Add(Key);
        }
        else if (Event == IE_Released)
        {
            RemoteInputPressedKeys.Remove(Key);
        }
        SendRemoteInputResponse(FString::Printf(
            TEXT("OK KEY name=%s event=%s handled=%s"),
            *KeyName,
            *EventName,
            bHandled ? TEXT("yes") : TEXT("no")));
        return;
    }

    SendRemoteInputResponse(TEXT("ERR command; use HELP"));
}

void ABMPlayerController::PollRemoteInputBridge()
{
    if (!IsLocalController() || !RemoteInputListenSocket)
    {
        return;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    if (!RemoteInputClientSocket)
    {
        bool bPendingConnection = false;
        if (RemoteInputListenSocket->HasPendingConnection(bPendingConnection) && bPendingConnection)
        {
            TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
            RemoteInputClientSocket = RemoteInputListenSocket->Accept(*RemoteAddress, TEXT("BlackMeridianRemoteInputClient"));
            if (RemoteInputClientSocket)
            {
                RemoteInputClientSocket->SetNonBlocking(true);
                RemoteInputClientSocket->SetNoDelay(true);
                RemoteInputReceiveBuffer.Reset();
                SendRemoteInputResponse(FString::Printf(TEXT("READY port=%d"), RemoteInputPort));
                UE_LOG(LogTemp, Display, TEXT("Black Meridian remote input: client connected address=%s"), *RemoteAddress->ToString(true));
            }
        }
    }

    if (!RemoteInputClientSocket)
    {
        return;
    }

    // A remote client can disappear without sending the development-only
    // CLOSE command (for example when an SSH session is interrupted).  In
    // that case HasPendingData() may stay false while the accepted socket is
    // already unusable, leaving the single-client bridge stuck in CLOSE_WAIT
    // and preventing the next playtest client from connecting.  Poll the
    // socket state before reading input so the listener can accept a fresh
    // client without requiring a game restart.
    const ESocketConnectionState ConnectionState = RemoteInputClientSocket->GetConnectionState();
    if (ConnectionState != SCS_Connected)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian remote input: client disconnected state=%d; releasing bridge slot"),
            static_cast<int32>(ConnectionState));
        DisconnectRemoteInputClient();
        return;
    }

    uint32 PendingDataSize = 0;
    while (RemoteInputClientSocket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
    {
        const int32 ReadSize = static_cast<int32>(FMath::Min(PendingDataSize, 8192u));
        TArray<uint8> Bytes;
        Bytes.SetNumUninitialized(ReadSize);
        int32 BytesRead = 0;
        if (!RemoteInputClientSocket->Recv(Bytes.GetData(), Bytes.Num(), BytesRead, ESocketReceiveFlags::None)
            || BytesRead <= 0)
        {
            DisconnectRemoteInputClient();
            return;
        }

        FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), BytesRead);
        RemoteInputReceiveBuffer.Append(Utf8.Get(), Utf8.Length());
        if (RemoteInputReceiveBuffer.Len() > 32768)
        {
            SendRemoteInputResponse(TEXT("ERR input buffer exceeded"));
            RemoteInputReceiveBuffer.Reset();
            break;
        }
    }

    int32 NewlineIndex = INDEX_NONE;
    while (RemoteInputReceiveBuffer.FindChar(TEXT('\n'), NewlineIndex))
    {
        FString CommandLine = RemoteInputReceiveBuffer.Left(NewlineIndex);
        RemoteInputReceiveBuffer.RightChopInline(NewlineIndex + 1, EAllowShrinking::No);
        CommandLine.TrimStartAndEndInline();
        if (!CommandLine.IsEmpty())
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian remote input: command=%s"), *CommandLine);
            ProcessRemoteInputCommand(CommandLine);
            if (!RemoteInputClientSocket)
            {
                return;
            }
        }
    }
}

void ABMPlayerController::TickAudioProbe(const float DeltaTime)
{
    if (!IsLocalController()
        || !FParse::Param(FCommandLine::Get(), TEXT("BM_AudioProbe"))
        || DeltaTime <= 0.0f)
    {
        return;
    }

    AudioProbeElapsedSeconds += DeltaTime;
    if (AudioProbeElapsedSeconds < 1.0f)
    {
        return;
    }
    AudioProbeElapsedSeconds = 0.0f;

    const USoundWave* AuthoredWave = MusicAudioComponent
        ? Cast<USoundWave>(MusicAudioComponent->GetSound())
        : nullptr;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian audio probe: music heartbeat state=%s playing=%s component=%s loop=%s"),
        *ActiveMusicState.ToString(),
        MusicAudioComponent && MusicAudioComponent->IsPlaying() ? TEXT("yes") : TEXT("no"),
        MusicAudioComponent ? TEXT("yes") : TEXT("no"),
        AuthoredWave && AuthoredWave->bLooping ? TEXT("yes") : TEXT("no"));
}

void ABMPlayerController::TickPerformanceProbe(const float DeltaTime)
{
    if (!IsLocalController()
        || bPerformanceProbeFinished
        || !FParse::Param(FCommandLine::Get(), TEXT("BM_PerformanceProbe")))
    {
        return;
    }

    const ABMHUD* HUD = GetBlackMeridianHUD();
    if (!HUD
        || (HUD->GetScreen() != EBMHUDScreen::InGame && HUD->GetScreen() != EBMHUDScreen::Scoreboard)
        || IsPaused()
        || DeltaTime <= 0.0f
        || DeltaTime > 1.0f)
    {
        return;
    }

    if (!bPerformanceProbeActive)
    {
        float RequestedDuration = 30.0f;
        FParse::Value(FCommandLine::Get(), TEXT("BM_PerformanceProbeSeconds="), RequestedDuration);
        PerformanceProbeDurationSeconds = FMath::Clamp(RequestedDuration, 5.0f, 600.0f);
        PerformanceProbeElapsedSeconds = 0.0f;
        PerformanceFrameTimesMilliseconds.Reset();
        PerformanceFrameTimesMilliseconds.Reserve(FMath::CeilToInt(PerformanceProbeDurationSeconds * 120.0f));
        bPerformanceProbeActive = true;

        int32 ViewportX = 0;
        int32 ViewportY = 0;
        GetViewportSize(ViewportX, ViewportY);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian performance probe: started duration_s=%.1f viewport=%dx%d"),
            PerformanceProbeDurationSeconds,
            ViewportX,
            ViewportY);
    }

    PerformanceProbeElapsedSeconds += DeltaTime;
    PerformanceFrameTimesMilliseconds.Add(DeltaTime * 1000.0f);
    if (PerformanceProbeElapsedSeconds < PerformanceProbeDurationSeconds)
    {
        return;
    }

    bPerformanceProbeFinished = true;
    bPerformanceProbeActive = false;
    if (PerformanceFrameTimesMilliseconds.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Black Meridian performance probe: no rendered samples collected"));
        return;
    }

    TArray<float> SortedFrameTimes = PerformanceFrameTimesMilliseconds;
    SortedFrameTimes.Sort();
    double TotalMilliseconds = 0.0;
    for (const float FrameTime : PerformanceFrameTimesMilliseconds)
    {
        TotalMilliseconds += FrameTime;
    }
    const float AverageMilliseconds = static_cast<float>(TotalMilliseconds / PerformanceFrameTimesMilliseconds.Num());
    const int32 P95Index = FMath::Clamp(
        FMath::CeilToInt(static_cast<float>(SortedFrameTimes.Num() - 1) * 0.95f),
        0,
        SortedFrameTimes.Num() - 1);
    const float P95Milliseconds = SortedFrameTimes[P95Index];
    const float AverageFPS = 1000.0f / FMath::Max(0.01f, AverageMilliseconds);
    const float P95FPS = 1000.0f / FMath::Max(0.01f, P95Milliseconds);
    const bool bWithin60FPSFrameBudget = P95Milliseconds <= (1000.0f / 60.0f);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian performance probe: complete duration_s=%.2f frames=%d avg_fps=%.2f p95_frame_ms=%.2f p95_fps=%.2f max_frame_ms=%.2f target_60fps_p95=%s"),
        PerformanceProbeElapsedSeconds,
        PerformanceFrameTimesMilliseconds.Num(),
        AverageFPS,
        P95Milliseconds,
        P95FPS,
        SortedFrameTimes.Last(),
        bWithin60FPSFrameBudget ? TEXT("yes") : TEXT("no"));

    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PerformanceProbeExit")))
    {
        FGenericPlatformMisc::RequestExit(false);
    }
}
#endif

void ABMPlayerController::HandleInitialApplicationReactivation()
{
    if (!IsLocalController() || IsSignalControlWorld())
    {
        return;
    }

    ABMHUD* HUD = GetBlackMeridianHUD();
    if (!HUD || HUD->GetScreen() != EBMHUDScreen::InGame)
    {
        return;
    }

    // The first focus regain can deliver the OS cursor reposition as mouse
    // input. Reapply the mission-facing view and guard that one transition.
    // Later focus changes must preserve the player's current heading rather
    // than snapping them back to the opening view, but still need the normal
    // game-only capture and look flags restored.
    if (!bHandledInitialApplicationReactivation)
    {
        bHandledInitialApplicationReactivation = true;
        FocusMissionStart();
        return;
    }

    RestoreInteractiveInput();
    GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::RestoreInteractiveInput);
}

void ABMPlayerController::InitializeLocalPresentation()
{
    if (!IsLocalController())
    {
        return;
    }

    ABMHUD* HUD = GetBlackMeridianHUD();
    if (!HUD)
    {
        GetWorldTimerManager().SetTimer(
            PresentationRetryTimer,
            this,
            &ABMPlayerController::InitializeLocalPresentation,
            0.05f,
            false);
        return;
    }

#if !UE_BUILD_SHIPPING
    const bool bAutoStartSignalMatch = FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartSignalMatch"));
#else
    constexpr bool bAutoStartSignalMatch = false;
#endif

    const bool bSignalWorld = bSignalControlLobby ||
        (GetWorld() && GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>() != nullptr);
    if (bSignalWorld)
    {
        // The first listen-server pawn can be created from Entry before the
        // signal game mode has spawned its runtime PlayerStarts. Correct the
        // local view once the replicated signal state and HUD are available,
        // so a new match always opens on the arena rather than the empty map
        // horizon.
        if (ABMPlayerCharacter* LocalPlayer = Cast<ABMPlayerCharacter>(GetPawn()))
        {
            const FVector ArenaCenter(650.0f, 0.0f, 72.0f);
            const float ArenaYaw = (ArenaCenter - LocalPlayer->GetActorLocation()).Rotation().Yaw;
            SetControlRotation(FRotator(0.0f, ArenaYaw, 0.0f));
            LocalPlayer->SetActorRotation(FRotator(0.0f, ArenaYaw, 0.0f));
        }
        HUD->SetScreen(bAutoStartSignalMatch ? EBMHUDScreen::InGame : EBMHUDScreen::Lobby);
        SetPause(!bAutoStartSignalMatch);
        if (bAutoStartSignalMatch)
        {
            RestoreInteractiveInput();
        }
        PlayMusicState(TEXT("SignalControl"));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UI: initialized signal screen=%d auto_start=%s hud=%s pawn=%s"),
            static_cast<int32>(HUD->GetScreen()),
            bAutoStartSignalMatch ? TEXT("yes") : TEXT("no"),
            *HUD->GetName(),
            GetPawn() ? *GetPawn()->GetName() : TEXT("none"));
        return;
    }

#if !UE_BUILD_SHIPPING
    // Development capture/probe sessions already have their mission started
    // by the authority. Do not create the normal splash timer in that path:
    // the delayed callback can race the showcase spawn and reintroduce the
    // menu pause after the combat actors have begun ticking.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartMission"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
    {
        HUD->SetScreen(EBMHUDScreen::InGame);
        SetPause(false);
        RestoreInteractiveInput();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UI: development mission presentation unpaused world_paused=%s"),
            GetWorld() && GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"));
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableProbe")))
    {
        // The development throwable probe must exercise the live timer and
        // fuse path, not wait behind the normal splash/menu pause.
        HUD->SetScreen(EBMHUDScreen::InGame);
        SetPause(false);
        RestoreInteractiveInput();
        UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable probe: local presentation unpaused"));
        return;
    }
#endif

    // HUD creation can finish after the window is already interactive. If a
    // player has selected Campaign, started the mission, or opened another
    // screen in that gap, a late splash initialization must not overwrite the
    // state they are already using.
    const EBMHUDScreen CurrentScreen = HUD->GetScreen();
    if (CurrentScreen != EBMHUDScreen::MainMenu && CurrentScreen != EBMHUDScreen::Splash)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UI: skipped late splash initialization screen=%d"),
            static_cast<int32>(CurrentScreen));
        return;
    }

    HUD->SetScreen(EBMHUDScreen::Splash);
    FTimerHandle SplashTimer;
    GetWorldTimerManager().SetTimer(SplashTimer, this, &ABMPlayerController::FinishSplash, 1.5f, false);
    UE_LOG(LogTemp, Display, TEXT("Black Meridian UI: initialized splash"));
}

void ABMPlayerController::FinishSplash()
{
    if (bSignalControlLobby)
    {
#if !UE_BUILD_SHIPPING
        const bool bAutoStartSignalMatch = FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartSignalMatch"));
#else
        constexpr bool bAutoStartSignalMatch = false;
#endif
        if (ABMHUD* HUD = GetBlackMeridianHUD())
        {
            HUD->SetScreen(bAutoStartSignalMatch ? EBMHUDScreen::InGame : EBMHUDScreen::Lobby);
        }
        SetPause(!bAutoStartSignalMatch);
        if (bAutoStartSignalMatch)
        {
            RestoreInteractiveInput();
        }
        return;
    }
    if (ABMHUD* SplashHUD = GetBlackMeridianHUD())
    {
        // A player may have advanced from the splash/menu while the delayed
        // callback was queued. Never reset a live mission or another menu to
        // MainMenu from this stale timer.
        if (SplashHUD->GetScreen() != EBMHUDScreen::Splash)
        {
            if (SplashHUD->GetScreen() == EBMHUDScreen::InGame)
            {
                // The normal menu and a development auto-start can cross this
                // delayed callback. Keep the already-live mission playable
                // instead of leaving the old menu capture state behind.
                SetPause(false);
                RestoreInteractiveInput();
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian UI: ignored stale splash completion screen=%d"),
                static_cast<int32>(SplashHUD->GetScreen()));
            return;
        }

        SplashHUD->SetScreen(EBMHUDScreen::MainMenu);
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartMission")))
        {
            SplashHUD->SetScreen(EBMHUDScreen::InGame);
            SetPause(false);
            RestoreInteractiveInput();
            GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::FocusMissionStart);
            return;
        }
#endif
    }
    SetPause(true);
}

void ABMPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (InputComponent)
    {
        InputComponent->BindAction(TEXT("StartMission"), IE_Pressed, this, &ABMPlayerController::StartMissionInput);
        InputComponent->BindAction(TEXT("TogglePause"), IE_Pressed, this, &ABMPlayerController::TogglePauseInput);
        InputComponent->BindAction(TEXT("OpenSettings"), IE_Pressed, this, &ABMPlayerController::OpenSettingsInput);
        InputComponent->BindAction(TEXT("OpenControls"), IE_Pressed, this, &ABMPlayerController::OpenControlsInput);
        // Keep both controls shortcuts as direct bindings as well as action
        // mappings. Some packaged input configurations can load the legacy
        // mappings without rebuilding newly-added aliases. The handler is
        // idempotent because a key can arrive through both paths in one
        // frame.
        FInputKeyBinding& ControlsF2Binding =
            InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ABMPlayerController::OpenControlsInput);
        ControlsF2Binding.bExecuteWhenPaused = true;
        FInputKeyBinding& ControlsFallbackBinding =
            InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ABMPlayerController::OpenControlsInput);
        ControlsFallbackBinding.bExecuteWhenPaused = true;
        InputComponent->BindAction(TEXT("OpenHowToPlay"), IE_Pressed, this, &ABMPlayerController::OpenHowToPlayInput);
        InputComponent->BindAction(TEXT("OpenCampaign"), IE_Pressed, this, &ABMPlayerController::OpenCampaignInput);
        InputComponent->BindAction(TEXT("OpenMultiplayer"), IE_Pressed, this, &ABMPlayerController::OpenMultiplayerInput);
        InputComponent->BindAction(TEXT("SettingsPrevious"), IE_Pressed, this, &ABMPlayerController::SettingsPreviousInput);
        InputComponent->BindAction(TEXT("SettingsNext"), IE_Pressed, this, &ABMPlayerController::SettingsNextInput);
        InputComponent->BindAction(TEXT("SettingsLeft"), IE_Pressed, this, &ABMPlayerController::AdjustSettingsLeftInput);
        InputComponent->BindAction(TEXT("SettingsRight"), IE_Pressed, this, &ABMPlayerController::AdjustSettingsRightInput);
        InputComponent->BindAction(TEXT("RestartCheckpoint"), IE_Pressed, this, &ABMPlayerController::RestartCheckpointInput);
        InputComponent->BindAction(TEXT("Scoreboard"), IE_Pressed, this, &ABMPlayerController::ToggleScoreboardInput);
        InputComponent->BindAction(TEXT("Back"), IE_Pressed, this, &ABMPlayerController::BackInput);
        for (int32 BindingIndex = 0; BindingIndex < InputComponent->GetNumActionBindings(); ++BindingIndex)
        {
            InputComponent->GetActionBinding(BindingIndex).bExecuteWhenPaused = true;
        }
    }
}

void ABMPlayerController::StartMissionInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        if (HUD->GetScreen() == EBMHUDScreen::MainMenu)
        {
            // On a controller, A confirms the highlighted runtime menu
            // action.  Keep the keyboard Enter path intuitive by defaulting
            // the first action to Campaign when no native widget exists.
            if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget(); NativeUI && NativeUI->ActivateMenuSelection())
            {
                return;
            }
            if (IsSignalControlWorld())
            {
                RequestMissionTravel();
            }
            else
            {
                HUD->SetScreen(EBMHUDScreen::CampaignSelection);
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::CampaignSelection || HUD->GetScreen() == EBMHUDScreen::Pause)
        {
            if (HUD->GetScreen() == EBMHUDScreen::Pause)
            {
                if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget(); NativeUI && NativeUI->ActivateMenuSelection())
                {
                    return;
                }
            }
            const bool bStartingMission = HUD->GetScreen() == EBMHUDScreen::CampaignSelection;
            EBMCampaignLevel RequestedLevel = SelectedCampaignLevel;
            if (!bStartingMission)
            {
                if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
                {
                    RequestedLevel = State->CampaignLevel;
                }
            }
            HUD->SetScreen(EBMHUDScreen::InGame);
            SetPause(false);
            RestoreInteractiveInput();
            if (bStartingMission)
            {
                GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::FocusMissionStart);
            }
            if (HasAuthority())
            {
                if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
                {
                    GameMode->RequestMissionStart(RequestedLevel);
                }
            }
            else
            {
                ServerRequestMissionStart(RequestedLevel);
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::MultiplayerMenu)
        {
            HUD->SetScreen(EBMHUDScreen::Lobby);
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Lobby)
        {
            HUD->SetScreen(EBMHUDScreen::InGame);
            SetPause(false);
            RestoreInteractiveInput();
            if (HasAuthority())
            {
                if (ABMSignalControlGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>())
                {
                    GameMode->RequestMatchStart();
                }
            }
            else
            {
                ServerRequestMatchStart();
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Death)
        {
            RestartCheckpointInput();
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Results)
        {
            // Results is a real confirmation screen. The native widget starts
            // with NEXT LEVEL focused, but Enter/A must still honor a user
            // moving focus to the visible campaign-menu alternative.
            if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget(); NativeUI && NativeUI->ActivateMenuSelection())
            {
                return;
            }
            UIStartNextCampaignLevel();
        }
    }
}

void ABMPlayerController::TogglePauseInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        if (HUD->GetScreen() == EBMHUDScreen::InGame)
        {
            if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
            {
                Player->ResetFireInputState();
            }
            HUD->SetScreen(EBMHUDScreen::Pause);
            SetPause(true);
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Pause)
        {
            HUD->SetScreen(EBMHUDScreen::InGame);
            SetPause(false);
            RestoreInteractiveInput();
        }
    }
}

void ABMPlayerController::FocusMissionStart()
{
    if (!IsLocalController())
    {
        return;
    }

    ABMPlayerCharacter* LocalPlayer = Cast<ABMPlayerCharacter>(GetPawn());
    if (!LocalPlayer)
    {
        return;
    }

    // The pawn can begin play before the controller owns the mouse-captured
    // viewport. Apply the same mission-facing direction one frame after the
    // mission becomes interactive so the first objective is on screen.
    const FVector LookTarget(250.0f, -500.0f, 72.0f);
    const FRotator LookRotation = (LookTarget - LocalPlayer->GetActorLocation()).Rotation();
    const FRotator StartRotation(-4.0f, LookRotation.Yaw, 0.0f);
    SetControlRotation(StartRotation);
    LocalPlayer->SetActorRotation(FRotator(0.0f, LookRotation.Yaw, 0.0f));
    RestoreInteractiveInput();
    LocalPlayer->BeginMissionLookGuard();
}

void ABMPlayerController::RestoreInteractiveInput()
{
    if (!IsLocalController())
    {
        return;
    }

    // A viewport can regain OS focus while retaining the old UI input mode or
    // a stale ignore-look flag. Reapply the same state as a fresh mission
    // without changing the player's current control rotation.
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());

    // SetInputMode only repairs Unreal's routing flags. On macOS a packaged
    // game launched from another Space can still leave the browser/terminal
    // as the native key window, so G/R/etc. never reaches Unreal at all.
    // Queue native activation after the launch/menu transition has settled;
    // unattended/null-renderer probes must remain headless and must not steal
    // the operator's desktop focus.
    if (!FParse::Param(FCommandLine::Get(), TEXT("unattended"))
        && FSlateApplication::IsInitialized())
    {
        GetWorldTimerManager().SetTimer(
            NativeFocusTimer,
            this,
            &ABMPlayerController::ActivateInteractiveGameWindow,
            0.75f,
            false);
    }

    // These are counters in AController, so decrementing once is not enough
    // if a pause/menu/cinematic path incremented them more than once.
    ResetIgnoreLookInput();
    ResetIgnoreMoveInput();

    // Let Unreal's game-only viewport capture own the OS cursor. Manually
    // warping it here caused a second, delayed MouseX/MouseY event on macOS;
    // that event could pitch the player into the floor after the guard had
    // expired. The pawn still filters implausible deltas and clamps pitch.
    if (ABMPlayerCharacter* LocalPlayer = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        LocalPlayer->ResetFireInputState();
        LocalPlayer->BeginMissionLookGuard(0.5f);
    }

    UE_LOG(LogTemp, Display, TEXT("Black Meridian input: restored interactive game capture"));
}

void ABMPlayerController::ActivateInteractiveGameWindow()
{
    if (!IsLocalController()
        || FParse::Param(FCommandLine::Get(), TEXT("unattended"))
        || !FSlateApplication::IsInitialized())
    {
        return;
    }

    const bool bForegroundBefore = FPlatformApplicationMisc::IsThisApplicationForeground();
    FPlatformApplicationMisc::ActivateApplication();
    if (const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow())
    {
        ActiveWindow->BringToFront(true);
    }
    FSlateApplication::Get().ActivateGameViewport();
    FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian input: native game window activated deferred foreground_before=%s"),
        bForegroundBefore ? TEXT("yes") : TEXT("no"));
}

void ABMPlayerController::OpenSettingsInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        const EBMHUDScreen CurrentScreen = HUD->GetScreen();
        if (CurrentScreen != EBMHUDScreen::Settings)
        {
            SettingsReturnScreen = CurrentScreen;
        }
        SettingsRow = 0;
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
        {
            Player->ResetFireInputState();
        }
        HUD->SetScreen(EBMHUDScreen::Settings);
        SetPause(true);
    }
}

void ABMPlayerController::OpenControlsInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        const EBMHUDScreen CurrentScreen = HUD->GetScreen();
        // OpenControls is present in both the legacy action map and the
        // direct packaged-build bindings. A single physical key press can
        // therefore call this function twice; do not overwrite the screen
        // we must return to on the second call.
        if (CurrentScreen == EBMHUDScreen::Controls)
        {
            return;
        }
        ControlsReturnScreen = CurrentScreen;
        ControlsRow = 0;
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
        {
            Player->ResetFireInputState();
        }
        HUD->SetScreen(EBMHUDScreen::Controls);
        SetPause(true);
        UE_LOG(LogTemp, Display, TEXT("Black Meridian UI: controls screen opened by keyboard/controller input"));
    }
}

void ABMPlayerController::OpenHowToPlayInput()
{
#if !UE_BUILD_SHIPPING
    // Combat showcase captures are unattended gameplay sessions. A stray
    // keyboard event from the host must not replace the live combat surface
    // with the briefing overlay while the probe is exercising reload, aim,
    // and enemy presentation.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
    {
        return;
    }
#endif
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        if (HUD->GetScreen() == EBMHUDScreen::HowToPlay)
        {
            HUD->SetScreen(HowToPlayReturnScreen);
            SetPause(HowToPlayReturnScreen != EBMHUDScreen::InGame);
            return;
        }

        const EBMHUDScreen CurrentScreen = HUD->GetScreen();
        if (CurrentScreen == EBMHUDScreen::Splash || CurrentScreen == EBMHUDScreen::Loading ||
            CurrentScreen == EBMHUDScreen::Settings || CurrentScreen == EBMHUDScreen::Controls ||
            CurrentScreen == EBMHUDScreen::Scoreboard || CurrentScreen == EBMHUDScreen::Death ||
            CurrentScreen == EBMHUDScreen::Results)
        {
            return;
        }

        HowToPlayReturnScreen = CurrentScreen;
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
        {
            Player->ResetFireInputState();
        }
        HUD->SetScreen(EBMHUDScreen::HowToPlay);
        SetPause(true);
    }
}

void ABMPlayerController::OpenCampaignInput()
{
    if (IsSignalControlWorld())
    {
        RequestMissionTravel();
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
        {
            Player->ResetFireInputState();
        }
        if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
        {
            SelectedCampaignLevel = State->CampaignLevel;
        }
        HUD->SetScreen(EBMHUDScreen::CampaignSelection);
    }
}

void ABMPlayerController::OpenMultiplayerInput()
{
    if (!IsSignalControlWorld())
    {
        RequestSignalControlTravel();
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::MultiplayerMenu);
    }
}

void ABMPlayerController::SettingsPreviousInput()
{
    ABMHUD* HUD = GetBlackMeridianHUD();
    if (HUD && IsNativeMenuSelectionScreen(HUD->GetScreen()))
    {
        if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget())
        {
            NativeUI->MoveMenuSelection(-1);
        }
    }
    else if (HUD && HUD->GetScreen() == EBMHUDScreen::CampaignSelection)
    {
        CycleCampaignLevel(-1);
    }
    else if (HUD && HUD->GetScreen() == EBMHUDScreen::Controls)
    {
        CycleControlsRow(-1);
    }
    else
    {
        CycleSettingsRow(-1);
    }
}

void ABMPlayerController::SettingsNextInput()
{
    ABMHUD* HUD = GetBlackMeridianHUD();
    if (HUD && IsNativeMenuSelectionScreen(HUD->GetScreen()))
    {
        if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget())
        {
            NativeUI->MoveMenuSelection(1);
        }
    }
    else if (HUD && HUD->GetScreen() == EBMHUDScreen::CampaignSelection)
    {
        CycleCampaignLevel(1);
    }
    else if (HUD && HUD->GetScreen() == EBMHUDScreen::Controls)
    {
        CycleControlsRow(1);
    }
    else
    {
        CycleSettingsRow(1);
    }
}

void ABMPlayerController::AdjustSettingsLeftInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && IsNativeMenuSelectionScreen(HUD->GetScreen()))
    {
        if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget())
        {
            NativeUI->MoveMenuSelection(-1);
        }
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && HUD->GetScreen() == EBMHUDScreen::CampaignSelection)
    {
        CycleCampaignLevel(-1);
        return;
    }
    AdjustSettings(-1);
}

void ABMPlayerController::AdjustSettingsRightInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && IsNativeMenuSelectionScreen(HUD->GetScreen()))
    {
        if (UBMNativeUIWidget* NativeUI = HUD->GetNativeUIWidget())
        {
            NativeUI->MoveMenuSelection(1);
        }
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && HUD->GetScreen() == EBMHUDScreen::CampaignSelection)
    {
        CycleCampaignLevel(1);
        return;
    }
    AdjustSettings(1);
}

void ABMPlayerController::CycleSettingsRow(const int32 Direction)
{
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && HUD->GetScreen() == EBMHUDScreen::Settings)
    {
        constexpr int32 SettingCount = 19;
        SettingsRow = (SettingsRow + Direction + SettingCount) % SettingCount;
    }
}

void ABMPlayerController::CycleControlsRow(const int32 Direction)
{
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && HUD->GetScreen() == EBMHUDScreen::Controls)
    {
        const int32 ControlCount = GetRemappableActions().Num();
        ControlsRow = (ControlsRow + Direction + ControlCount) % ControlCount;
    }
}

void ABMPlayerController::CycleCampaignLevel(const int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }

    if (ABMHUD* HUD = GetBlackMeridianHUD(); !HUD || HUD->GetScreen() != EBMHUDScreen::CampaignSelection)
    {
        return;
    }

    const int32 CampaignLevelCount = GetBMCampaignLevelCount();
    const int32 CurrentIndex = static_cast<int32>(SelectedCampaignLevel);
    const int32 NextIndex = (CurrentIndex + Direction + CampaignLevelCount) % CampaignLevelCount;
    SelectedCampaignLevel = static_cast<EBMCampaignLevel>(NextIndex);

    const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(SelectedCampaignLevel);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign UI: selected level=%s index=%d/%d"),
        *Definition.LevelId.ToString(),
        NextIndex + 1,
        CampaignLevelCount);
}

FString ABMPlayerController::GetControlBindingLabel(const int32 Row) const
{
    const TArray<FName>& Actions = GetRemappableActions();
    const TArray<FString>& Labels = GetRemappableLabels();
    if (!Actions.IsValidIndex(Row) || !Labels.IsValidIndex(Row))
    {
        return TEXT("UNBOUND");
    }

    const FKey CurrentKey = GetCurrentKeyboardBinding(Actions[Row]);
    const FString KeyLabel = CurrentKey.IsValid() ? CurrentKey.GetDisplayName().ToString() : TEXT("UNBOUND");
    return FString::Printf(TEXT("%-18s  %s"), *Labels[Row], *KeyLabel);
}

void ABMPlayerController::BM_CheatGiveAmmo()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        return;
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->AddReserveAmmo(120);
    }
#endif
}

void ABMPlayerController::BM_CheatRestore()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        return;
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->RestoreForCheckpoint(100.0f);
        Player->RestoreThrowableInventory(2, 1);
    }
#endif
}

void ABMPlayerController::AdjustControlBinding(const int32 Direction)
{
    if (Direction == 0 || !GetRemappableActions().IsValidIndex(ControlsRow))
    {
        return;
    }

    const TArray<TArray<FKey>>& Presets = GetRemappableKeyPresets();
    if (!Presets.IsValidIndex(ControlsRow) || Presets[ControlsRow].Num() == 0)
    {
        return;
    }

    const FName ActionName = GetRemappableActions()[ControlsRow];
    const FKey CurrentKey = GetCurrentKeyboardBinding(ActionName);
    int32 PresetIndex = Presets[ControlsRow].IndexOfByKey(CurrentKey);
    if (PresetIndex == INDEX_NONE)
    {
        PresetIndex = Direction > 0 ? -1 : 0;
    }
    PresetIndex = (PresetIndex + Direction + Presets[ControlsRow].Num()) % Presets[ControlsRow].Num();
    ApplyControlBinding(ActionName, Presets[ControlsRow][PresetIndex]);
}

void ABMPlayerController::ApplyControlBinding(const FName ActionName, const FKey& NewKey)
{
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return;
    }

    const TArray<FInputActionKeyMapping> ExistingMappings = InputSettings->GetActionMappings();
    TArray<FInputActionKeyMapping> UpdatedMappings;
    FString FailureReason;
    const BMInputRemap::ERemapResult Result = BMInputRemap::BuildKeyboardRemap(
        ExistingMappings,
        ActionName,
        NewKey,
        UpdatedMappings,
        FailureReason);
    if (Result != BMInputRemap::ERemapResult::Applied)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian controls remap rejected action=%s key=%s result=%s reason=%s"),
            *ActionName.ToString(),
            *NewKey.GetDisplayName().ToString(),
            BMInputRemap::LexToString(Result),
            *FailureReason);
        return;
    }

    for (const FInputActionKeyMapping& Mapping : ExistingMappings)
    {
        const bool bStillPresent = UpdatedMappings.ContainsByPredicate(
            [&Mapping](const FInputActionKeyMapping& Candidate)
            {
                return Candidate == Mapping;
            });
        if (!bStillPresent)
        {
            InputSettings->RemoveActionMapping(Mapping, false);
        }
    }
    for (const FInputActionKeyMapping& Mapping : UpdatedMappings)
    {
        const bool bWasAlreadyPresent = ExistingMappings.ContainsByPredicate(
            [&Mapping](const FInputActionKeyMapping& Candidate)
            {
                return Candidate == Mapping;
            });
        if (!bWasAlreadyPresent)
        {
            InputSettings->AddActionMapping(Mapping, false);
        }
    }
    InputSettings->SaveKeyMappings();
    if (PlayerInput)
    {
        PlayerInput->ForceRebuildingKeyMaps(true);
    }
}

void ABMPlayerController::AdjustSettings(const int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD(); HUD && HUD->GetScreen() == EBMHUDScreen::Controls)
    {
        AdjustControlBinding(Direction);
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD(); !HUD || HUD->GetScreen() != EBMHUDScreen::Settings)
    {
        return;
    }

    UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!Settings)
    {
        return;
    }

    const float Step = Direction > 0 ? 0.05f : -0.05f;
    switch (SettingsRow)
    {
    case 0:
        Settings->FieldOfView = FMath::Clamp(Settings->FieldOfView + static_cast<float>(Direction), 70.0f, 110.0f);
        break;
    case 1:
        Settings->HorizontalSensitivity = FMath::Clamp(Settings->HorizontalSensitivity + Step, 0.1f, 3.0f);
        break;
    case 2:
        Settings->VerticalSensitivity = FMath::Clamp(Settings->VerticalSensitivity + Step, 0.1f, 3.0f);
        break;
    case 3:
        Settings->HipFireSensitivity = FMath::Clamp(Settings->HipFireSensitivity + Step, 0.1f, 3.0f);
        break;
    case 4:
        Settings->ADSSensitivity = FMath::Clamp(Settings->ADSSensitivity + Step, 0.1f, 3.0f);
        break;
    case 5:
        Settings->bToggleADS = !Settings->bToggleADS;
        break;
    case 6:
        Settings->bToggleCrouch = !Settings->bToggleCrouch;
        break;
    case 7:
        Settings->bToggleSprint = !Settings->bToggleSprint;
        break;
    case 8:
        Settings->bReducedCameraMotion = !Settings->bReducedCameraMotion;
        break;
    case 9:
        Settings->bReducedMotionBlur = !Settings->bReducedMotionBlur;
        break;
    case 10:
        Settings->SubtitleSize = FMath::Clamp(Settings->SubtitleSize + Direction, 0, 2);
        break;
    case 11:
        Settings->ColorBlindPreset = FMath::Clamp(Settings->ColorBlindPreset + Direction, 0, 3);
        break;
    case 12:
        Settings->bGoreReduction = !Settings->bGoreReduction;
        break;
    case 13:
        Settings->GraphicsQualityPreset = FMath::Clamp(Settings->GraphicsQualityPreset + Direction, 0, 3);
        Settings->SetOverallScalabilityLevel(Settings->GraphicsQualityPreset);
        Settings->ApplySettings(false);
        break;
    case 14:
        Settings->MasterVolume = FMath::Clamp(Settings->MasterVolume + Step, 0.0f, 1.0f);
        break;
    case 15:
        Settings->MusicVolume = FMath::Clamp(Settings->MusicVolume + Step, 0.0f, 1.0f);
        break;
    case 16:
        Settings->EffectsVolume = FMath::Clamp(Settings->EffectsVolume + Step, 0.0f, 1.0f);
        break;
    case 17:
        Settings->DialogueVolume = FMath::Clamp(Settings->DialogueVolume + Step, 0.0f, 1.0f);
        break;
    case 18:
        Settings->bFriendlyFireEnabled = !Settings->bFriendlyFireEnabled;
        break;
    default:
        break;
    }
    Settings->SaveSettings();
    ApplyLocalSettings();
}

void ABMPlayerController::ApplyLocalSettings(const bool bApplyResolution)
{
    UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!Settings)
    {
        return;
    }
    Settings->FieldOfView = FMath::Clamp(Settings->FieldOfView, 70.0f, 110.0f);
    Settings->HorizontalSensitivity = FMath::Clamp(Settings->HorizontalSensitivity, 0.1f, 3.0f);
    Settings->VerticalSensitivity = FMath::Clamp(Settings->VerticalSensitivity, 0.1f, 3.0f);
    Settings->HipFireSensitivity = FMath::Clamp(Settings->HipFireSensitivity, 0.1f, 3.0f);
    Settings->ADSSensitivity = FMath::Clamp(Settings->ADSSensitivity, 0.1f, 3.0f);
    Settings->SubtitleSize = FMath::Clamp(Settings->SubtitleSize, 0, 2);
    Settings->ColorBlindPreset = FMath::Clamp(Settings->ColorBlindPreset, 0, 3);
    Settings->GraphicsQualityPreset = FMath::Clamp(Settings->GraphicsQualityPreset, 0, 3);
    Settings->SetOverallScalabilityLevel(Settings->GraphicsQualityPreset);
    if (bApplyResolution)
    {
        Settings->ApplySettings(false);
    }
    else
    {
        Settings->ApplyNonResolutionSettings();
    }
    Settings->MasterVolume = FMath::Clamp(Settings->MasterVolume, 0.0f, 1.0f);
    Settings->MusicVolume = FMath::Clamp(Settings->MusicVolume, 0.0f, 1.0f);
    Settings->EffectsVolume = FMath::Clamp(Settings->EffectsVolume, 0.0f, 1.0f);
    Settings->DialogueVolume = FMath::Clamp(Settings->DialogueVolume, 0.0f, 1.0f);

    if (IConsoleVariable* MotionBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
    {
        MotionBlur->Set(Settings->bReducedMotionBlur ? 0 : 4, ECVF_SetByGameSetting);
    }
    if (USoundMix* AudioMix = Settings->AudioMix.LoadSynchronous())
    {
        UGameplayStatics::PopSoundMixModifier(this, AudioMix);
        UGameplayStatics::PushSoundMixModifier(this, AudioMix);
        const auto ApplySoundClassVolume = [this, AudioMix](const TSoftObjectPtr<USoundClass>& SoundClass, const float Volume)
        {
            if (USoundClass* LoadedClass = SoundClass.LoadSynchronous())
            {
                UGameplayStatics::SetSoundMixClassOverride(this, AudioMix, LoadedClass, Volume, 1.0f, 0.1f, true);
            }
        };
        ApplySoundClassVolume(Settings->MasterSoundClass, Settings->MasterVolume);
        ApplySoundClassVolume(Settings->MusicSoundClass, Settings->MusicVolume);
        ApplySoundClassVolume(Settings->EffectsSoundClass, Settings->EffectsVolume);
        ApplySoundClassVolume(Settings->DialogueSoundClass, Settings->DialogueVolume);
    }
    if (MusicAudioComponent)
    {
        MusicAudioComponent->SetVolumeMultiplier(GetMusicVolumeMultiplier());
    }
    if (AmbientAudioComponent)
    {
        AmbientAudioComponent->SetVolumeMultiplier(GetMusicVolumeMultiplier());
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()); Player && Player->GetFirstPersonCamera())
    {
        Player->GetFirstPersonCamera()->SetFieldOfView(Player->IsAiming() ? 72.0f : Settings->FieldOfView);
    }
}

void ABMPlayerController::StartAmbientAudio()
{
    if (!IsLocalController() || bAmbientAudioStarted)
    {
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
    {
        bAmbientAudioStarted = true;
        return;
    }

    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Ambient = Audio->AmbientBattlefield.LoadSynchronous())
        {
            if (USoundWave* AmbientWave = Cast<USoundWave>(Ambient))
            {
                // Battlefield beds are intentionally persistent. Mark the
                // source as looping before creating the retained component so
                // a short imported wave cannot end and leave the mission
                // acoustically empty.
                AmbientWave->bLooping = true;
            }

            AmbientAudioComponent = UGameplayStatics::SpawnSound2D(
                this,
                Ambient,
                GetMusicVolumeMultiplier(),
                1.0f,
                0.0f,
                nullptr,
                false,
                false);

#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_AudioProbe")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian audio: ambient source=%s component=%s playing=%s loop=%s"),
                    *GetNameSafe(Ambient),
                    AmbientAudioComponent ? TEXT("yes") : TEXT("no"),
                    AmbientAudioComponent && AmbientAudioComponent->IsPlaying() ? TEXT("yes") : TEXT("no"),
                    Cast<USoundWave>(Ambient) && Cast<USoundWave>(Ambient)->bLooping ? TEXT("yes") : TEXT("no"));
            }
#endif
        }
    }
    SyncMissionMusicToGameState();
    bAmbientAudioStarted = true;
}

void ABMPlayerController::SyncMissionMusicToGameState()
{
    if (!IsLocalController())
    {
        return;
    }

    FName MusicState = TEXT("Mission");
    if (const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
    {
        MusicState = GetMusicStateForMissionPhase(GameState->MissionPhase);
    }
    PlayMusicState(MusicState);
}

float ABMPlayerController::GetMusicVolumeMultiplier() const
{
    const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!Settings)
    {
        return 1.0f;
    }

    // When an authored sound mix is present, its music class owns the setting
    // and this component must stay at unity to avoid applying the value twice.
    const bool bMusicClassIsRouted = Settings->AudioMix.IsValid() && Settings->MusicSoundClass.IsValid();
    return bMusicClassIsRouted
        ? 1.0f
        : FMath::Clamp(Settings->MasterVolume * Settings->MusicVolume, 0.0f, 1.0f);
}

void ABMPlayerController::PlayMusicState(const FName StateId)
{
    if (!IsLocalController())
    {
        ClientPlayMusicState(StateId);
        return;
    }

    if (StateId.IsNone())
    {
        return;
    }

    if (ActiveMusicState == StateId && MusicAudioComponent && MusicAudioComponent->IsPlaying())
    {
        return;
    }

    if (MusicAudioComponent)
    {
        MusicAudioComponent->Stop();
        MusicAudioComponent->DestroyComponent();
        MusicAudioComponent = nullptr;
    }
    ProceduralMusicWave = nullptr;

    if (FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
    {
        ActiveMusicState = StateId;
        UE_LOG(LogTemp, Display, TEXT("Black Meridian audio: music state=%s source=disabled"), *StateId.ToString());
        return;
    }

    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Music = Audio->FindMusicState(StateId).LoadSynchronous())
        {
            const bool bShouldLoop = StateId != TEXT("Failure") && StateId != TEXT("Complete");
            const bool bLoopConfigured = ConfigureMusicLoop(Music, bShouldLoop);
            MusicAudioComponent = UGameplayStatics::SpawnSound2D(
                this,
                Music,
                GetMusicVolumeMultiplier(),
                1.0f,
                0.0f,
                nullptr,
                false,
                false);
            if (MusicAudioComponent)
            {
                ActiveMusicState = StateId;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian audio: music state=%s source=authored volume=%.2f loop=%s"),
                    *StateId.ToString(),
                    GetMusicVolumeMultiplier(),
                    bShouldLoop && bLoopConfigured ? TEXT("yes") : TEXT("no"));
                return;
            }
        }
    }

    const TCHAR* GeneratedMusicName = nullptr;
    if (StateId == TEXT("Suspense"))
    {
        GeneratedMusicName = TEXT("BM_Music_Suspense");
    }
    else if (StateId == TEXT("Combat") || StateId == TEXT("SignalControl"))
    {
        GeneratedMusicName = TEXT("BM_Music_Combat");
    }
    else if (StateId == TEXT("Complete"))
    {
        GeneratedMusicName = TEXT("BM_Music_Complete");
    }
    else if (StateId == TEXT("Mission"))
    {
        GeneratedMusicName = TEXT("BM_Music_Mission");
    }
    if (GeneratedMusicName)
    {
        if (USoundBase* GeneratedMusic = LoadGeneratedCombatCue(GeneratedMusicName))
        {
            const bool bShouldLoop = StateId != TEXT("Failure") && StateId != TEXT("Complete");
            const bool bLoopConfigured = ConfigureMusicLoop(GeneratedMusic, bShouldLoop);
            MusicAudioComponent = UGameplayStatics::SpawnSound2D(
                this,
                GeneratedMusic,
                GetMusicVolumeMultiplier(),
                1.0f,
                0.0f,
                nullptr,
                false,
                false);
            if (MusicAudioComponent)
            {
                ActiveMusicState = StateId;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian audio: music state=%s source=generated_asset volume=%.2f loop=%s"),
                    *StateId.ToString(),
                    GetMusicVolumeMultiplier(),
                    bShouldLoop && bLoopConfigured ? TEXT("yes") : TEXT("no"));
                return;
            }
        }
    }

    float Duration = 0.0f;
    bool bLooping = false;
    TArray<uint8> LoopData = BuildProceduralMusicSamples(StateId, Duration, bLooping);
    if (LoopData.Num() == 0)
    {
        ActiveMusicState = NAME_None;
        return;
    }

    ProceduralMusicWave = NewObject<USoundWaveProcedural>(this);
    if (!ProceduralMusicWave)
    {
        ActiveMusicState = NAME_None;
        return;
    }
    ProceduralMusicWave->NumChannels = 2;
    ProceduralMusicWave->SetSampleRate(22050);
    ProceduralMusicWave->Duration = Duration;
    ProceduralMusicWave->SoundGroup = SOUNDGROUP_Music;
    ProceduralMusicWave->bLooping = bLooping;
    ProceduralMusicWave->QueueAudio(LoopData.GetData(), LoopData.Num());

    if (bLooping)
    {
        TArray<uint8> RefillData = MoveTemp(LoopData);
        ProceduralMusicWave->OnSoundWaveProceduralUnderflow.BindLambda(
            [RefillData = MoveTemp(RefillData)](USoundWaveProcedural* Wave, int32)
            {
                if (Wave && RefillData.Num() > 0)
                {
                    Wave->QueueAudio(RefillData.GetData(), RefillData.Num());
                }
            });
    }

    MusicAudioComponent = UGameplayStatics::SpawnSound2D(
        this,
        ProceduralMusicWave,
        GetMusicVolumeMultiplier(),
        1.0f,
        0.0f,
        nullptr,
        false,
        false);
    if (MusicAudioComponent)
    {
        ActiveMusicState = StateId;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian audio: music state=%s source=procedural loop=%s volume=%.2f"),
            *StateId.ToString(),
            bLooping ? TEXT("yes") : TEXT("no"),
            GetMusicVolumeMultiplier());
    }
    else
    {
        ProceduralMusicWave = nullptr;
        ActiveMusicState = NAME_None;
    }
}

void ABMPlayerController::PlayDialogueCue()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Dialogue = Audio->PositionalDialogue.LoadSynchronous())
        {
            const FVector Location = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
            UGameplayStatics::PlaySoundAtLocation(this, Dialogue, Location);
        }
    }
}

void ABMPlayerController::RestartCheckpointInput()
{
    ABMHUD* HUD = GetBlackMeridianHUD();
    if (!HUD || (HUD->GetScreen() != EBMHUDScreen::Death && HUD->GetScreen() != EBMHUDScreen::Pause))
    {
        return;
    }

    if (HasAuthority())
    {
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            if (ABMMissionDirector* Mission = GameMode->GetMissionDirector())
            {
                Mission->RestartFromCheckpoint();
            }
        }
    }
    else
    {
        ServerRequestCheckpointRestart();
    }
    HUD->SetScreen(EBMHUDScreen::InGame);
    SetPause(false);

    // A campaign death switches the local presentation to the failure cue.
    // Restore the phase-driven combat/suspense bed before recapturing the
    // mouse so the checkpoint does not resume under a stale death state.
    SyncMissionMusicToGameState();
    RestoreInteractiveInput();

    // A campaign death pauses the world and leaves the old viewport capture
    // state behind. Reapply the ordinary game-only input mode after the
    // checkpoint request so the restarted pawn can turn immediately, even
    // when the user presses Enter before macOS has sent a focus callback.
    GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::RestoreInteractiveInput);
}

void ABMPlayerController::ToggleScoreboardInput()
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScoreboardVisible(HUD->GetScreen() != EBMHUDScreen::Scoreboard);
    }
}

void ABMPlayerController::BackInput()
{
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->ResetFireInputState();
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        if (HUD->GetScreen() == EBMHUDScreen::HowToPlay)
        {
            HUD->SetScreen(HowToPlayReturnScreen);
            const bool bReturningToGameplay = HowToPlayReturnScreen == EBMHUDScreen::InGame ||
                HowToPlayReturnScreen == EBMHUDScreen::Scoreboard;
            SetPause(!bReturningToGameplay);
            if (bReturningToGameplay)
            {
                RestoreInteractiveInput();
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Settings)
        {
            HUD->SetScreen(SettingsReturnScreen);
            const bool bReturningToGameplay = SettingsReturnScreen == EBMHUDScreen::InGame ||
                SettingsReturnScreen == EBMHUDScreen::Scoreboard;
            SetPause(!bReturningToGameplay);
            if (bReturningToGameplay)
            {
                RestoreInteractiveInput();
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Controls)
        {
            HUD->SetScreen(ControlsReturnScreen);
            const bool bReturningToGameplay = ControlsReturnScreen == EBMHUDScreen::InGame ||
                ControlsReturnScreen == EBMHUDScreen::Scoreboard;
            SetPause(!bReturningToGameplay);
            if (bReturningToGameplay)
            {
                RestoreInteractiveInput();
            }
        }
        else if (HUD->GetScreen() == EBMHUDScreen::CampaignSelection || HUD->GetScreen() == EBMHUDScreen::MultiplayerMenu || HUD->GetScreen() == EBMHUDScreen::Lobby)
        {
            HUD->SetScreen(EBMHUDScreen::MainMenu);
        }
        // Escape is mapped to both TogglePause and Back so it can close
        // menus/death/results screens. TogglePause owns the InGame <-> Pause
        // transition; if Back also consumes the newly-created Pause screen,
        // one physical Escape press immediately undoes the pause. Leave the
        // Pause case to TogglePauseInput so Escape resumes cleanly on the
        // next press instead.
        else if (HUD->GetScreen() == EBMHUDScreen::Death)
        {
            // Match the Death overlay contract: ESC returns to the campaign
            // menu instead of leaving the player on a paused, non-lookable
            // dead pawn.
            RequestMissionTravel();
        }
        else if (HUD->GetScreen() == EBMHUDScreen::Results)
        {
            RequestMissionTravel();
        }
    }
}

void ABMPlayerController::ServerRequestMissionStart_Implementation(const EBMCampaignLevel RequestedLevel)
{
    const EBMCampaignLevel ValidatedLevel = static_cast<EBMCampaignLevel>(FMath::Clamp(
        static_cast<int32>(RequestedLevel),
        static_cast<int32>(EBMCampaignLevel::SableQuay),
        static_cast<int32>(EBMCampaignLevel::LastLight)));
    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        GameMode->RequestMissionStart(ValidatedLevel);
    }
}

void ABMPlayerController::ServerRequestCheckpointRestart_Implementation()
{
    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        if (ABMMissionDirector* Mission = GameMode->GetMissionDirector())
        {
            Mission->RestartFromCheckpoint();
        }
    }
}

void ABMPlayerController::ServerRequestMatchStart_Implementation()
{
    if (ABMSignalControlGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>())
    {
        GameMode->RequestMatchStart();
    }
}

void ABMPlayerController::RequestSignalControlTravel()
{
    static const FString SignalOptions = TEXT("game=/Script/BlackMeridianRuntime.BMSignalControlGameMode");
    static const FString SignalTravelURL = TEXT("/Engine/Maps/Entry?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode");
    if (!GetWorld())
    {
        return;
    }
    ShowLoadingScreen();
    if (GetNetMode() == NM_Standalone)
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, SignalOptions);
    }
    else if (HasAuthority())
    {
        GetWorld()->ServerTravel(SignalTravelURL, true);
    }
    else
    {
        ServerRequestSignalControlTravel();
    }
}

void ABMPlayerController::RequestMissionTravel()
{
    static const FString MissionOptions = TEXT("game=/Script/BlackMeridianRuntime.BMGameMode");
    static const FString MissionTravelURL = TEXT("/Engine/Maps/Entry?game=/Script/BlackMeridianRuntime.BMGameMode");
    if (!GetWorld())
    {
        return;
    }
    ShowLoadingScreen();
    bSignalControlLobby = false;
    if (GetNetMode() == NM_Standalone)
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, MissionOptions);
    }
    else if (HasAuthority())
    {
        GetWorld()->ServerTravel(MissionTravelURL, true);
    }
    else
    {
        ServerRequestMissionTravel();
    }
}

void ABMPlayerController::RequestMissionTravelToLevel(const EBMCampaignLevel RequestedLevel)
{
    const int32 LevelIndex = FMath::Clamp(
        static_cast<int32>(RequestedLevel),
        static_cast<int32>(EBMCampaignLevel::SableQuay),
        static_cast<int32>(EBMCampaignLevel::LastLight));
    const FString MissionOptions = FString::Printf(
        // Unreal URL options are separated with '?'. Using '&' makes the
        // campaign level part of the game-mode class name, so a confirmed
        // next-level selection silently falls back to Sable Quay.
        TEXT("game=/Script/BlackMeridianRuntime.BMGameMode?BM_CampaignLevel=%d"),
        LevelIndex);
    const FString MissionTravelURL = FString::Printf(
        TEXT("/Engine/Maps/Entry?game=/Script/BlackMeridianRuntime.BMGameMode?BM_CampaignLevel=%d"),
        LevelIndex);
    if (!GetWorld())
    {
        return;
    }

    ShowLoadingScreen();
    bSignalControlLobby = false;
    if (GetNetMode() == NM_Standalone)
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, MissionOptions);
    }
    else if (HasAuthority())
    {
        GetWorld()->ServerTravel(MissionTravelURL, true);
    }
    else
    {
        ServerRequestNextCampaignLevel();
    }
}

bool ABMPlayerController::TryGetNextCampaignLevel(EBMCampaignLevel& OutLevel) const
{
    if (IsSignalControlWorld())
    {
        return false;
    }

    const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    if (!State)
    {
        return false;
    }

    const int32 CurrentIndex = static_cast<int32>(State->CampaignLevel);
    if (CurrentIndex < 0 || CurrentIndex >= GetBMCampaignLevelCount() - 1)
    {
        return false;
    }

    OutLevel = static_cast<EBMCampaignLevel>(CurrentIndex + 1);
    return true;
}

bool ABMPlayerController::IsSignalControlWorld() const
{
    return bSignalControlLobby || (GetWorld() && GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>() != nullptr);
}

void ABMPlayerController::ShowLoadingScreen()
{
    if (!IsLocalController())
    {
        return;
    }

    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->ResetFireInputState();
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::Loading);
    }
    SetPause(true);
}

void ABMPlayerController::ServerRequestSignalControlTravel_Implementation()
{
    RequestSignalControlTravel();
}

void ABMPlayerController::ServerRequestMissionTravel_Implementation()
{
    RequestMissionTravel();
}

void ABMPlayerController::ServerRequestNextCampaignLevel_Implementation()
{
    EBMCampaignLevel NextLevel = EBMCampaignLevel::SableQuay;
    if (TryGetNextCampaignLevel(NextLevel))
    {
        RequestMissionTravelToLevel(NextLevel);
    }
    else
    {
        RequestMissionTravel();
    }
}

void ABMPlayerController::ShowSubtitle(const FText& Text, const float Duration)
{
    if (!IsLocalController())
    {
        ClientShowSubtitle(Text, Duration);
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowSubtitle(Text, Duration);
    }
    PlayDialogueCue();
}

void ABMPlayerController::ShowMissionCard(const FText& Title, const FText& Body, const float Duration)
{
    if (!IsLocalController())
    {
        ClientShowMissionCard(Title, Body, Duration);
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowMissionCard(Title, Body, Duration);
    }
}

void ABMPlayerController::ShowDeathScreen()
{
    if (!IsLocalController())
    {
        ClientShowDeathScreen();
        return;
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->ResetFireInputState();
    }
    const bool bSignalControl = IsSignalControlWorld();
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::Death);
        // Signal Control is a live team match. The rest of the world must
        // keep ticking so the authoritative respawn timer can run while the
        // local player sees the downed overlay.
        SetPause(!bSignalControl);

        if (!bSignalControl)
        {
            // The dead pawn is deliberately not lookable or movable. Release
            // the captured cursor, however, so the player can see that this
            // is a recoverable menu state instead of a frozen camera/input
            // failure. RestoreInteractiveInput() resets both guards on Enter.
            bShowMouseCursor = true;
            // Keep the controller in GameOnly mode so legacy action bindings
            // marked bExecuteWhenPaused (including Enter -> StartMission)
            // still reach this paused controller on macOS. The visible cursor
            // and explicit ignore guards provide the menu-state presentation
            // without routing keyboard recovery through Slate UI focus.
            SetInputMode(FInputModeGameOnly());
            SetIgnoreLookInput(true);
            SetIgnoreMoveInput(true);
            PlayMusicState(TEXT("Failure"));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UI: death screen signal_control=%s paused=%s"),
            bSignalControl ? TEXT("yes") : TEXT("no"),
            IsPaused() ? TEXT("yes") : TEXT("no"));
    }
}

void ABMPlayerController::ShowRespawnedPlayer()
{
    if (!IsLocalController())
    {
        ClientShowRespawnedPlayer();
        return;
    }

    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::InGame);
    }
    SetPause(false);
    RestoreInteractiveInput();
    GetWorldTimerManager().SetTimerForNextTick(this, &ABMPlayerController::RestoreInteractiveInput);
    UE_LOG(LogTemp, Display, TEXT("Black Meridian UI: respawned player presentation restored"));
}

void ABMPlayerController::ShowResultsScreen()
{
    if (!IsLocalController())
    {
        ClientShowResultsScreen();
        return;
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
    {
        Player->ResetFireInputState();
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::Results);
        SetPause(true);
        if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr; State && State->bSignalControlMode)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian UI: results screen signal_control=yes winner=%d lattice_score=%d vanta_score=%d"),
                static_cast<int32>(State->SignalControl.WinningTeam),
                State->SignalControl.LatticeScore,
                State->SignalControl.VantaScore);
        }
        else if (const ABMGameState* CampaignState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
        {
            const bool bHasNextCampaignLevel = static_cast<int32>(CampaignState->CampaignLevel) < GetBMCampaignLevelCount() - 1;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian UI: campaign completion prompt current_level=%d next_level=%d default_action=%s alternate_action=%s"),
                static_cast<int32>(CampaignState->CampaignLevel) + 1,
                bHasNextCampaignLevel ? static_cast<int32>(CampaignState->CampaignLevel) + 2 : 0,
                bHasNextCampaignLevel ? TEXT("NEXT_LEVEL") : TEXT("RETURN_TO_MENU"),
                bHasNextCampaignLevel ? TEXT("CAMPAIGN_MENU") : TEXT("CLOSE"));
        }
    }
}

void ABMPlayerController::SetDamageDirection(const float RelativeYaw)
{
    if (!IsLocalController())
    {
        ClientSetDamageDirection(RelativeYaw);
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetDamageDirection(RelativeYaw);
    }
}

void ABMPlayerController::ShowDamageFeedback(const float Intensity)
{
    if (!IsLocalController())
    {
        return;
    }

    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowDamageFlash(Intensity);
    }
}

void ABMPlayerController::SetInteractionPrompt(const FText& Prompt)
{
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowInteractionPrompt(Prompt);
    }
}

void ABMPlayerController::ShowSignalLobby()
{
    bSignalControlLobby = true;
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_SignalControlProbe")))
    {
        UE_LOG(LogTemp, Display, TEXT("Black Meridian signal probe: lobby ready authority=%s"), HasAuthority() ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (!IsLocalController())
    {
        ClientShowSignalLobby();
        return;
    }
    InitializeLocalPresentation();
}

void ABMPlayerController::RequestSignalMatchStartFromUI()
{
    StartMissionInput();
}

void ABMPlayerController::UIStartMission()
{
    StartMissionInput();
}

void ABMPlayerController::UIStartNextCampaignLevel()
{
    EBMCampaignLevel NextLevel = EBMCampaignLevel::SableQuay;
    if (!TryGetNextCampaignLevel(NextLevel))
    {
        RequestMissionTravel();
        return;
    }

    if (HasAuthority() || GetNetMode() == NM_Standalone)
    {
        RequestMissionTravelToLevel(NextLevel);
    }
    else
    {
        ServerRequestNextCampaignLevel();
    }
}

void ABMPlayerController::UIOpenCampaign()
{
    OpenCampaignInput();
}

void ABMPlayerController::UIOpenMultiplayer()
{
    OpenMultiplayerInput();
}

void ABMPlayerController::UIOpenSettings()
{
    OpenSettingsInput();
}

void ABMPlayerController::UIOpenControls()
{
    OpenControlsInput();
}

void ABMPlayerController::UIOpenHowToPlay()
{
    OpenHowToPlayInput();
}

void ABMPlayerController::UIBack()
{
    BackInput();
}

void ABMPlayerController::UIRestartCheckpoint()
{
    RestartCheckpointInput();
}

void ABMPlayerController::UIReturnToMenu()
{
    RequestMissionTravel();
}

void ABMPlayerController::UITogglePause()
{
    TogglePauseInput();
}

void ABMPlayerController::ShowMissionMenu()
{
    bSignalControlLobby = false;
#if !UE_BUILD_SHIPPING
    const bool bThrowableProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableProbe"));
    const bool bDevelopmentCombatStart = FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartMission"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase"));
#else
    constexpr bool bThrowableProbe = false;
    constexpr bool bDevelopmentCombatStart = false;
#endif
    if (!IsLocalController())
    {
        ClientShowMissionMenu();
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        const bool bSkipMissionMenu = bThrowableProbe || bDevelopmentCombatStart;
        if (!bSkipMissionMenu)
        {
            if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn()))
            {
                Player->ResetFireInputState();
            }
        }
        HUD->SetScreen(bSkipMissionMenu ? EBMHUDScreen::InGame : EBMHUDScreen::MainMenu);
        SetPause(!bSkipMissionMenu);
        if (bSkipMissionMenu)
        {
            RestoreInteractiveInput();
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UI: mission presentation screen=%d paused=%s development_start=%s"),
            static_cast<int32>(HUD->GetScreen()),
            GetWorld() && GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"),
            bDevelopmentCombatStart ? TEXT("yes") : TEXT("no"));
    }
    FName MenuMusicState = TEXT("Mission");
    if (const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        GameState && GameState->bMatchLive && GameState->MissionPhase != EBMMissionPhase::Intro)
    {
        MenuMusicState = GetMusicStateForMissionPhase(GameState->MissionPhase);
    }
    else if (ActiveMusicState == TEXT("Suspense") || ActiveMusicState == TEXT("Combat"))
    {
        // PostLogin can show the menu after the authority has already
        // auto-started the campaign. Do not let that presentation hand-off
        // replace the live mission bed with menu music.
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian audio: preserved live music state=%s during menu hand-off"),
            *ActiveMusicState.ToString());
        return;
    }
    PlayMusicState(MenuMusicState);
}

void ABMPlayerController::NotifyHitMarker(const bool bHeadshot)
{
    if (!IsLocalController())
    {
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowHitMarker(bHeadshot);
    }
    PlayRuntimeCombatCue(this, AudioProfile.LoadSynchronous(), false, bHeadshot);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AudioProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian audio probe: hit marker dispatched headshot=%s controller=%s"),
            bHeadshot ? TEXT("yes") : TEXT("no"),
            *GetNameSafe(this));
    }
#endif
}

void ABMPlayerController::NotifyDryFire()
{
    if (!IsLocalController())
    {
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowDryFire();
    }
}

void ABMPlayerController::NotifyElimination(const bool bHeadshot, const int32 KillCount)
{
    int32 ResolvedKillCount = KillCount;
    if (ResolvedKillCount < 0)
    {
        if (const ABMPlayerState* LocalPlayerState = GetPlayerState<ABMPlayerState>())
        {
            ResolvedKillCount = LocalPlayerState->Kills;
        }
        else
        {
            ResolvedKillCount = 0;
        }
    }
    if (!IsLocalController())
    {
        ClientShowElimination(bHeadshot, ResolvedKillCount);
        return;
    }
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->ShowElimination(bHeadshot, ResolvedKillCount);
    }
    PlayRuntimeCombatCue(this, AudioProfile.LoadSynchronous(), true, bHeadshot);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat feedback: elimination confirmed headshot=%s kills=%d"),
        bHeadshot ? TEXT("yes") : TEXT("no"),
        ResolvedKillCount);
}

#if !UE_BUILD_SHIPPING
void ABMPlayerController::StartCombatProbeFire(
    const FVector& Direction,
    const FVector& ProbeLocation,
    const FRotator& ProbeRotation)
{
    if (!HasAuthority())
    {
        return;
    }

    // Keep the expected view on the authoritative controller. The staged
    // pawn is deliberately held in MOVE_None, so its normal movement packet
    // cannot carry the freshly prepared control rotation back to the server.
    // The client acknowledgement below makes the probe wait for this exact
    // view instead of weakening the ordinary server cone validation.
    PendingCombatProbeDirection = Direction.GetSafeNormal();
    PendingCombatProbeLocation = ProbeLocation;
    PendingCombatProbeRotation = ProbeRotation;
    bCombatProbeViewPending = true;
    ClientRunCombatProbeFire(Direction.GetSafeNormal(), ProbeLocation, ProbeRotation);
}

void ABMPlayerController::StartCombatProbeReload()
{
    if (HasAuthority())
    {
        ClientRunCombatProbeReload();
    }
}

void ABMPlayerController::StartCombatProbeReloadInterrupt()
{
    if (HasAuthority())
    {
        ClientRunCombatProbeReloadInterrupt();
    }
}
#endif

void ABMPlayerController::ClientRunCombatProbeFire_Implementation(
    const FVector_NetQuantizeNormal Direction,
    const FVector_NetQuantize ProbeLocation,
    const FRotator ProbeRotation)
{
#if !UE_BUILD_SHIPPING
    if (!IsLocalController())
    {
        return;
    }

    PendingCombatProbeDirection = Direction;
    PendingCombatProbeLocation = ProbeLocation;
    PendingCombatProbeRotation = ProbeRotation;
    ExecuteCombatProbeFire(Direction);
#else
    (void)Direction;
#endif
}

#if !UE_BUILD_SHIPPING
void ABMPlayerController::ExecuteCombatProbeFire(const FVector_NetQuantizeNormal Direction)
{
    if (!IsLocalController())
    {
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Player || !Player->GetCurrentWeapon())
    {
        // Packaged clients can spend substantially longer creating the
        // authored character, weapon mesh, and replicated loadout than an
        // editor client. Keep the development probe alive until the client
        // has actually received the pawn instead of reporting a false combat
        // failure while the level is still streaming.
        constexpr int32 MaxCombatProbeRetries = 800;
        if (CombatProbeRetryCount++ < MaxCombatProbeRetries)
        {
            GetWorldTimerManager().SetTimer(
                CombatProbeRetryTimer,
                this,
                &ABMPlayerController::RetryCombatProbeFire,
                0.15f,
                false);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian combat probe: client fire abandoned after waiting for pawn/weapon pawn=%s weapon=%s"),
                *GetNameSafe(Player),
                Player && Player->GetCurrentWeapon() ? TEXT("yes") : TEXT("no"));
        }
        return;
    }

    CombatProbeRetryCount = 0;

    // CharacterMovement can still be replaying saved moves when the server
    // stages the probe pawn. Apply the authoritative transform on the owning
    // client before using the normal camera-origin fire path; the server keeps
    // its existing shot-origin validation unchanged.
    Player->PrepareCombatProbeTransform(PendingCombatProbeLocation, PendingCombatProbeRotation);

    // Signal Control opens in a paused lobby. Release only this probe-owned
    // client before invoking the ordinary player input/fire path.
    SetPause(false);
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::InGame);
    }

    // Do not fire until the server has installed the same view rotation on
    // its controller copy. This preserves the production view-cone guard and
    // removes the listen-server staging race from the evidence harness.
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client view sync request direction=%s rotation=%s"),
        *Direction.ToString(),
        *PendingCombatProbeRotation.ToString());
    ServerPrepareCombatProbeView(PendingCombatProbeRotation);
}

void ABMPlayerController::RetryCombatProbeFire()
{
    ExecuteCombatProbeFire(PendingCombatProbeDirection);
}

#endif

void ABMPlayerController::ServerPrepareCombatProbeView_Implementation(const FRotator ProbeRotation)
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority()
        || !bCombatProbeViewPending
        || !FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || ProbeRotation.ContainsNaN())
    {
        return;
    }

    // The client may only acknowledge the rotation supplied by the
    // authoritative probe dispatcher. Treat any other value as a malformed
    // development request and leave the normal shot path untouched.
    if (!ProbeRotation.Equals(PendingCombatProbeRotation, 1.0f))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian combat probe: view sync rejected rotation=%s expected=%s"),
            *ProbeRotation.ToString(),
            *PendingCombatProbeRotation.ToString());
        return;
    }

    SetControlRotation(PendingCombatProbeRotation);
    bCombatProbeViewPending = false;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: server view synchronized rotation=%s"),
        *GetControlRotation().ToString());
    ClientCombatProbeViewReady();
#else
    (void)ProbeRotation;
#endif
}

void ABMPlayerController::ClientCombatProbeViewReady_Implementation()
{
#if !UE_BUILD_SHIPPING
    if (!IsLocalController())
    {
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Player || !Player->GetCurrentWeapon())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian combat probe: view sync acknowledged without pawn/weapon pawn=%s weapon=%s"),
            *GetNameSafe(Player),
            Player && Player->GetCurrentWeapon() ? TEXT("yes") : TEXT("no"));
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client fire dispatch direction=%s"),
        *PendingCombatProbeDirection.ToString());
    Player->RunCombatProbeFire(PendingCombatProbeDirection);
#endif
}

void ABMPlayerController::ClientRunCombatProbeReload_Implementation()
{
#if !UE_BUILD_SHIPPING
    ExecuteCombatProbeReload(false);
#endif
}

void ABMPlayerController::ClientRunCombatProbeReloadInterrupt_Implementation()
{
#if !UE_BUILD_SHIPPING
    ExecuteCombatProbeReload(true);
#endif
}

#if !UE_BUILD_SHIPPING
void ABMPlayerController::ExecuteCombatProbeReload(const bool bInterrupt)
{
    if (!IsLocalController())
    {
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Player || !Player->GetCurrentWeapon())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian combat probe: client reload action unavailable pawn=%s weapon=%s interrupt=%s"),
            *GetNameSafe(Player),
            Player && Player->GetCurrentWeapon() ? TEXT("yes") : TEXT("no"),
            bInterrupt ? TEXT("yes") : TEXT("no"));
        return;
    }

    SetPause(false);
    if (ABMHUD* HUD = GetBlackMeridianHUD())
    {
        HUD->SetScreen(EBMHUDScreen::InGame);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client reload dispatch action=%s"),
        bInterrupt ? TEXT("interrupt") : TEXT("reload"));
    if (bInterrupt)
    {
        Player->RunCombatProbeReloadInterrupt();
    }
    else
    {
        Player->RunCombatProbeReload();
    }
}
#endif

void ABMPlayerController::ClientShowSubtitle_Implementation(const FText& Text, const float Duration)
{
    ShowSubtitle(Text, Duration);
}

void ABMPlayerController::ClientShowMissionCard_Implementation(const FText& Title, const FText& Body, const float Duration)
{
    ShowMissionCard(Title, Body, Duration);
}

void ABMPlayerController::ClientShowDeathScreen_Implementation()
{
    ShowDeathScreen();
}

void ABMPlayerController::ClientShowRespawnedPlayer_Implementation()
{
    ShowRespawnedPlayer();
}

void ABMPlayerController::ClientShowResultsScreen_Implementation()
{
    ShowResultsScreen();
}

void ABMPlayerController::ClientPlayMusicState_Implementation(const FName StateId)
{
    PlayMusicState(StateId);
}

void ABMPlayerController::ClientSetDamageDirection_Implementation(const float RelativeYaw)
{
    SetDamageDirection(RelativeYaw);
}

void ABMPlayerController::ClientShowSignalLobby_Implementation()
{
    ShowSignalLobby();
}

void ABMPlayerController::ClientShowMissionMenu_Implementation()
{
    ShowMissionMenu();
}

void ABMPlayerController::ClientShowElimination_Implementation(const bool bHeadshot, const int32 KillCount)
{
    NotifyElimination(bHeadshot, KillCount);
}

ABMHUD* ABMPlayerController::GetBlackMeridianHUD() const
{
    return Cast<ABMHUD>(GetHUD());
}
