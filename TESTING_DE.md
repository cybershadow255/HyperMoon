# Test-Anleitung für HyperMon

Um das Programm in deiner Sandbox (z. B. eine Windows 10/11 VM) zu testen, folge diesen Schritten:

### 1. Kompilierung (Build)
Du benötigst Visual Studio mit dem **Windows Driver Kit (WDK)**.

*   **Treiber:** Öffne `HyperMon.sln`, wähle `x64 Release` und baue das Projekt. Kopiere die `HyperMon.sys` in den Ordner mit der `setup.exe`.
*   **Usermode (Setup & Monitor):**
    *   Entweder via CMake:
        ```bash
        mkdir build && cd build
        cmake ..
        cmake --build . --config Release
        ```
    *   Oder direkt via MSVC-Konsole:
        ```bash
        cl setup.cpp /Fe:setup.exe /W4
        cl monitor.cpp /Fe:monitor.exe /W4
        ```

### 2. Der Test-Ablauf (Plug & Play Simulation)

1.  **Vorbereitung:** Kopiere `setup.exe` und `HyperMon.sys` in deine Sandbox.
2.  **Schritt 1 (HVCI Check):** Starte `setup.exe` als Administrator.
    *   Falls **Kernisolierung (HVCI)** an ist: Das Programm öffnet die Windows-Einstellungen. Schalte "Speicherintegrität" aus und starte die Sandbox neu.
    *   Dank des `RunOnce`-Eintrags startet das Programm nach dem Neustart automatisch wieder (oder du startest es manuell).
3.  **Schritt 2 (Treiber laden):** Wenn HVCI aus ist, wird `setup.exe` versuchen, den Treiber zu laden.
    *   *Hinweis:* Da wir keinen echten Manual Mapper (wie kdmapper.exe) mitliefern (um AV-Probleme zu vermeiden), nutzt das Programm standardmäßig `sc.exe`. Das funktioniert in der Sandbox am besten, wenn du einmalig `bcdedit /set testsigning on` ausführst und neustartest.
4.  **Schritt 3 (Schutz testen):** Sobald der Treiber geladen ist, siehst du die Meldung: `Prozess-Schutz aktiv (PID: XXXX)`.
    *   Versuche nun innerhalb der 10 Sekunden, die `setup.exe` über den Task-Manager zu beenden oder mit einem Tool darauf zuzugreifen. Es sollte blockiert werden.
5.  **Schritt 4 (Automatische Deinstallation):** Nach 10 Sekunden deaktiviert der Treiber die Callbacks und `setup.exe` deinstalliert den Treiberdienst automatisch.

### 3. Live-Überwachung
Du kannst parallel die `monitor.exe` starten, um zu sehen, welche anderen Prozesse oder Registry-Zugriffe der Treiber im Hintergrund erkennt.

---
**WICHTIG:** Für echtes "Plug & Play" ohne `testsigning on` kopiere bitte die `kdmapper.exe` in den Ordner. Das Programm erkennt dies und nutzt sie automatisch für das Laden ohne Neustart-Zwang.
