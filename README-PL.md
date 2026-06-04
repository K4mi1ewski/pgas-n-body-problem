# Problem N-cial z PGAS (UPC++)

Program symuluje ruch `N` cial oddzialujacych grawitacyjnie (metoda Velocity Verlet) z wykorzystaniem **UPC++** i modelu **PGAS** (Partitioned Global Address Space).

Srodowisko docelowe: **klaster Linux** z zainstalowanym UPC++ (modul lub zmienna `UPCXX_INSTALL`).

## Wymagania

- kompilator C++14 (przez wrapper `upcxx`)
- [UPC++](https://upcxx.lbl.gov/) (`upcxx`, `upcxx-run`)
- `make`, `libm`
- opcjonalnie: OpenMP w kompilatorze (`make threaded`)

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

Build z OpenMP (watki wewnatrz kazdego procesu UPC++):

```bash
make threaded
```

## Uruchamianie

```bash
cd src
make run
```

Domyslne parametry `make run`:

- `RUN_PROCS=16` — liczba procesow UPC++ (`upcxx-run -n`)
- `INPUT=input.txt`
- `OUTPUT=output.txt`
- `SHARED_HEAP=256M` — rozmiar shared heap (zwieksz przy duzym `N`)

Przyklady:

```bash
make run RUN_PROCS=8 INPUT=input.txt OUTPUT=wynik.txt
make run RUN_PROCS=16 SHARED_HEAP=1G
```

Bezposrednio:

```bash
upcxx-run -shared-heap 512M -n 4 ./nbody_upcxx input.txt wynik.txt
```

Bez drugiego argumentu domyslnie powstanie `output.txt`:

```bash
upcxx-run -shared-heap 512M -n 4 ./nbody_upcxx input.txt
```

## Rownoleglosc: UPC++ i OpenMP

Dwa niezalezne poziomy:

| Poziom | Mechanizm | Sterowanie |
|--------|-----------|------------|
| Miedzy procesami | UPC++ / PGAS | `upcxx-run -n P` (`RUN_PROCS`) |
| W jednym procesie | OpenMP (opcjonalnie) | `make threaded` |

- **`make`** (domyslnie): w kazdym procesie petle wykonuje jeden watk CPU. PGAS nadal uzywa `P` procesow.
- **`make threaded`**: kompilacja z `-fopenmp` i `UPCXX_THREADMODE=par`. Petle po `local_count` w kroku Verlet maja `#pragma omp parallel for` (przyspieszenia oraz aktualizacja predkosci/pozycji).

OpenMP dzieli iteracje `li = 0 … local_count-1` miedzy watki CPU **na tym samym ranku**. Nie zastepuje procesow UPC++: przy `RUN_PROCS=4` i 8 watkach OpenMP masz 4 procesy × do 8 watkow kazdy.

**Uwaga:** `compute_accel` wywoluje `global_get` / `global_put` (PGAS). Przy OpenMP kilka watkow na jednym ranku moze jednoczesnie korzystac ze shared heap — wymaga UPC++ z `-threadmode=par` (`make threaded` to ustawia). Jesli na klastrze zalecany jest tylko `threadmode=seq`, uzywaj zwyklego `make`.

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

## Uwagi dla klastra

- Przed kompilacja zaladuj modul UPC++ (zalezne od site).
- Ustaw `-shared-heap` wystarczajaco duzy (ok. `10 * N * 8` bajtow dla tablic globalnych + zapas).
- Na produkcji uzyj `make opt`.
