#pragma once

#include <Geode/Geode.hpp>

namespace nt {
    // Supabase project "naters-tournament": every call is an RPC on a SECURITY DEFINER function.
    inline constexpr char const* kApiUrl = "https://twkodkllcyvttnytszdy.supabase.co/rest/v1/rpc/";
    inline constexpr char const* kApiKey = "sb_publishable_8IFoarwL26BQ_itAyN1Ixg_D68n3Ukf";

    // Tournaments only mix players on the same game version.
    inline constexpr char const* kGdVersion = GEODE_GD_VERSION_STRING;

    inline constexpr int kEmoteCount = 15;
    inline constexpr int kMaxPlayers = 32;

    // seconds between polls
    inline constexpr float kPollLobby = 1.0f;
    inline constexpr float kPollBackground = 3.0f;
    inline constexpr float kReportInterval = 0.5f;
}
