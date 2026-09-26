#ifndef MUSIC_HPP
#define MUSIC_HPP

// Bakgrunnsmusikk. Som lydeffektene lages den helt i kode (se music.cpp):
// en liten sequencer spiller akkorder, bass, arpeggio, melodi og trommer,
// og hvert spor rendres én gang når spillet starter og loopes.

enum class MusicTrack {
    NONE,
    MENU,   // Hoffnarr-vals i 3/4
    GAME,   // Drivende spor under spillingen
    BOSS,   // Mørkt og raskt i tronsalen
    COUNT
};

void InitGameMusic();              // Etter InitGameAudio
void UnloadGameMusic();
void UpdateGameMusic(float deltaTime);
void SetMusicTrack(MusicTrack track);  // Krysstoner over til nytt spor
void SetMusicVolume01(float volume);   // 0.0 - 1.0 (i tillegg til hovedvolumet)

#endif // MUSIC_HPP
