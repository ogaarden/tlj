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

// 7 oktober – builds, items og evolusjoner
1) Items (items.cpp): 10 passive gjenstander med 5 nivåer hver, som velges i level-up.
   Maks 6 items per runde, så man må velge. Nullstilles hver runde.
   Sjonglørball (+prosjektil), Kongens kappe (+område), Narreskoene (+fart), Slipestein (+skade),
   Timeglass (-cooldown), Hjerteamulett (+HP/regen), Ringbrynje (+armor), Magnetstein (+pickup),
   Uglefjær (+XP), Heldig terning (+krit)
2) Evolusjoner: ability på level 9 + riktig item -> neste skattekiste gjør den til en superversjon.
   Trefork+Sjonglørball=Poseidons trefork, Ground Slam+Ringbrynje=Jordskjelv,
   Ricochet+Terning=Kaoskule, Kakekast+Kappe=Bryllupskake, Magic Missile+Uglefjær=Stjerneregn,
   Rot+Hjerteamulett=Svartedauden, Dagger+Narreskoene=Tusen kniver,
   Orbit Blades+Slipestein=Stålvirvel, Lightning+Timeglass=Tordenguden.
   Kortene viser kombo-hint, HUD-en viser "KLAR!", og pausemenyen viser oppskriftene.
3) Ny XP-kurve (35 + 35L + 4L^2): første level innen 30 sek, ca. level 25 ved 10 min.
   Et fullt build krever 70+ valg, så man må satse. Level-up gir +5 maks HP og 20 % liv
   (ikke full heal lenger). Flere level på én gang gir hvert sitt valg.
4) 3 rerolls per runde ([R] i level-up)
5) XP-krystaller i fire tiers (blå, grønn, rød, lilla) som spretter ut, glitrer og suges inn
   raskere og raskere. Tonen stiger når man plukker mange på rad.
6) Sjeldne drops: magnet (suger inn all XP) og kyllinglår (+30 % liv)

// 8 oktober – dobbelt så mange abilities, items fra kister
1) Level-up gir nå BARE abilities (nye og oppgraderinger). Items finnes bare i skattekister.
   Kista gir evolusjon hvis en ability er klar, ellers 3 item-valg. Er alle items fulle,
   gir den en gratis ability-oppgradering i stedet.
2) Flere kister: en kiste dukker opp i slottet hvert 80. sek (første etter 40 sek),
   hver horde har en elite-kaptein med kiste, og elites slipper kister (felles nedkjøling
   på 22 sek så det ikke regner kister sent i runden). Gule piler i skjermkanten peker mot kister.
3) 9 nye abilities (18 totalt), i src/weapons_extra.cpp:
   Ildsluker (ildkjegle), Bumerang, Kortstokk (kort i alle retninger), Frostnova (bremser fiender),
   Katapult (steiner fra himmelen), Narrebjeller (lydringer som dytter), Rampelys (roterende
   lysstråler), Sabelhugg og Virvelvind (vandrende virvler som suger inn fiender)
4) 8 nye items (18 totalt): Vampyrtann (liv per drap), Piggkrage (torner), Kronjuvel (+gull/XP),
   Skyggekappe (+dodge), Kikkert (+prosjektilfart), Evighetslys (+varighet),
   Trollspeil (+1 reroll), Firkløver (+flaks)
5) 9 nye evolusjoner: Ildsluker+Evighetslys=Drakepust, Bumerang+Kikkert=Stormbumerang,
   Kortstokk+Trollspeil=Full kortstokk, Frostnova+Skyggekappe=Evig vinter,
   Katapult+Kronjuvel=Kongelig bombardement, Narrebjeller+Piggkrage=Dommedagsklokker,
   Rampelys+Firkløver=Primadonna, Sabelhugg+Vampyrtann=Blodsabel, Virvelvind+Magnetstein=Malstrøm
6) 6 ability-slots (1 innate + 5). Fiender kan nå bremses (frost) og dyttes (bjeller, bumerang, torner).

// 9 oktober – minibosser, Kongens septer og item-kombinasjoner
1) Minibosser ved 3, 6 og 9 min (miniboss.cpp). En lilla innkallingssirkel dukker opp et sted i slottet
   (lilla prikk på kartet og pil i skjermkanten). Går du inn i sirkelen, stiger en tilfeldig miniboss opp:
   - Bøddelen: treg, men løfter øksa og knuser bakken rundt seg (rød sirkel = kom deg unna!)
   - Hoffmagikeren: holder avstand, skyter ringer av magiske kuler og teleporterer
   - Jernridderen: sikter (rød linje) og stormer gjennom deg
   De er svært sterke (12 000 / 28 000 / 50 000 HP) og har egen HP-bar øverst.
2) Minibossene slipper Kongens septer (som Aghanim's Scepter i Dota) + en kiste og mye XP.
   Septeret lar deg velge hvilken ability som får sin septer-oppgradering (Poseidons trefork,
   Blodsabel, Malstrøm ...). Hver ability kan få septeret én gang. Dette erstatter evolusjonene:
   abilities kombineres IKKE lenger med items.
3) Items kombineres med hverandre: to items som hører sammen, begge på nivå 5, kan smeltes sammen
   i en skattekiste. Kombinasjonen tar bare én plass (frigjør en plass!) og gir en ekstra bonus:
   Slipestein+Terning=Bøddelens øks (krit 3x), Sjonglørball+Kikkert=Sjonglørens kikkert,
   Kappe+Kronjuvel=Kongens regalier, Narresko+Skyggekappe=Skyggedanser,
   Timeglass+Evighetslys=Evighetens timeglass, Hjerteamulett+Vampyrtann=Blodhjerte,
   Ringbrynje+Piggkrage=Piggrustning, Magnetstein+Uglefjær=Visdommens magnet,
   Trollspeil+Firkløver=Lykkespeilet. Item-kortene viser hvilket item de kombineres med.

// 10 oktober – tegnede figurer og porselensgulv
1) Alle klovner, fiender, minibosser og kongen er tegnet med Higgsfield (assets/sprites/) og vises som
   "papirfigurer" i 3D-verdenen (sprites.cpp), som i Paper Mario: de vender alltid mot kameraet,
   speilvendes etter hvilken vei de går, hopper og vugger når de går, blinker hvitt når de blir truffet
   og stiger opp av gulvet når de spawner. Bakgrunnen ble fjernet med et eget chroma key-verktøy.
2) Wester er nå en original sirkus-strongman (den første tegningen ble for lik Wario).
3) Porselensgulv (assets/floor/): hvite delft-fliser speilet i 2x2 til store rosetter, med koboltblå
   stjernefliser i bånd mellom. Begge ligger i én tekstur, så gulvet tegnes i én batch.
4) Innstillinger -> Figurer: bytt mellom TEGNET og de gamle 3D-MODELLENE (lagres).

// 11 oktober – fiendehav og en mye farligere konge
1) Mange flere fiender sent i runden: samme rolige start, men wave-størrelsen har fått et kubisk ledd
   (ca. 130 fiender i wave 10 og 750 i wave 20, dvs. over 20 i sekundet). Tak på 800 samtidig.
2) Horder hvert minutt fra 1:45 (før: hvert 2. minutt), større og tøffere: soldater og troll fra 3 min,
   armbrøstskyttere fra 5 min, og to ringer etter 7 min. Hver horde har fortsatt en elite-kaptein med kiste.
3) Mindre XP per fiende jo senere det er (xpScale i spawner.cpp): hver wave gir omtrent like mye XP som før,
   selv om det er mange flere fiender. Hordefiender gir halv XP. Krystaller som ligger tett slås sammen.
4) Kongen (150 000 HP) har to faser:
   - Fase 1, PÅ TRONEN: spiraler av septerkuler, vifter rett mot deg, kongelige dekreter
     (røde sirkler som eksploderer) og vakter (soldater og armbrøstskyttere).
   - Under 55 %: KONGEN REISER SEG! Hopper ned med en sjokkbølge og JAGER deg: tre storminger på rad,
     tramp med stor eksplosjon og kulering, kuler bak seg etter hver storming. Rustning i denne fasen.
   - Under 25 %: KONGEN ER RASENDE! Raskere, flere vakter og spiraler mens han jager.

// 12 oktober – pikselkunst og søyler
1) Figurene er gjort om til pikselkunst (ca. 1.5 verdensenheter per piksel): nedskalert, færre farger,
   hard kant og 1 piksel mørk kontur, tegnet med skarpe piksler (TEXTURE_FILTER_POINT). Passer bedre
   med pikselfonten og porselensgulvet enn de glatte tegningene.
2) Marmorsøyler med gullringer i storsalen: søylerekker langs løperne og en firkant midt i hver sal.
   Spilleren og fiendene kan ikke gå gjennom dem (fiender glir rundt), prosjektiler går forbi.
   Søylene vises som prikker på minimapet, og kister havner aldri inni en søyle.

// 13 oktober – ny figurstil og bærende søyler
1) Alle figurer er tegnet på nytt med Higgsfield i ÉN felles stil: malte papirteater-dukker med tykk
   mørk blekkontur, flate farger og samme palett som slottet (koboltblått, krem, karmosin og gull).
   Klovnene, vaktene og minibossene/kongen ble tegnet i to ark (det andre med det første som
   stilreferanse), så alle ser ut som de hører til i samme spill. Fiendene bærer kongens
   karmosin-og-gull-livré. Hver figur har en kremhvit papirkant, så de synes godt både på de blå
   flisene og på de røde løperne. Tegnes nå mykt (mipmaps) i stedet for som pikselkunst.
2) HUD-portrettet viser ansiktet til klovnen (ikke hammeren eller treforken).
3) Søylene er mye sjeldnere (ett par midt på hver løper, 4 per sal i stedet for 16), tykkere og går
   helt opp til taket, ut av bildet. Står en søyle mellom kameraet og spilleren, blir den gjennomsiktig.

// 14 oktober – gange, balanse, nye fiender og kongelige dekreter
1) Figurene går ordentlig: beina svinger vekselvis (bildet deles mellom føttene), kroppen hever seg
   i takt med stegene, og kapper/kjoler svinger. Når en figur bytter retning, snur den seg som en
   papirfigur (smalner inn og vider ut igjen) i stedet for å speilvendes med et rykk.
2) Mindre gull: mynter skaleres ned med antall fiender (akkurat som XP), lavere sjanse per fiende,
   elites gir mindre, og 6 g per minutt / 1 g per level. Shoppen er ca. 70 % dyrere
   (høyere startpris og prisen øker x1.5 per nivå). Å kjøpe alt tar nå 50+ gode runder.
3) Brattere XP-kurve: ca. level 24 ved 10 min, 30-35 i de lengste rundene (før: 40-50+).
   Level-up helbreder 12 % (før 20 %).
4) Vanskeligere: fiendene har x6 HP ved 10 min (før x4), dobbel skade (før x1.6), litt raskere,
   større waves og elites fra 1:30 (opptil 14 %).
5) Alle prosjektiler går 30 % saktere (levetiden er lengre, så rekkevidden er omtrent lik).
6) Fire nye fiender (tegnet i samme stil):
   - Kongens hunder (fra 2 min): kommer i flokker på 4-5, kryper sammen (rød pil) og kaster seg mot deg
   - Hoffprest (fra 3 min): holder avstand og helbreder alle fiender rundt seg (gyllen ring). Drep ham først!
   - Trommeslager (fra 3 min): alle rundt ham går 40 % fortere. Er også med i de sene hordene.
   - Kanonér (fra 4 min): lobber granater som lander der du står – rød sirkel på gulvet
7) Kongelige dekreter: kl. 2:30 og deretter hvert 2. minutt leser kongen opp et dekret som varer i 30 sek:
   Kongens fest (raskere fiender, dobbel XP), Blodmåne (fiendene slår hardere, mye mer gull),
   Den store jakten (tre hundeflokker) og Mobilisering (dobbelt så mange fiender, +50 % XP).

// 15 oktober – tilbake til 3D-modellene
1) 3D-modellene (klovnene og fiendene bygget i kode) er standard igjen. De tegnede papirfigurene
   finnes fortsatt under Innstillinger -> Figurer (TEGNET). Gamle lagringer starter også med 3D.
2) De fire nye fiendene har fått egne 3D-modeller i samme stil som de andre:
   hund på fire bein med dekken, hoffprest med kjortel og røkelseskar, trommeslager med shako og tromme
   (stikkene slår i takt), og kanonér med skjegg og bronsemorter som løftes når han sikter.

// 16 oktober – skattmesteren, to nye dekreter og echelon-bonus
1) Skattmesteren: kl. 3:15 og deretter hvert 3. minutt dukker en feit skattmester med flosshatt og
   pengesekk opp i nærheten ("SKATTMESTEREN ER HER!", gyllen pil i skjermkanten). Han løper fra deg,
   snubler i sekken (og mister en mynt) og rømmer etter 25 sek. Tar du ham, sprekker sekken:
   16 mynter (32 g), en skattekiste og mye XP. Han påvirkes ikke av prest, trommeslager eller Kongens fest.
2) To nye kongelige dekreter:
   - Mørklegging: alt utenfor en lyssirkel rundt deg blir mørkt, men +50 % XP
   - Gullregn: mynter faller ned rundt deg, men nye fiender har +40 % HP
   Blodmånen har fått et tydeligere rødt skjær.
3) Echelon-bonus på gull: +15 % gull per echelon over 1 (E5 = x1.6, E10 = x2.35). Vises på game over-skjermen.

// 17 oktober – dyr shop og bærende søyler
1) Shoppen blir veldig dyr mot toppen: pris = grunnpris * 1.55^nivå * (1 + 0.18 * nivå²).
   En oppgradering til 90 g koster 90, 165, 372, 878 og 2016 g. Alt til sammen: ca. 43 000 g
   (før ca. 16 000). De første nivåene er rimelige, men å kjøpe ALT tar veldig mange runder –
   og da skal spillet også være ganske enkelt.
2) Søylene står nå rett opp fra gulvet og går opp mot taket. Før vippet de kraftig utover mot kanten
   av skjermen (perspektivet), så de så ut som de lå på gulvet. Nå vippes hver søyle litt sidelengs
   inn mot kameraet, så den ser loddrett ut på skjermen (og blir bredere oppover, mot kameraet).
   Søyler som står nesten rett foran kameraet tones ut nederst på skjermen, og en søyle som skjuler
   spilleren blir gjennomsiktig.

// 18 oktober – pynt i stedet for søyler
1) De høye søylene er byttet ut med lav pynt som står rett opp fra gulvet (som fyrfatene), så den
   følger gulvets perspektiv: delftvase (hvit med koboltblå bånd) på marmorsokkel med gullkant,
   rustning på rund sokkel (karmosin våpenkjole, fjærbusk og hellebard) og klippet hekk i steinkrukke.
   Hvilken som står hvor, bestemmes av posisjonen. Samme plasser som søylene (et par på hver side
   av løperne), og man kan fortsatt ikke gå gjennom dem. Står en foran spilleren, blir den gjennomsiktig.

// 19 oktober – et ferdig slott med faste saler
1) Slottet er ikke lenger uendelig: det er 4 x 4 saler (ca. 6100 x 6100) med en murvegg rundt
   (murtinder og vegglykter). Fiender, kister, skattmesteren og miniboss-sirkler holdes innenfor
   muren, og minimapet viser veggen. Utenfor muren er det mørk stein.
2) Hver sal har et tema med faste plasser for pynten og sitt eget gulv:
   - Storsalen (de fire i midten, der du starter): marmorstatue av dronningen og fire fanestativer
   - Statuegalleriet: to rekker med konge- og ridderstatuer langs en rød løper
   - Rustkammeret: skifergulv, rustninger på rad og våpenstativer med spyd og skjold
   - Fanehallen: to lange rekker med karmosin og koboltblå faner langs en blå løper
   - Hagegården: fontene med vann som glitrer, gressplen, grusganger og klippede hekker
   - Porselenssalongen: stort rundt delftteppe med delftvaser i en rombe
   Annenhver sal er dreid 90 grader, så like saler ikke ser helt like ut.
   Navnet på salen vises kort når du går inn i den.
3) Nye modeller: statuer på trinnet marmorsokkel, fanestativ med frynser og kronemerke,
   våpenstativ og fontene. All pynt er litt større enn før.

// 20 oktober – detaljerte gulv i salene
1) Salene har fått egne teksturer (assets/floor/, laget med tools/gen_room_textures.py – kjør det
   på nytt for å justere dem). Alle flis-teksturene er sømløse:
   - Hagegården: gress med strå og små blomster, grusganger av småstein, steinkant og blomsterbed
     med masse blomster rundt hver hekk
   - Rustkammeret: skiferheller med fuger, sprekker og slitasje, jernramme med nagler og
     et skjold med kryssede sverd i midten
   - Statuegalleriet og Fanehallen: vevde løpere med rutebord og kronemedaljonger / liljer
   - Porselenssalongen: stort malt delftteppe med skjellbord, blomsterkrans og stjerne
   - Storsalen: innlagt marmormedaljong med kompassrose og marmorårer
2) Teksturene ligger fast på gulvet (verdenskoordinater), så de henger sammen på tvers av formene.

// 21 oktober – kistenivåer og salbonuser
1) Tre nivåer skattekister:
   - Trekiste (vanlig): 1 skatt
   - Sølvkiste (hordekapteiner, og 20 % av kistene som dukker opp i slottet – mer med Firkløver): 2 skatter på rad
   - Gullkiste (minibosser og skattmesteren): 3 skatter på rad, større med juvel og gnister
   Større kister har høyere lyssøyle. Valgskjermen viser "Skatt 2 av 3" osv.
2) Salbonuser: hver sal gir en fordel så lenge du står i den, så det lønner seg å flytte seg rundt:
   Storsalen +50 % plukkeradius, Statuegalleriet +10 % krit, Rustkammeret +15 % skade,
   Fanehallen +15 % fart, Hagegården +2 HP/sek, Porselenssalongen +25 % XP.
   Salen og bonusen vises alltid under klokka.
3) Løperne har et rolig geometrisk mønster i stedet for kroner/liljer.
4) Skyggen under fontenen ligger nå konsentrisk med kanten.

// 22 oktober – sjeldne items (Crownfall-stil)
1) Items er MYE sjeldnere, ca. 20 skatter i en hel runde (før 60+). Du må velge hvilke par du satser på:
   en kombinasjon krever begge items på nivå 5 + en kiste til = 11 skatter, så man rekker 1-2 i en runde.
   - Kister i slottet: første etter 75 sek, så hvert 150. sek (før 40/80). 10 % sølvkiste (før 20 %)
   - Hordekapteiner: bare annenhver horde (ca. hvert 2. min) har kiste, og det er en trekiste (før sølv hver gang)
   - Vanlige elites: 8 % sjanse for trekiste med 90 sek felles nedkjøling (før alltid, 22 sek)
   - Minibosser slipper sølvkiste (før gull). Skattmesteren er den eneste med gullkiste
   Firkløver gjør fortsatt alt dette oftere.
2) 6 nye items (24 totalt, bare 6 plasser): Helgenrelikvie (regen + armor), Rosenkrans (-cooldown + varighet),
   Krigstromme (fart + skade), Fekthanske (krit + prosjektilfart), Tiggerskål (+gull), Narremaske (XP + område)
3) 3 nye kombinasjoner (12 totalt): Relikvie+Rosenkrans=Katedralens velsignelse (+1 aegis, +1 HP/s),
   Tromme+Fekthanske=Kavaleriets marsj (fart, skade og krit), Tiggerskål+Narremaske=Gatekunstnerens hatt
   (+40 % gull, +15 % XP, +1 reroll)

g++ tlj.cpp src/*.cpp -Iinclude -Iraylib/src -Lraylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXinerama -lXcursor -o tlj