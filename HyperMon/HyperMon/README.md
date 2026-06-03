# HyperMon – Windows Kernel Monitor

Ein Ring-0 Kernel-Driver für Windows 10/11 (x64) der mit offiziellen
Windows-Kernel-Callbacks Prozesse, Threads, DLL-Loads und Registry-Ops
in Echtzeit abfängt und an eine usermode Console weiterleitet.

## Architektur

```
┌─────────────────────────────────────────────────┐
│                 USERMODE (Ring 3)                │
│   monitor.exe  ──DeviceIoControl──►  HyperMon   │
└─────────────────────────────────────────────────┘
                          │ IOCTL (buffered I/O)
┌─────────────────────────────────────────────────┐
│                KERNELMODE (Ring 0)               │
│                                                 │
│  PsSetCreateProcessNotifyRoutineEx              │
│  PsSetCreateThreadNotifyRoutine     ──► Queue   │
│  PsSetLoadImageNotifyRoutine            (4096)  │
│  CmRegisterCallback                             │
└─────────────────────────────────────────────────┘
```

Alle verwendeten Callbacks sind **offizielle, dokumentierte Windows APIs** –
genau die, die AV-Software (Windows Defender, CrowdStrike etc.) intern nutzt.

---

## Voraussetzungen

| Tool | Warum | Download |
|------|-------|----------|
| Visual Studio 2022 | C++ Compiler | visualstudio.microsoft.com |
| Windows Driver Kit (WDK) | Kernel Headers + Build-Tools | aka.ms/wdk |
| Windows SDK (10.0.22621+) | kommt mit VS | – |

> WDK-Version muss zur installierten Windows-Version passen.
> Beim WDK-Installer "Install Windows Driver Kit" wählen (nicht nur SDK).

---

## Build – Schritt für Schritt

### 1. Visual Studio Projekt anlegen

1. **Visual Studio 2022** öffnen
2. „Create new project" → Suche: **„Kernel Mode Driver, Empty (KMDF)"**
   - (alternativ: „Empty WDM Driver")
3. Name: `HyperMon`, Location: dieses Verzeichnis
4. `driver.cpp` und `shared.h` in den Projekt-Ordner kopieren

### 2. Projekt-Einstellungen

Rechtsklick auf Projekt → Properties:

```
Configuration: Release   Platform: x64
```

#### Driver Settings
```
Driver Settings → Target OS Version: Windows 10
Driver Settings → Target Platform:   Desktop
```

#### Linker → Command Line  ⚠️ WICHTIG
```
Additional Options: /integritycheck
```
> Ohne `/integritycheck` schlägt `PsSetCreateProcessNotifyRoutineEx` mit
> `STATUS_ACCESS_DENIED` (0xC0000022) fehl!

#### C/C++ → General
```
Warning Level: W4
Treat Warnings as Errors: No
```

### 3. Driver bauen
```
Build → Build Solution  (Ctrl+Shift+B)
```
Output: `x64\Release\HyperMon.sys`

### 4. Usermode Monitor bauen

Developer Command Prompt (VS) öffnen:
```cmd
cd usermode
cl monitor.cpp /Fe:monitor.exe /W3 /EHsc /std:c++17
```

---

## Setup & Installation

**Einmalig** (erstes Mal auf neuem PC):
```powershell
# Als Administrator:
powershell -ExecutionPolicy Bypass -File setup.ps1
# Falls Test Signing gerade erst aktiviert wurde:
powershell -ExecutionPolicy Bypass -File setup.ps1 -Restart
```

**Nach jedem Reboot** (Treiber manuell starten):
```powershell
sc start HyperMon
```

**Monitor starten:**
```
monitor.exe
```

**Deinstallieren:**
```powershell
powershell -ExecutionPolicy Bypass -File setup.ps1 -Uninstall
```

---

## Was passiert beim Setup?

1. **Test Signing Mode** aktivieren (`bcdedit /set testsigning on`)
   - Erlaubt selbst-signierte Kernel-Driver zu laden
   - Zeigt ein kleines „Test Mode" Wasserzeichen auf dem Desktop
   - Reboot einmalig nötig

2. **Self-signed Certificate** anlegen (via PowerShell `New-SelfSignedCertificate`)

3. **Driver signieren** (mit `signtool.exe` aus dem WDK)

4. **Service registrieren** (`sc create`) und starten (`sc start`)

---

## Ausgabe-Beispiel

```
#      TIME          EVENT    PID     PPID/TID    PATH / DATA
----------------------------------------------------------------------------------------------------
#1     14:22:01.431  PROC +   9832    PID:6120    C:\Windows\System32\notepad.exe
#2     14:22:01.435  THR  +   9832    TID:11204
#3     14:22:01.437  DLL/EXE  9832               C:\Windows\System32\ntdll.dll
#4     14:22:01.438  DLL/EXE  9832               C:\Windows\System32\kernel32.dll
#5     14:22:01.512  REG NEW  9832               \REGISTRY\USER\S-1-5-21-...\Software\...
#6     14:22:09.201  PROC -   9832
```

---

## Debugging

**DebugView (Sysinternals)** als Admin starten → Kernel-Messages aktivieren.
Der Treiber gibt folgende Meldungen aus:
```
[HyperMon] Loading...
[HyperMon] Loaded successfully.
[HyperMon] Unloading...
```

Falls der Driver nicht startet:
```cmd
# Fehlercode anzeigen:
sc start HyperMon
# oder:
Get-WinEvent -LogName System | Where-Object {$_.Message -like "*HyperMon*"}
```

Häufige Fehler:

| Code | Bedeutung | Lösung |
|------|-----------|--------|
| 0xC0000022 | `/integritycheck` fehlt | Linker-Flag setzen, neu bauen |
| 0xC000010E | Reboot nach Test-Signing | PC neu starten |
| 0xC0000428 | Signatur ungültig | `setup.ps1` nochmal ausführen |
| 0xC000003B | Pfad nicht gefunden | Absoluten Pfad in `sc create` prüfen |

---

## Projektstruktur

```
HyperMon/
├── driver/
│   ├── shared.h        ← IOCTL-Codes & Event-Structs (shared)
│   └── driver.cpp      ← Ring-0 Kernel Driver
├── usermode/
│   └── monitor.cpp     ← Usermode Console App
├── setup.ps1           ← Ein-Klick Setup
└── README.md
```

---

## Weiterführend / nächste Schritte

- **Event-Filtering** nach PID oder Pfad in der Console
- **File-System Minifilter** (eigener `FltRegisterFilter`) für File I/O
- **WFP Callout Driver** für Netzwerk-Callbacks
- **VMCS / VT-x** für echten Hypervisor (nächster Level nach diesem)
- Bücher: „Windows Internals 7th Ed." (Russinovich), „Rootkits & Bootkits"
