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
1) Echelon-struktur (1-10), velges i karaktervalg. Neste låses opp ved å slå bossen
2) Timer teller ned til boss (10 min på E1, lenger for dypere echelons)
3) Teleport til boss-arena når timeren er ferdig + enkel boss (jage + dash)
4) TODO: echelon-modifikatorer (f.eks. E3 slow on hit, E4 mindre XP)

g++ tlj.cpp src/*.cpp -Iinclude -Iraylib/src -Lraylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXinerama -lXcursor -o tlj