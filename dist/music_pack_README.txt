Replacement Soundtrack (Music Pack)
===================================

Drop your own music files in this folder to replace the in-game soundtrack.

  Supported formats : .ogg (Ogg Vorbis) and .wav (OGG strongly recommended)
  File naming       : track_<id>.ogg   e.g. track_1.ogg, track_10.ogg

NOTE: Track 0 is reserved by the sound driver for silence, room transitions,
and sound stopping. Do NOT name any music track_0.ogg.

Finding or Verifying Track IDs:
-------------------------------
1. Open the in-game menu (F10 on desktop, the gear icon on Android).
2. Go to the "HD Pack" tab and enable "Enable Music Pack".
3. The menu always displays the current game track ID and track name under:
   "Current Game Track: ID <N> (<Name>)".
4. Name your replacement file "track_<N>.ogg" (or .wav).
5. Click "Reload Music Pack" to apply changes immediately without restarting.

Track ID Index (Resident Evil Gaiden):
--------------------------------------
ID  | In-Game Track Name / Scene
----+---------------------------------------------------------------
0   | [Silence / Stop / Room Transitions] (Not a playable track)
1   | Battle
2   | Introduction (Part 1 - Capcom & Legal disclaimers)
3   | Introduction (Part 2 - Virgin Interactive & M4)
4   | Introduction (Part 3 - In-game Intro Cutscene)
5   | Restaurant
6   | Restaurant (Alternate Loop)
7   | Restaurant X
8   | Prologue / Cutscenes
9   | [Unused / Silence]
10  | Title Screen
11  | Side Deck
12  | Side Deck X
13  | Main Hall X (Variation)
14  | Side Deck X (Mild Drums)
15  | Main Hall
16  | Main Hall (Slower, No Bass)
17  | Main Hall X
18  | Title (Battle Version)
19  | Zombie Attack
20  | Class Cabin
21  | Class Cabin X
22  | Class Cabin (With Bass)
23  | Corridor
24  | Corridor X
25  | Corridor X (Variation)
26  | Corridor X (No Drums, Slower)
27  | Class Cabin (With Bass & Drums)

How It Works:
-------------
Tracks follow the game automatically, so the right music plays in the right
place, and any id you have not supplied a file for keeps the original music.

Because Game Boy music and sound effects share the same audio channels, the
original audio is turned down rather than muted while your music plays, so
sound effects (gunshots, doors, menu blips) still come through. Adjust this
with the "Game Audio While Music Plays" slider in the in-game menu.

Seamless Loop Points:
---------------------
Custom tracks can loop seamlessly from a designated loop point (skipping the track's
introductory part upon loop repetitions). Loop points can be defined in three ways:

1. Embedded in WAV files (smpl chunk):
   Standard sampler loop points exported from digital audio workstations (DAWs) such
   as FL Studio, Reaper, Wavosaur, Audacity, GoldWave, etc., are detected automatically.

2. Embedded in OGG Vorbis comment tags:
   Add any of the following standard Vorbis comment tags using any audio tagger:
   - LOOPSTART=<samples_or_seconds>  (e.g. 145200 or 3.292s or 00:03.292)
   - LOOPEND=<samples_or_seconds>    (optional, defaults to end of track)
   - LOOPLENGTH=<samples_or_seconds> (alternative to LOOPEND)
   Tags with underscores (LOOP_START, LOOP_END, LOOP_LENGTH) are also recognized.

3. External music_pack.ini (or track_<id>.ini):
   Create a "music_pack.ini" text file inside this folder without needing any audio editor:

   [track_10]
   loop_start = 3.292s
   loop_end = 85.5s

   [track_1]
   loop_start = 145200

   Values can be specified in seconds (e.g. 3.5s, 01:15.5) or raw sample counts.
   When loop points are active, the in-game menu (F10) displays the loop range!

No music is included with this project. These files are yours to supply,
exactly like the ROM.
