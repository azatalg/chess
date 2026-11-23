# Benchmarki silnika

Najprościej uruchomić krótki zestaw w zoptymalizowanej konfiguracji:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target bench -j
```

Target `bench` buduje i uruchamia profil `quick`. Pełny zestaw oraz wybrane
scenariusze można uruchomić bezpośrednio:

```sh
./build-release/chess_bench --full
./build-release/chess_bench --filter perft
./build-release/chess_bench --list
```

Zestaw mierzy generowanie legalnych ruchów, wykonanie/cofnięcie ruchu,
parsowanie FEN, perft dla pozycji startowej i Kiwipete oraz wyszukiwanie w
pozycji ze środka gry. `perft` raportuje węzły/s, a pozostałe testy właściwą
dla nich przepustowość.

Wyniki najlepiej porównywać na tej samej maszynie, przy tym samym kompilatorze
i konfiguracji `Release`. Profil `full` jest mniej podatny na szum pomiarowy.
