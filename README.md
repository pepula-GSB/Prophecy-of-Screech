# PROPHECY OF SCREAM
Syntezator screechy do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

## Jak zdobyć plik .vst3 (Windows)

**Opcja A – bez instalowania niczego (GitHub):**
1. Załóż nowe repo na GitHubie i wrzuć do niego zawartość tego folderu (z ukrytym folderem `.github`).
2. Zakładka **Actions** → workflow „Build Windows VST3” uruchomi się sam (ok. 5–8 min).
3. Po zakończeniu pobierz artefakt **Prophecy-of-Scream-VST3-Windows** (zip).

**Opcja B – lokalnie:** zainstaluj Visual Studio 2022 Community (pakiet „Desktop development with C++”) i Git, potem kliknij dwa razy `build_windows.bat`.

## Instalacja w FL Studio
1. Skopiuj cały folder `Prophecy of Scream.vst3` do `C:\Program Files\Common Files\VST3\`.
2. FL Studio → *Options → Manage plugins* → **Find installed plugins**.
3. Dodaj jako generator (Channel rack → + → Prophecy of Scream).

## Tor sygnału
`unison osc (saw↔square + FM)` → `BITE (HP)` → `DISTORTION` → `FILTER` → `AMP` *(wszystko 4× oversampling)* → `DELAY` → `REVERB` → `MASTER (soft clip)`

## Pokrętła
| Sekcja | Pokrętło | Co robi |
|---|---|---|
| OSCILLATOR | WAVE | płynne przejście piła → kwadrat |
| | UNISON / DETUNE / WIDTH | 1–7 głosów, rozstrojenie w centach, szerokość stereo |
| | FM / RATIO | phase-modulation – to robi „krzyk”; ratio 0.5–7 |
| PITCH | GLIDE | portamento między nutami (legato) |
| | P.ENV / P.DECAY | obwiednia pitchu ±48 st – klasyczny „zjazd” screecha |
| | BEND | zakres pitch-wheela (do ±24 st) |
| | LEGATO | nuty nachodzące nie restartują obwiedni (slide'y) |
| DISTORTION | TYPE | HARD / TUBE / FOLD / SCREAM |
| | DRIVE / BITE / MIX | gain przesteru, high-pass przed przesterem (więcej „gryzienia”), dry/wet |
| FILTER | TYPE | LP12 / LP24 / BAND / HIGH |
| | CUTOFF / RESO / ENV / DECAY / KEY | odcięcie, rezonans, obwiednia filtra (± oktawy), keytracking |
| LFO | SHAPE / SYNC / RATE / DIV | sine/tri/saw/square/S&H, sync do tempa FL |
| | > CUTOFF / > PITCH | ile LFO rusza filtrem i pitchem („gadający” screech) |
| THE SCREAM | SCREAM | makro: +FM, +drive, +rezonans naraz. **Mod wheel też go podkręca** |
| AMP | A / D / S / R | obwiednia głośności |
| DELAY | TIME / FDBK / MIX / P.PONG | delay zsynchronizowany z tempem, ping-pong |
| REVERB | SIZE / DAMP / MIX | pogłos (z high-passem, żeby nie mulić dołu) |
| MASTER | VOLUME | wyjście + oscyloskop |

Dwuklik na pokrętle = wartość domyślna. Okno można skalować za róg.

## Presety
Fabryczne: Prophecy Lead, Raw Scream, Talking Screech, Siren Riser, Uptempo Laser, Euphoric Scream, Frenchcore Rip, Hoover Scream, **Dutchie** (ładuje się domyślnie przy nowej instancji).

- **SAVE** – zapisuje obecne brzmienie jako Twój preset (`Dokumenty\Prophecy of Scream\Presets\*.pos`). Twoje presety pojawiają się na liście w sekcji USER. Żeby usunąć preset, skasuj jego plik z tego folderu.
- **RANDOMIZE** – losuje nowy screech w rozsądnych zakresach (głośność zostaje bez zmian).

## Tip
Screeche najlepiej grać monofonicznie z nachodzącymi nutami (legato + glide) i ustawić pitch-bend w pianorollu FL.
