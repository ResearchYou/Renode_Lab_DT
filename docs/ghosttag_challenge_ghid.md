# ghosttag apocalypse - status, cerinte si responsabilitati

## scop

GhostTag Apocalypse este un challenge de firmware, securitate si simulare BLE.
Studentii implementeaza protocolul unui tag nRF52840 care trebuie sa poata fi
gasit de gateway-uri autorizate fara sa transmita un identificator stabil.

Challenge-ul combina:

- cod c pentru protocol;
- SipHash-2-4 pentru identitate efemera si autentificare;
- firmware Zephyr pentru `nrf52840dk/nrf52840`;
- simulare Renode cu mai multe placi si un mediu BLE pozitional;
- validare automata, raport HTML si executie in Kubernetes.

Acesta este un challenge de hackathon, nu un protocol complet pentru un produs
real. Nu acopera integral provisioning securizat, protectia cheilor, replay,
certificarea radio sau evaluarea completa de confidentialitate.

## status curent

status verificat la 2026-07-18:

| componenta | status | observatii |
|---|---|---|
| codul challenge-ului | gata | runtime-ul live este construit din commit-ul `4889c2e` pe branch-ul `nrf52840-swarm/ghosttag-apocalypse` |
| starter pentru studenti | gata | contine exact 3 TODO-uri in `firmware/ghost_protocol.c` |
| implementare de referinta | gata | se afla in `reference/firmware/`; nu trebuie oferita studentilor |
| teste native | gata | 64 vectori SipHash, vector complet de payload, tamper pe fiecare byte, header/EID/MAC, seed, sector si leakage |
| build Zephyr | gata | tag si gateway pentru `nrf52840dk/nrf52840` |
| simulare Renode | gata | tag-uri, gateway-uri, clone, pozitii si raza BLE determinista |
| validator si raport | gata | produce `validation.json` si `output/report.html` |
| integrare cu platforma | activa live | scenariul este activ in ConfigMap si in workspace-ul `initial` |
| matrice pozitiva si negativa | trecuta | referinta trece; starter-ul si 9 mutatii invalide pica; modificarea contractului pica cu exit 2 |
| validare locala de referinta | trecuta | a trecut si cu reteaua dezactivata, 4 CPU si 4 GiB: 6/6 tag-uri, 6/6 rotite, raport HTML generat |
| validare live de referinta | trecuta | rulare pornita prin API-ul IDE, `GHOST_VALIDATION passed=1`, `done: 0`, fara OOM |
| validare live negativa | trecuta | starter-ul a produs 69 esecuri de contract, a sarit peste Renode, a generat raport si a terminat cu `done: 1` |
| imagine studenti | publicata | sursa de referinta si helperul privat de provisioning sunt absente; gateway-ul foloseste numai un obiect ARM precompilat |
| imagine showcase | publicata | seed separat cu referinta, acelasi runtime complet offline |
| cluster Kubernetes | sanatos | toate cele 3 noduri sunt `Ready`, fara poduri defecte, pagina `/login` raspunde 200 |
| scenariu activ pentru studenti | da | `nrf52840-swarm-ghosttag-apocalypse` este presetul live |
| imagine GhostTag in registry | confirmata | digest participant `cd4ed4bafd24`, digest showcase `d6d69c681e81`, ambele cu label `4889c2e` |
| suprafata participant live | verificata | workspace-ul `initial` arata exact `firmware/`, `problem/` si `renode/`; hashurile coincid cu imaginea; nu expune referinta sau derivarea seed-urilor |
| showcase GhostTag | trecut | acceptanta 12/12 a trecut pe digestul anterior; runtime-ul curent a trecut smoke-ul complet local cu 6/6 tag-uri si referinta |
| utilizatori studenti live | provisionati | `user1..user9` au aceeasi parola de eveniment, IDE-urile sunt Ready si loginurile au fost verificate; `user10` nu exista |

## ce este deja implementat

### contractul radio

Payload-ul BLE are exact 28 de bytes:

| bytes | continut |
|---|---|
| `0..1` | company id `0xF00D` |
| `2` | versiune protocol `2` |
| `3` | flags |
| `4..7` | epoch de rotatie, little-endian |
| `8..11` | sector public, little-endian |
| `12..19` | identificator efemer autentificat |
| `20..27` | tag de autentificare pentru bytes `0..19` |

Pachetul nu contine id-ul stabil al dispozitivului. Identitatea radio se
schimba la fiecare epoch, iar gateway-ul verifica pachetul folosind spatiul
mic de chei autorizate al sectorului.

### mediul studentului

Studentul primeste in browser:

- `firmware/` - sursa editabila;
- `problem/` - enuntul challenge-ului;
- `renode/` - generatorul scenariului, launcherul si descrierea platformei;
- butonul Run - porneste pipeline-ul de validare;
- panoul Results - afiseaza logurile, rezultatele si `output/report.html`.

Explorer-ul arata exact cele trei directoare `firmware/`, `problem/` si
`renode/`. Copiile din `renode/` sunt pentru inspectie; modificarile lor nu
schimba evaluarea, care ruleaza scripturile imutabile din imagine.

Datele sunt pastrate pe PVC-ul utilizatorului. O schimbare de scenariu arhiveaza
workspace-ul anterior. PVC-urile nu trebuie sterse in timpul challenge-ului sau
la rollback.

### pipeline-ul automat

La fiecare Run se executa, in ordine:

1. verificarea ca numai `firmware/ghost_protocol.c` a fost schimbat;
2. compilare nativa C17 cu `-Wall -Wextra -Werror`;
3. 64 vectori SipHash, vectorul complet de payload si testele de contract;
4. tamper pe fiecare dintre cei 28 bytes si cazuri resemnate invalide;
5. build Zephyr pentru firmware-ul tag-ului;
6. build Zephyr pentru gateway-ul trusted;
7. generarea sectorului Renode;
8. pornirea masinilor nRF52840 pe mediul BLE;
9. colectarea logurilor UART ale gateway-urilor;
10. validarea flotei si generarea raportului HTML.

Validatorul foloseste numai traficul observat de gateway-uri. Nu citeste
variabilele c ale studentului si nu inspecteaza memoria tag-urilor.

Testul de contract este in imagine si nu poate fi inlocuit din workspace.
Gateway-ul foloseste un obiect ARM precompilat. Imaginea studentilor nu contine
directorul `reference/` si nu contine sursa implementarii organizatorilor.
Cand testele de contract esueaza, pipeline-ul opreste build-ul si simularea,
scrie totusi `validation.json` si `report.html`, apoi intoarce exit nenul.

### scara scenariului

| mod | tag-uri | gateway-uri | clone rogue | total placi |
|---|---:|---:|---:|---:|
| smoke local | 6 | 3 | 2 | 11 |
| rulare normala student | 6 | 3 | 2 | 11 |
| un sector showcase | 16 | 3 | 2 | 21 |
| showcase complet | 192 | 36 | 24 | 252 |

Showcase-ul foloseste 12 sectoare independente si ruleaza maximum 4 in paralel.
O rulare de student cere si limiteaza `4 CPU` si `4096 MiB` RAM. Un sector de
showcase cere `1800m CPU` si `4096 MiB`, cu limite de `3 CPU` si `5120 MiB`.
Showcase-ul observa 8 secunde simulate si nu permite retry la validare. Aceste
valori nu trebuie schimbate fara un nou test de capacitate. Fiecare pod are si
un deadline dur de 30 minute, ca un emulator blocat sa devina esec explicit.
Seed-ul RF 11 a produs o crestere patologica a timpului host la testul de
capacitate. Ultimul sector refoloseste programarea RF validata pentru seed 0,
dar pastreaza sectorul, cheile, adresele si id-urile sale distincte.

Acceptanta live finala a terminat indexurile `0-11` pe digestul showcase
`2079e1f5fb0a`. Toate cele 12 poduri au iesit cu cod 0 si fiecare log contine
`GHOST_VALIDATION passed=1`, `tags=16/16` si `rotated=16/16`.

### infrastructura live

Platforma a fost recuperata si intarita dupa incidentul de alimentare:

- CoreDNS functioneaza si rezolva nume interne si externe;
- tunnel-ul Cloudflare foloseste token din fisier montat, nu token in argumente;
- credentialul tunnel-ului a fost rotit;
- frontend-ul platformei este sanatos;
- provisioner-ul selecteaza numai workeri `Ready=True` si schedulable;
- toate cele 3 noduri au systemd in starea `running`;
- blocarea infinita in `plymouth-quit-wait.service` a fost eliminata;
- jurnalele systemd sunt persistente pe toate nodurile;
- build-ul Zephyr protejeaza automat Ninja de diferentele de ceas host-cluster;
- imaginea include local SVD-ul nRF52840, deci Renode nu acceseaza internetul;
- heap-ul Mono are limita soft de 2 GiB, sub limita podului.

Modificarile platformei sunt in commit-ul `87fa465` din
`/home/pwd/eg106-platform` pe `cloud_s1`.

## ce trebuie sa faca studentii

### suprafata de lucru

Studentii modifica numai:

```text
firmware/ghost_protocol.c
```

Nu trebuie schimbate header-ul, dimensiunea pachetului, CMake, gateway-ul,
formatul UART, durata epoch-ului sau harness-ul de testare.

### todo 1 - siphash-2-4

Studentul implementeaza `ghost_siphash24`:

- cheie de 128 bits;
- rezultat de 64 bits;
- constructie canonica SipHash-2-4;
- citire little-endian;
- suport corect pentru lungimi care nu sunt multiplu de 8;
- rezultat identic cu vectorii cunoscuti inclusi in teste.

O implementare aproximativa sau cu ordinea gresita a bytes-ilor nu trece.

### todo 2 - construirea payload-ului

Studentul completeaza `ghost_build_payload`:

- pastreaza header-ul si layout-ul existent;
- deriva identitatea efemera folosind domeniul `0x45`, epoch-ul si sectorul;
- scrie identitatea efemera in bytes `12..19`;
- deriva MAC-ul cu un domeniu de cheie diferit de cel folosit pentru EID;
- autentifica bytes `0..19`;
- scrie MAC-ul in bytes `20..27`;
- nu copiaza `device_seed` in pachet.

Aceeasi cheie bruta nu trebuie refolosita direct pentru ambele roluri.

### todo 3 - verificarea payload-ului

Studentul completeaza `ghost_verify_payload` astfel incat sa respinga:

- company id gresit;
- versiune gresita;
- identitate efemera gresita;
- MAC gresit;
- flags modificati dupa semnare;
- orice modificare a bytes-ilor autentificati;
- verificarea cu un seed neautorizat;
- pachete de clone care doar respecta forma protocolului.

Verificarea trebuie sa reconstruiasca valorile asteptate, nu sa accepte un
pachet doar pentru ca are dimensiunea si header-ul corecte.

### workflow recomandat studentilor

1. cititi `problem/problem.md` si comentariile celor 3 TODO-uri;
2. implementati si verificati mai intai SipHash;
3. implementati EID-ul si MAC-ul;
4. implementati verificarea;
5. rulati challenge-ul din butonul Run;
6. reparati intai erorile native, apoi erorile Zephyr;
7. analizati logurile Renode numai dupa ce build-urile trec;
8. deschideti sau refresh-uiti panoul Results dupa terminarea rularii.

### criterii de succes pentru studenti

O solutie este acceptata numai daca:

- toate testele SipHash trec;
- toate cele 3 gateway-uri pornesc;
- toate tag-urile autorizate sunt observate;
- fiecare tag isi schimba identitatea radio;
- traficul clonelor este respins;
- nu exista erori fatale in firmware;
- validatorul afiseaza `GHOST_VALIDATION passed=1`;
- rularea se termina cu `GHOSTTAG FLEET SURVIVED THE APOCALYPSE`.

Compilarea fara erori nu este suficienta.

## ce trebuie sa facem noi ca organizatori

### inainte de challenge

1. verificam clusterul, frontend-ul, registry-ul si workerii;
2. reconstruim imaginea GhostTag din branch-ul corect;
3. rulam smoke test-ul cu implementarea de referinta si 6 tag-uri;
4. nu continuam daca markerul final de succes lipseste;
5. impingem explicit tagul
   `nrf52840-swarm-ghosttag-apocalypse` in registry;
6. verificam manifestul imaginii direct in registry sau prin pull de pe un nod;
7. rulam dry-run server-side pentru ambele manifeste Kubernetes;
8. salvam ConfigMap-ul scenariului activ pentru rollback;
9. activam `k8s/platform/active-scenario.yaml`;
10. testam cu un utilizator disposable.

Testul disposable trebuie sa demonstreze ambele directii. Pe live, ambele au
fost verificate prin acelasi API folosit de butonul Run:

- starter-ul este seed-uit corect si esueaza curat cu exit 1;
- implementarea de referinta trece complet cu exit 0;
- raportul apare in Results;
- fisierele vechi sunt arhivate la schimbarea scenariului.

Matricea locala obligatorie demonstreaza si cazurile care nu trebuie acceptate:

- starter fara TODO-uri implementate;
- SipHash gresit;
- EID fara epoch, sector sau seed;
- seed stabil folosit ca EID;
- aceeasi cheie bruta folosita pentru EID si MAC;
- verificare de header, EID sau MAC eliminata;
- implementare `accept-all`;
- modificarea `main.c`, header-ului, CMake sau `prj.conf`.

Dupa validare stergem utilizatorii si namespace-urile disposable. Lotul pentru
studenti se provisionaza din nou numai cand deschidem accesul. Lotul curent
`user1..user9` este provisionat si verificat live.

Comenzile operationale complete sunt in
[`docs/HACKATHON_RUNBOOK.md`](HACKATHON_RUNBOOK.md).

### la inceputul sesiunii

Noi trebuie sa explicam clar:

- problema de confidentialitate: un id stabil permite urmarirea persoanei;
- de ce exista epoch, EID si MAC;
- de ce gateway-ul trusted cauta in flota autorizata;
- ce controleaza studentul si ce controleaza harness-ul;
- de ce exista clone rogue;
- care este singurul fisier editabil;
- cum se citesc cele trei niveluri de feedback.

Nu oferim implementarea de referinta si nu expunem seed-urile flotei.

### in timpul challenge-ului

Noi monitorizam:

- starea nodurilor si a podurilor;
- coada de Job-uri pentru rularile studentilor;
- erori de pull pentru imagine;
- utilizarea CPU, RAM si ephemeral storage;
- disponibilitatea frontend-ului si a ingress-ului;
- PVC-urile participantilor;
- timpii de executie Renode.

Ajutam studentii cu erori de compilare, folosirea platformei si interpretarea
raportului. Nu implementam TODO-urile in locul lor.

### showcase-ul final

Showcase-ul este optional si separat de rularile studentilor.

Inainte de lansare:

- verificam resursele cerute pe ambii workeri;
- confirmam ca exista cel putin un worker sanatos si suficient spatiu pentru
  imaginea de aproximativ 3.76 GB;
- pastram `parallelism: 4`;
- nu stergem poduri sau PVC-uri ale studentilor pentru a face loc.

Lansam `k8s/showcase/indexed-job.yaml` si urmarim toate cele 12 completions.
Showcase-ul este reusit numai daca toate cele 12 sectoare termina cu succes si
fiecare log contine `GHOST_VALIDATION passed=1`.

### dupa challenge sau la rollback

- stergem numai Job-ul de showcase daca trebuie oprit;
- restauram ConfigMap-ul salvat daca revenim la scenariul anterior;
- nu stergem PVC-urile studentilor;
- pastram rapoartele si logurile utile pentru evaluare;
- verificam din nou health endpoint-ul public;
- documentam orice incident de cluster sau limita de capacitate observata.

## definitia de gata pentru organizatori

Challenge-ul poate fi deschis studentilor cand toate punctele sunt adevarate:

- [x] imaginea GhostTag este construita din commit-ul dorit;
- [x] smoke test-ul de referinta trece si fara acces la retea;
- [x] tagul imaginii este confirmat in registry;
- [x] toate nodurile necesare sunt `Ready`;
- [x] frontend-ul si DNS-ul sunt sanatoase;
- [x] dry-run-ul manifestelor trece;
- [x] starea anterioara a scenariului este salvata;
- [x] scenariul GhostTag este activ;
- [x] starter-ul esueaza controlat pentru utilizatorul disposable;
- [x] referinta trece pentru utilizatorul disposable;
- [x] raportul HTML se incarca in Results;
- [x] rollback-ul este pregatit;
- [x] solutia de referinta nu este accesibila participantilor.
- [x] `user1..user9` sunt provisionati si fiecare login ajunge in IDE.

workspace-ul live `initial` a fost readus la starter dupa testul pozitiv.
utilizatorii `user1..user9` sunt activi; `user10` nu este provisionat.

## fisiere importante

| fisier | rol |
|---|---|
| `firmware/ghost_protocol.c` | singurul fisier editat de student |
| `firmware/include/ghost_protocol.h` | contractul wire si API-ul |
| `firmware/tests/test_protocol.c` | testele publice native |
| `support/tests/test_protocol_contract.c` | contractul complet si nemodificabil |
| `tests/test_protocol_contract.py` | referinta, starter si cele 9 mutatii negative |
| `ide/problem/problem.md` | enuntul complet afisat in browser |
| `reference/firmware/` | solutia organizatorilor |
| `support/gateway/` | gateway-ul trusted |
| `docker/scripts/build_firmware.sh` | teste native si build-uri Zephyr |
| `docker/Dockerfile.showcase` | imaginea separata, numai pentru organizatori |
| `docker/scripts/generate_swarm_resc.py` | generarea topologiei Renode |
| `docker/scripts/validate_swarm.py` | criteriile automate de acceptare |
| `docker/scripts/generate_report.py` | raportul HTML |
| `k8s/platform/active-scenario.yaml` | activarea scenariului pentru studenti |
| `k8s/showcase/indexed-job.yaml` | showcase-ul de 252 placi |
| `docs/HACKATHON_RUNBOOK.md` | procedura operationala si rollback |
| `docs/instructor_notes.md` | structura recomandata pentru sesiunea de 3 ore |
