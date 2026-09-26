#ifndef AUDIO_HPP
#define AUDIO_HPP

// Lydeffekter. Alle lydene lages med kode (syntese) når spillet starter,
// så vi trenger ingen lydfiler. Se audio.cpp for hvordan hver lyd er bygget.

enum class Sfx {
    XP,           // Plukke opp XP
    COIN,         // Plukke opp gull / kjøpe i shoppen
    HIT,          // Treffe en fiende
    KILL,         // Drepe en fiende
    PLAYER_HURT,  // Spilleren tar skade
    LEVEL_UP,
    THUNDER,      // Lynnedslag
    ZAP,          // Kjedelyn hopper
    EXPLOSION,    // Kamikaze-eksplosjon
    BOSS_GONG,    // Teleport til tronsalen
    BOSS_CHARGE,  // Kongen lader opp Royal Charge
    VICTORY,      // Bossen er slått
    DEATH,        // Spilleren døde
    UI_MOVE,      // Bla i menyer
    UI_SELECT,    // Bekrefte i menyer
    COUNT
};

void InitGameAudio();   // Etter InitWindow
void UnloadGameAudio(); // Før CloseWindow
void PlaySfx(Sfx sfx);

// Hovedvolum 0.0 - 1.0
void SetGameVolume(float volume);
float GetGameVolume();

#endif // AUDIO_HPP
