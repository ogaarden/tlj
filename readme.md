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

g++ tlj.cpp src/*.cpp -Iinclude -Iraylib/src -Lraylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXinerama -lXcursor -o tlj