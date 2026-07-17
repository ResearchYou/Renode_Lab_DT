# ghosttag apocalypse - ghid operational si pentru studenti

## scop

ghosttag apocalypse este un challenge de firmware, securitate, persistenta si
simulare ble. studentii implementeaza protocolul unui tag nrf52840 care trebuie
sa ramana identificabil pentru gateway-uri autorizate fara sa emita un id
stabil.

challenge-ul adauga trei probleme de sistem peste protocolul radio:

- cheia se schimba la fiecare epoch printr-un ratchet;
- o cadere de tensiune nu trebuie sa reutilizeze un epoch;
- un replay valid criptografic trebuie respins;
- scrierile si erase-urile flash trebuie sa ramana in bugetul energetic.

durata recomandata este 8 ore. nu este un exercitiu de 2-3 ore si nu este un
protocol complet pentru productie.

## status

statusul final trebuie verificat dupa fiecare publicare:

| componenta | stare asteptata |
|---|---|
| branch | `nrf52840-swarm/ghosttag-apocalypse` |
| starter | 6 todo-uri, compileaza, dar pica testele |
| referinta | trece testele native, zephyr si renode |
| runtime student | nu expune `reference/` sau helperul privat de seed in explorer ori workspace |
| runtime showcase | contine seed-ul de referinta separat |
| scenariu live | `nrf52840-swarm-ghosttag-apocalypse` |
| conturi | `initial` si `user1..user9` |
| workspace | exact `firmware/`, `problem/`, `renode/` |

valorile exacte pentru commit, digesturi si acceptanta live sunt in sectiunea
`ultima acceptanta`.

## ce este deja implementat de organizatori

### platforma

- ide in browser cu explorer, editor, terminal, buton run si panou results;
- workspace persistent separat pentru fiecare user;
- job kubernetes separat pentru fiecare rulare;
- imagine offline cu zephyr, sdk arm si renode;
- gateway trusted si validator nemodificabile;
- raport `output/report.html` si rezultat `validation.json`;
- scenariu renode determinist si fara acces la internet.

### suprafata studentului

studentul vede exact:

```text
firmware/
problem/
renode/
```

rolurile sunt:

- `firmware/` - codul starter si testele publice;
- `problem/` - enuntul complet;
- `renode/` - scripturile publice folosite pentru topologie si rulare.

studentul modifica numai:

```text
firmware/ghost_protocol.c
```

`main.c`, header-ul, `prj.conf`, cmake, gateway-ul si testele complete sunt
contracte. o modificare a lor produce exit 2.

### scenariul automat

o rulare normala contine:

| rol | numar |
|---|---:|
| tag-uri autorizate | 6 |
| gateway-uri | 3 |
| clona cu seed neautorizat | 1 |
| atacator replay | 1 |
| total masini nrf52840 | 11 |

secventa renode este:

1. initializeaza jurnalul fiecarui device ca flash erased;
2. porneste tag-urile, gateway-urile si clona;
3. ruleaza 3 secunde virtuale;
4. reseteaza tag-ul 1 fara sa stearga jurnalul;
5. porneste atacatorul cu un pachet autorizat capturat la epoch 0;
6. ruleaza inca 5 secunde;
7. valideaza logurile uart.

### payload v3

payload-ul ble are exact 28 bytes:

| bytes | continut |
|---|---|
| `0..1` | company id `0xf00d` |
| `2` | versiune `3` |
| `3` | flags |
| `4..7` | epoch persistent, little-endian |
| `8..11` | sector public, little-endian |
| `12..19` | eid derivat cu cheia epoch-ului |
| `20..27` | mac pentru bytes `0..19` |

id-ul stabil si seed-ul nu apar in pachet.

### ratchet

cheia initiala se obtine din seed prin helperul existent. pentru fiecare
`next_epoch`, urmatoarea cheie are doua jumatati siphash:

```text
0x52 || next_epoch_le32 || lane
```

lane este 0 pentru prima jumatate si 1 pentru a doua. ambele jumatati se
calculeaza cu cheia veche. cheia veche este inlocuita numai dupa ambele calcule.

eid-ul foloseste domeniul `0x45`, epoch-ul si sectorul. mac-ul foloseste o cheie
separata prin xor pe bytes 0, 7, 8 si 15. verificarea respinge header gresit,
epoch peste 1000000, eid gresit, mac gresit, tamper si seed gresit.

### jurnal persistent

jurnalul are 2 pagini de cate 4096 bytes si recorduri de 40 bytes:

| bytes | continut |
|---|---|
| `0..3` | magic `0x47535452` |
| `4..7` | generatie |
| `8..11` | epoch de reluare |
| `12..15` | numar total de erase-uri |
| `16..31` | cheia pentru epoch-ul de reluare |
| `32..35` | crc32 pentru bytes `0..31` |
| `36..39` | commit `0xc01117ed` |

regulile obligatorii sunt:

- corpul de 36 bytes se scrie primul;
- commit-ul se scrie separat si ultimul;
- un record fara commit sau cu crc gresit este invalid;
- lease-ul are 16 epoch-uri;
- viitorul epoch si cheia lui se salveaza inainte de prima emisie;
- la reboot se reia din viitor si se rezerva urmatorul lease;
- un record incert dupa ultimul record valid produce un skip de 2 lease-uri;
- la umplerea paginii se sterge cealalta pagina;
- pagina cu cel mai nou record valid nu se sterge inainte de commit-ul urmator.

### replay si energie

gateway-ul retine pentru fiecare tag adresa ble si ultimul epoch acceptat.
respinge un epoch mai mic sau un pachet valid venit de la alta adresa si
raporteaza `ghost_replay`.

formula energetica este:

```text
unitati = scrieri_flash * 8 + erase_pagina * 40 + advertismente
```

un record costa doua scrieri. scenariul cu reboot trebuie sa ramana la maximum
80 unitati. testul de anduranta pentru 2000 epoch-uri permite maximum 252
scrieri si exact un erase.

## ce trebuie sa faca studentii

### todo 1 - siphash-2-4

- implementare canonica;
- cheie de 128 bits si rezultat de 64 bits;
- cuvinte little-endian;
- bloc final partial corect;
- toti cei 64 vectori cunoscuti trebuie sa treaca.

### todo 2 - pasul de ratchet

- domeniu `0x52`;
- epoch little-endian;
- lane 0 si lane 1 distincte;
- ambele rezultate derivate din cheia veche.

### todo 3 - payload v3

- header complet;
- eid pe domeniul `0x45`;
- cheie mac separata;
- mac peste bytes `0..19`;
- fara seed sau id stabil in radio.

### todo 4 - verificare

- verificare stricta de header si limita de epoch;
- reconstruire eid si mac;
- comparatie fara early exit;
- respingere pentru orice tamper si seed gresit.

### todo 5 - boot si recovery

- scanarea ambelor pagini;
- verificarea magic, commit si crc;
- alegerea generatiei celei mai noi;
- tratarea recordurilor rupte sau corupte;
- rezervarea durabila a lease-ului inainte de return.

### todo 6 - emisie si avans

- rezervare noua inainte de epuizarea lease-ului;
- un singur payload pentru epoch-ul curent;
- un singur avans al epoch-ului si cheii in ram;
- fara reutilizare dupa reboot.

### workflow recomandat

1. cititi complet `problem/problem.md` si header-ul;
2. rezolvati siphash si ratchet;
3. rezolvati build si verify;
4. implementati encoderul, crc-ul, scanarea si append-ul;
5. testati fresh boot si reboot normal;
6. testati torn body, torn commit si crc corupt;
7. verificati wear si energie;
8. porniti run-ul complet;
9. analizati intai testele native, apoi build-ul zephyr, apoi renode;
10. confirmati markerul final, nu doar compilarea.

### criterii de acceptare

o solutie trece numai daca:

- toate testele native trec;
- starter-ul nerezolvat nu trece;
- mutatiile invalide nu trec;
- tag-ul si gateway-ul se compileaza pentru nrf52840;
- toate 3 gateway-urile pornesc;
- toate 6 tag-urile sunt vazute si rotite;
- clona este respinsa;
- replay-ul este respins;
- reboot-ul este observat fara epoch duplicat;
- energia este maximum 80;
- nu exista `ghost_fatal`;
- validatorul afiseaza `passed=1`;
- run-ul termina cu `ghosttag fleet survived the apocalypse`.

## ce trebuie sa facem noi

### inainte de eveniment

1. rulam `pytest -q`;
2. verificam ca referinta trece testul nativ complet;
3. verificam ca starter-ul pica;
4. construim imaginile participant si showcase din acelasi commit;
5. rulam referinta in imaginea participant, cu reteaua dezactivata;
6. inspectam imaginea participant pentru scurgeri de referinta;
7. impingem branch-ul pe `public-origin`;
8. publicam imaginile si verificam digesturile din registry;
9. salvam rollback-ul pentru scenariul si imaginile live;
10. reprovizionam `initial` si `user1..user9` cu workspace curat;
11. verificam loginul si cele trei directoare pentru fiecare user;
12. folosim numai `initial` pentru acceptanta pozitiva si negativa;
13. restauram `initial` la skeleton dupa proba finala.

### acceptanta obligatorie pe initial

1. confirmam hashurile skeletonului;
2. apasam run si cerem exit nenul in gate-ul nativ;
3. completam numai `firmware/ghost_protocol.c`;
4. apasam run prin acelasi api folosit de student;
5. cerem markerul complet de succes;
6. verificam raportul html;
7. resetam workspace-ul la starter.

nu folosim conturile studentilor pentru a rula solutia de referinta.

### in timpul evenimentului

- monitorizam nodurile, frontend-ul, ingress-ul si registry-ul;
- monitorizam coada de job-uri, cpu, ram si ephemeral storage;
- pastram pvc-urile studentilor;
- ajutam la compilare si interpretarea logurilor;
- nu oferim referinta, seed-urile sau rezolvarea todo-urilor;
- nu schimbam durata, bugetele sau testele in timpul sesiunii.

### rollback

- restauram digesturile si configmap-ul capturat;
- nu stergem pvc-uri la un rollback obisnuit;
- stergem namespace-uri numai cand reprovisionarea completa este intentionata;
- verificam din nou `/healthz`, loginul si un run negativ;
- documentam incidentul si dovezile.

## ultima acceptanta

acceptanta din 18 iulie 2026:

- commit runtime: `28cd5971c798fd6b02419d626462d53da09b2838`;
- commit documentatie ocw: `8951a68b14dc6d89b7f6c03813b74caa2f3d5b9b`;
- digest participant:
  `sha256:cca10144e4b069d6f5dfa01704fd4c7ea5857f50d04f079050cd71d9e827cd20`;
- digest showcase:
  `sha256:06fdaa3a15c9bcf488b2c14f5c418a8432085b461029d0ad831603df0ec00c5a`;
- utilizatori reprovisionati: `initial` si `user1..user9`, toate cele 10 ide-uri
  ready, toate workspace-urile verificate la acelasi skeleton;
- starter live din `initial`: `protocol_contract_tests failures=94`,
  `ghost_validation passed=0`, sse terminat cu exit 1;
- referinta live din `initial`: `protocol_contract_tests failures=0`, ambele
  build-uri zephyr reusite, renode exit 0, 6/6 tag-uri vazute, 6/6 rotite, 56
  pachete rogue respinse, 30 replay-uri respinse, recovery la epoch 16 fara
  reutilizare, energie 35/80, `ghost_validation passed=1`, sse exit 0;
- raportul html si `validation.json` au fost verificate si arhivate;
- `initial` a fost reprovisionat dupa acceptanta si verificat din nou: 6
  todo-uri, zero fisiere reference si explorer cu `firmware/`, `problem/`,
  `renode/`.

## fisiere importante

| fisier | rol |
|---|---|
| `firmware/ghost_protocol.c` | singurul fisier editabil |
| `firmware/include/ghost_protocol.h` | api, layout si limite |
| `firmware/tests/test_protocol.c` | feedback public rapid |
| `support/tests/test_protocol_contract.c` | contract complet imutabil |
| `tests/test_protocol_contract.py` | referinta, starter si mutatii negative |
| `ide/problem/problem.md` | enuntul studentului |
| `docs/OCW_GHOSTTAG_CHALLENGE.md` | pagina ocw pentru studenti |
| `reference/firmware/` | solutia organizatorilor |
| `support/gateway/` | observer trusted si replay policy |
| `docker/scripts/generate_swarm_resc.py` | power cut si atacatori |
| `docker/scripts/validate_swarm.py` | criterii finale |
| `docker/scripts/generate_report.py` | raport html |
| `docs/HACKATHON_RUNBOOK.md` | rollout si rollback |
