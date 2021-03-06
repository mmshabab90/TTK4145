# Øving 1: Hello world

## 3: Grunner til samtidighet og parallellitet
> **Parallellitet:** Flere tråder kjører samtidig.  
> **Samtidighet:** En mer generell form for parallellitet som kan inkludere tids-beskjæring som en form for virtuell parallellitet.

> *-- [Oracle.com](http://docs.oracle.com/cd/E19455-01/806-5257/6je9h032b/index.html)*

* Prosessorer bruker flere kjerner blant annet for å redusere effektforbruk med økende frekvensskalering.
* Samtidighet kan for eksempel brukes til å drive to uavhengige fysiske enheter på samme tid.
* Samtidighet vanskeligere?
* Prosesser og tråder er eksekvering av programmer, der tråder er prosesser som deler minne. Grønne tråder er tråder som er planlagt av et runtime-bibliotek eller en virtuell maskin i stedet for direkte vha operativsystemet. Korutiner er en kontrollstruktur der flytkontroll foregår mellom to forskjellige rutiner uten å returnere.
  * `pthread_create()` - *tråd*
  * `threading.Thread()`: - *tråd*
  * `go`: - *tråd*
* GIL-en synkroniserer kjøringen av Python-tråder, slik at kun en av trådene kan kjøre om gangen.
* ...workaround?
* `func GOMAXPROCS(n int) int` begrenser antall operativsystemtråder som kan kjøre Go-kode på brukernivå samtidig.

## 4: Endelig kode
* [C](ex01.c)
* [Python](ex01.py)
* [Go](ex01.go)
