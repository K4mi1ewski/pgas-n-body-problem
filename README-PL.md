# Problem N-cial z PGAS (UPC++)

Program symuluje ruch `N` cial oddzialujacych grawitacyjnie (metoda Velocity Verlet) z wykorzystaniem **UPC++** i modelu **PGAS** (Partitioned Global Address Space).

Srodowisko docelowe: **klaster Linux** z zainstalowanym UPC++ (modul lub zmienna `UPCXX_INSTALL`).

## Wymagania

- kompilator C++14 (przez wrapper `upcxx`)
- [UPC++](https://upcxx.lbl.gov/) (`upcxx`, `upcxx-run`)
- `make`, `libm`
- opcjonalnie: OpenMP (`make threaded`)
- opcjonalnie: CUDA Toolkit (`make cuda`)

## Kompilacja

```bash
cd src
export UPCXX_INSTALL=/sciezka/do/upcxx   # jesli nie ma w PATH
make
```

Build z optymalizacja:

```bash
make opt
```

## Uruchamianie

```bash
cd src
make run
```

Domyslne parametry `make run`:

- `RUN_PROCS=4` — liczba procesow UPC++ (`upcxx-run -n`)
- `INPUT=input.txt`
- `OUTPUT=output.txt`
- `SHARED_HEAP=512M` — rozmiar shared heap (zwieksz przy duzym `N`)

Przyklady:

```bash
make run RUN_PROCS=8 INPUT=input.txt OUTPUT=wynik.txt
make run RUN_PROCS=16 SHARED_HEAP=1G
```

Bezposrednio:

```bash
OUTPUT_FILE=wynik.txt upcxx-run -shared-heap 512M -n 4 ./nbody_upcxx input.txt
```

## PGAS a MPI

Wersja MPI uzywa kolektywow (`MPI_Allgatherv`) do synchronizacji polozen i predkosci. Wersja PGAS trzyma stan w **shared heap** UPC++ (`upcxx::new_array`). Kazdy proces aktualizuje swoj wycinek tablic; `upcxx::barrier()` zapewnia widocznosc danych przed kolejnym liczeniem przyspieszen. Odczyty zdalne realizowane sa przez `rget`.

## Format pliku wejsciowego

1. Pierwsza linia:

   ```text
   N dt steps
   ```

2. Kolejne `N` linii:

   ```text
   M x y z vx vy vz
   ```

Znaczenie pol:

- `N` — liczba cial
- `dt` — krok czasowy (s)
- `steps` — liczba krokow
- `M` — masa (kg)
- `x y z` — polozenie (m)
- `vx vy vz` — predkosc (m/s)

Stala grawitacji: `G = 6.67430e-11 m^3/(kg*s^2)` (SI).

## Wynik programu

- `stdout`: linia `N dt steps`, potem `N` linii `M x y z vx vy vz`
- plik `OUTPUT` (domyslnie `output.txt`):

  ```text
  N={N} bodies
  i - M x y z vx vy vz
  ...
  ```

## Opcjonalne CUDA

Katalog `cuda/` jest opcjonalny. Domyslny `make` nie uzywa GPU.

```bash
make cuda
make run RUN_PROCS=4
```

Wymaga `nvcc` i biblioteki CUDA. Katalog `cuda/` mozna usunac — build CPU pozostaje bez zmian.

## Uwagi dla klastra

- Przed kompilacja zaladuj modul UPC++ (zalezne od site).
- Ustaw `-shared-heap` wystarczajaco duzy (ok. `10 * N * 8` bajtow dla tablic globalnych + zapas).
- Na produkcji uzyj `make opt`.
