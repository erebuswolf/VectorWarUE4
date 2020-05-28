// Copyright 2020 BwdYeti.

#include "VWGameStateBase.h"
#include "include/ggponet.h"

#define ARRAYSIZE(a) sizeof(a) / sizeof(a[0])
#define FRAME_RATE 60
#define ONE_FRAME (1.0f / FRAME_RATE)

void AVWGameStateBase::BeginPlay()
{
    Super::BeginPlay();

    Hwnd = StartSinglePlayerGGPOSession();

    if (!Hwnd)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create GGPO session"));
    }
}

void AVWGameStateBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    MSG msg = { 0 };

    ElapsedTime += DeltaSeconds;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) { }
    }
    int32 IdleMs = (int32)(ONE_FRAME - (int32)(ElapsedTime * 1000));
    VectorWarHost::VectorWar_Idle(FMath::Max(0, IdleMs - 1));
    while (ElapsedTime >= ONE_FRAME) {
        VectorWarHost::VectorWar_RunFrame(Hwnd);
        ElapsedTime -= ONE_FRAME;
    }
}

void AVWGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (Hwnd)
    {
        VectorWarHost::VectorWar_Exit();
        VectorWarHost::DestroyWindow(Hwnd);

        Hwnd = 0;
    }
}

HWND AVWGameStateBase::StartSinglePlayerGGPOSession()
{
    // Get port
    uint16 LocalPort = 7000;
    // Get number of players
    int32 NumPlayers = 1;

    return StartGGPOPlayerSession(LocalPort, NumPlayers, { L"local" });
}

HWND AVWGameStateBase::StartGGPOPlayerSession(
    const uint16 LocalPort,
    const int32 NumPlayers,
    TArray<wchar_t*> PlayerParameters)
{
    int32 Offset = 0;
    wchar_t WideIpBuffer[128];
    uint32 WideIpBufferSize = (uint32)ARRAYSIZE(WideIpBuffer);

    GGPOPlayer Players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];

    if (NumPlayers > PlayerParameters.Num())
        return 0;

    int32 i;
    for (i = 0; i < NumPlayers; i++) {
        const wchar_t* Arg = PlayerParameters[Offset++];

        Players[i].size = sizeof(Players[i]);
        Players[i].player_num = i + 1;
        if (!_wcsicmp(Arg, L"local")) {
            Players[i].type = GGPO_PLAYERTYPE_LOCAL;
            continue;
        }

        Players[i].type = GGPO_PLAYERTYPE_REMOTE;
        if (swscanf_s(Arg, L"%[^:]:%hd", WideIpBuffer, WideIpBufferSize, &Players[i].u.remote.port) != 2) {
            return 0;
        }
        wcstombs_s(nullptr, Players[i].u.remote.ip_address, ARRAYSIZE(Players[i].u.remote.ip_address), WideIpBuffer, _TRUNCATE);
    }
    // these are spectators...
    int32 NumSpectators = 0;
    while (Offset < PlayerParameters.Num()) {
        Players[i].type = GGPO_PLAYERTYPE_SPECTATOR;
        if (swscanf_s(PlayerParameters[Offset++], L"%[^:]:%hd", WideIpBuffer, WideIpBufferSize, &Players[i].u.remote.port) != 2) {
            return 0;
        }
        wcstombs_s(nullptr, Players[i].u.remote.ip_address, ARRAYSIZE(Players[i].u.remote.ip_address), WideIpBuffer, _TRUNCATE);
        i++;
        NumSpectators++;
    }

    HWND hwnd = VectorWarHost::CreateMainWindow(hInstance);
    VectorWarHost::VectorWar_Init(hwnd, LocalPort, NumPlayers, Players, NumSpectators);

    UE_LOG(LogTemp, Warning, TEXT("GGPO session started in a separate window"));

    return hwnd;
}