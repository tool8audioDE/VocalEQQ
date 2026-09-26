#pragma once

/** Parameter-IDs sind ab dem ersten Release VERBINDLICH.

    Eine DAW speichert im Projekt nicht den Reglernamen, sondern diese
    Zeichenkette. Wird eine ID später umbenannt, findet das Plugin beim Laden
    eines alten Projekts den Wert nicht mehr und fällt auf den Standardwert
    zurück — die Einstellungen des Nutzers sind dann weg. Neue Parameter
    dürfen jederzeit dazukommen, bestehende nicht umbenannt werden.
*/
namespace vocaleqq::ParamID
{
    inline constexpr const char* deEssOn          = "deessOn";
    inline constexpr const char* deEssReduction   = "deessReduction";
    inline constexpr const char* deEssSensitivity = "deessSensitivity";
    inline constexpr const char* eqOn             = "eqOn";
    inline constexpr const char* eqStrength       = "eqStrength";
    inline constexpr const char* eqLowCut         = "eqLowCut";
    inline constexpr const char* compOn           = "compOn";
    inline constexpr const char* compAmount       = "compAmount";
    inline constexpr const char* mix              = "mix";
    inline constexpr const char* autoGain         = "autoGain";
    inline constexpr const char* bypass           = "bypass";
}
