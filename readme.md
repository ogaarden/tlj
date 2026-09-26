##
It does not matter how chad y are
when the jester begins
even the king is watching


Notes:
// 21 september
1) Shop menu
2) enemies spawn
3) xp drop

// 22 september
1) pick up xp drop
2) kunne level opp
3) ability på levelopp

//23 september
1) shop effekt påføres spiller
2) projectile count skal ha effekt

// 24 september

1) Projectile count søker ulike fiender
2) Støtte ulike type våpen
3) Når spiller levler opp, skal ability menu komme

// 25 september
1) Ricochet ability (spretter videre, svakere per sprett)
2) Rot ability (AOE aura, lav skade hver frame)
3) Damage numbers over fiender
4) Hver karakter har en unik (innate) ability + 4 ledige ability slots
5) HUD med 5 ability slots (mørke til de låses opp)
6) Abilities går fra level 1-9 med forhåndsbestemte oppgraderinger (abilities.cpp)
7) Nye abilities: Magic Missile, Dagger, Orbit Blades, Lightning

// 26 september
1) Fiender gjør kontaktskade, spilleren kan dø (Aegis = ekstra liv)
2) Gull som metaprogresjon: sjeldne mynter + bonus for tid og level
3) Game over-skjerm med oppsummering av gull
4) Shop med 14 oppgraderinger, lagres i save.txt mellom økter

// 27 september
1) Echelon-struktur (1-10). Neste låses opp ved å slå bossen
2) Teleport til boss-arena når tiden er nådd + enkel boss (jage + dash)

// 28 september
1) Echelon-meny etter karaktervalg (låste echelons er mørke)
2) Klokka teller alltid opp; boss på 10:00 + 30 sek per echelon (= én ny wave)
3) Unike echelon-effekter som stacker:
   E1 basic, E2 kamikaze, E3 slow ved treff, E4 +25% fiendeskade, E5 curse,
   E6 raskere fiender, E7 -25% XP, E8 ingen regen, E9 +30% fiende-HP, E10 ekstra curse
4) Kamikaze-fiende (Exploder) som eksploderer ved død
5) Curses: Frailty, Sluggish, Famine, Blunt, Brittle, Myopia

// 29 september
1) Slottsgulv: rutete marmor med fuger og røde løpere (castle.cpp)
2) Boss-arenaen er nå kongens tronsal med trone, søyler og bannere
3) Bossen er Kongen: kappe, hermelin, krone, skjegg og septer
4) Skygger under alle figurer (første steg mot 3D-look)
5) 2.5D: skrått 3D-kamera, gulvet tegnes som tekstur på et 3D-plan,
   figurer/prosjektiler/tronsal i ekte 3D med belysning (render3d.cpp)

// 30 september
1) Kjedelyn: Lightning hopper videre til nye fiender, svakere for hvert hopp
2) Lydeffekter (syntetisert i audio.cpp): XP, gull, treff, kills, skade,
   level up, lyn, eksplosjon, gong i tronsalen, Royal Charge, seier, død, menyer
3) Volumkontroll under Settings (lagres i save.txt)
4) Kamera mer ovenfra (72 grader) med smal linse = nesten flatt 2D-perspektiv
5) 3D-klovner: Jester (sirkusklovn), Wester (feit Wario-klovn), tok geek (lang nerd med briller)
6) Karaktervalg viser klovnene roterende i 3D

// 1 oktober
1) Nytt hovedmeny-look: slott i skumring bak logoen (tårn med spisse tak, flagg,
   rosevindu, lys i vinduene, måne, skyer og ildfluer) – tegnet i kode (ui.cpp)
2) Ny logo: "THE LAST" smått over et stort, bølgende "JESTER" med narrelue på J-en
3) Alt skalerer med vinduet: menyer tegnes på et 1280x700-lerret som skaleres,
   HUD festes til kantene. [F11] = fullskjerm (kantløst vindu)
4) Samlet HUD (hud.cpp): XP-bar over hele toppen, spillerpanel med 3D-portrett,
   HP, level, aegis/gull/kills, klokke-plakett med nedtelling til Kongen, boss-bar
5) Minimap oppe til høyre: fiender, gull, løperne på gulvet, kompass og Kongen.
   Roterer med kameraet (Q/E), [M] skjuler/viser kartet
6) Alle menyer bruker samme stil: slottet dempet i bakgrunnen og paneler med gullkant

// 2 oktober
1) Fikset "rare" 3D-figurer: ellipsoider og sylindre ble tegnet vrengt (man så
   innsiden av dem). Nå vender alle trekanter utover (render3d.cpp)
2) Jester har fått ekte narrelue med tre tupper og bjeller; Wester og tok geek har fått hår
3) Ny shop: grid med kort (ikon, nivåprikker, pris) + detaljpanel med nå/neste-effekt
   og kjøpsknapp. Navigeres med WASD
4) Ny level-up: liggende kort som glir inn, fargebanner, ikon, NY!/LV-merke og nivåprikker
5) Egne ikoner for alle abilities og shop-oppgraderinger (icons.cpp), også i HUD-en

// 3 oktober
1) VFX-system (vfx.cpp): additiv glød, lyn, sjokkbølger, gulv-decals og partikler
2) VFX-teksturer generert med Higgsfield (assets/vfx/): lynstrek, elektrisk nedslag,
   magisk kule, ildeksplosjon, gyllen sjokkbølge, gifttåke, sverdhugg og gnist.
   Laget på svart bakgrunn og tegnet additivt, så svart blir usynlig.
   Mangler en fil, lager spillet en enkel erstatning i kode.
3) Alle våpen har nye effekter: glødende prosjektiler med spor, lyn fra himmelen,
   sjokkbølge med støv, roterende gifttåke med bobler, hugg-buer på orbit-bladene
4) Gnister ved treff, lysglimt ved drap, ildkule ved kamikaze, glød rundt XP og gull

// 4 oktober
1) Kameraet er zoomet ut (FOVY 20 -> 28), fiender spawner lenger unna (950)
2) Vanskelighetsgrad (spawner.cpp): rolig start (~13 fiender første halvminutt),
   så kvadratisk vekst (~100 i wave 10, ~300 i wave 20). Fiendene får mer HP (x4 ved 10 min),
   skade og fart jo lenger runden varer. Elite-fiender fra 2 min (større, x4 HP, gyllen aura).
   Horde hvert 2. minutt som omringer deg. Maks 500 fiender samtidig.
3) Nye fiendemodeller: soldat med skjold og fjærbusk, ogre med klubbe og støttenner,
   ond mini-narr med kniv, bombe med sinte øyne. Gangeanimasjon, hvitt treffglimt,
   stiger opp av gulvet når de spawner
4) Klovnene: kantlys (rim light), Jester har narrestav og narredrakt, Wester har en
   stor hammer han slår i bakken, tok geek har ryggsekk og sprettball
5) Standardvåpen: treforken er en ekte 3D-trefork med munningsglimt, Ricochet lager
   elektriske buer mellom sprettene, Ground Slam rister skjermen
6) Skjermristing ved slag, eksplosjoner, lyn og når du tar skade
7) Ytelse: 3D-formene bruker ferdig utregnede tabeller, og detaljnivået senkes
   automatisk når det er mange fiender (SetShapeDetail)

// 5 oktober
1) Pausemeny (ESC/P): Fortsett eller Gi opp, med oversikt over alle stats.
   Før avsluttet ESC runden med en gang!
2) Ny armor-formel: armor / (armor + 30). 8 armor = -21 % skade, 23 = -43 %, maks -75 %.
   Før ga 8 armor bare -7 %. Prosenten vises i karaktervalget og pausemenyen.
3) Stat-oppgraderinger i level-up (maks 5 av hver): maks HP, fart, skade, cooldown,
   område, magnet, armor og XP. Alltid minst ett stat-valg.
4) Skattekister: elite-fiender slipper en kiste med lyssøyle -> gratis oppgraderingsvalg
5) Fiendene har fått riktige roller: ogren (Goon) er tank med 320 HP, soldaten 110
6) Slottsgulvet: gyllen kompassrose der løperne krysser, fyrfat med flammer og lys
7) Tronsalen: fyrfat langs muren og emblem i midten. Kongen blir RASENDE under 50 % HP:
   raskere, kortere pauser mellom dashene og kaller inn lakeier

// 6 oktober
1) Ny karakter: Pierrot (hvit mime-klovn). Standardvåpen Kakekast: kremkaker lobbes i en
   bue, spruter i et område og lar klissete krem ligge igjen (skade over tid)
2) Ny fiende: armbrøstskytter (fra 2 min). Holder avstand, sikter med rød linje på
   gulvet i et halvt sekund og skyter en glødende pil – flytt deg!
3) Kritiske treff: 5 % sjanse for 2x skade, store gule tall med "!".
   Ny stat-oppgradering: Presisjon (+5 % per nivå)
4) Musikk (music.cpp), laget i kode: hoffnarr-vals i menyen, drivende spor i spillet og
   mørk bossmusikk i tronsalen. Krysstoner mellom sporene. Eget musikkvolum i innstillinger
5) Fyrverkeri over slottet i menyen

g++ tlj.cpp src/*.cpp -Iinclude -Iraylib/src -Lraylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXinerama -lXcursor -o tlj