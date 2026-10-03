# EldenRing-FluidControls

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

[English](#english) | [Русский](#русский)

## English

A DLL mod for Elden Ring that changes keyboard controls and lock-on behaviour: dodge on key press, sprint on a separate key, sprint without stamina cost, and a lock-on that stays on the chosen target.

Every feature has its own switch in `ErFluidControls.ini`, so you can take only the lock-on fixes or only the sprint key.

Tested on `eldenring.exe` 2.7.1 with me3 0.13.0, The Convergence and Seamless Co-op. The memory patches are found by byte signature. If a signature is not found after a game update, that patch is skipped and the rest of the mod keeps working.

> [!WARNING]
> DLL mods do not load with Easy Anti-Cheat running. Play offline with EAC disabled (Mod Engine and me3 do this for you), or use Seamless Co-op.

### Features

**Instant dodge.** The game rolls when Space is released, because holding the same key means sprint. With `InstantDodge` the mod swallows the physical Space press and sends a short tap of `TapDurationMs` instead, so the roll starts on press.

**Sprint key.** With `ToggleSprint` or `HoldSprint` the key from `SprintKey` controls sprint: the mod holds a virtual Space for you. `ToggleSprint` switches sprint on and off with one press, `HoldSprint` sprints while the key is down. `SmartWASD` drops sprint when no movement key is pressed. `MinHoldDurationMs` keeps the virtual Space held long enough for the game to read it as sprint and not as a roll. `ResumeAfterRoll` turns sprint back on after a dodge.

**Free sprint.** `InfiniteSprintStamina` removes the stamina cost of sprinting, on foot and on Torrent. Rolls, attacks and blocking cost stamina as usual. The mod patches the drain code; if that code is not found, it falls back to refilling stamina while the sprint key is active.

**Lock-on.** `DisableTargetSwitch` stops the lock from jumping to another enemy on mouse movement or by itself; press the lock-on key again to pick a new target. `PreventLockBreak` keeps the lock when the target is briefly hidden behind a wall, a pillar or another enemy.

**Chat.** With `ChatSuspend`, Enter pauses the mod's hotkeys and the next Enter resumes them, so typing in Seamless Co-op chat does not trigger rolls.

The keyboard features assume the default bindings: Space for dodge and sprint, WASD for movement. They act only while the game window is in the foreground. The gamepad is not affected.

### Installation

Take `ErFluidControls.dll` and `ErFluidControls.ini` from the [`release`](release) folder.

**me3**: put both files into the `dll` folder of your mod and add the DLL to the profile:

```toml
[[natives]]
path = './../mod/dll/ErFluidControls.dll'
```

**Mod Engine 2**: add the DLL path to `external_dlls` in `config_eldenring.toml`.

**[Elden Mod Loader](https://www.nexusmods.com/eldenring/mods/117)**: put both files into `ELDEN RING\Game\mods`.

### Configuration

`ErFluidControls.ini` lives next to the DLL and is read once at startup. Booleans accept `true` or `1`. The shipped ini turns the lock-on fixes and free sprint on and leaves dodge and the sprint key off.

| Section | Key | Shipped value | Description |
| --- | --- | --- | --- |
| `[Dodge]` | `InstantDodge` | `false` | Roll on Space press instead of release |
| `[Dodge]` | `TapDurationMs` | `25` | Length of the injected Space tap, ms |
| `[Sprint]` | `ToggleSprint` | `false` | One press of the sprint key switches sprint on or off |
| `[Sprint]` | `HoldSprint` | `false` | Sprint while the sprint key is held |
| `[Sprint]` | `SprintKey` | `LeftAlt` | `LeftAlt`, `LeftShift` or `CapsLock` |
| `[Sprint]` | `SmartWASD` | `true` | Stop sprinting when no WASD key is pressed |
| `[Sprint]` | `MinHoldDurationMs` | `380` | Minimum time the virtual Space stays held, ms |
| `[Sprint]` | `ResumeAfterRoll` | `false` | Resume sprint after a roll |
| `[Sprint]` | `InfiniteSprintStamina` | `true` | Sprinting costs no stamina |
| `[Input]` | `ChatSuspend` | `true` | Enter pauses and resumes the mod's hotkeys |
| `[LockOn]` | `DisableTargetSwitch` | `true` | No target switching on mouse movement or by itself |
| `[LockOn]` | `PreventLockBreak` | `true` | Keep the lock when the target is briefly out of sight |
| `[Debug]` | `Logging` | `false` | Write `ErFluidControls.log` next to the DLL |

Without the ini file the mod uses the same values, except that `InstantDodge` and `ToggleSprint` are on.

If a feature does not work after a game update, set `Logging = true` and look for `pattern not found` in the log.

### Building

MinGW-w64 g++ with C++20 support is required. From the repository root:

```powershell
.\build.ps1
```

The script compiles the four files from `src` into a statically linked `ErFluidControls.dll`.

### License

MIT, see [LICENSE](LICENSE). Not affiliated with FromSoftware or Bandai Namco.

## Русский

DLL-мод для Elden Ring, который меняет управление с клавиатуры и поведение захвата цели: перекат по нажатию, бег на отдельной клавише, бег без расхода выносливости и захват, который не слетает с выбранной цели.

Каждая функция включается отдельно в `ErFluidControls.ini`: можно взять только правки захвата или только клавишу бега.

Проверено на `eldenring.exe` 2.7.1 с me3 0.13.0, The Convergence и Seamless Co-op. Патчи памяти ищутся по сигнатурам. Если после обновления игры сигнатура не найдена, этот патч пропускается, остальное продолжает работать.

> [!WARNING]
> С включённым Easy Anti-Cheat DLL-моды не загружаются. Играйте офлайн с отключённым EAC (Mod Engine и me3 делают это сами) или через Seamless Co-op.

### Возможности

**Мгновенный перекат.** Игра делает перекат при отпускании Space, потому что удержание той же клавиши означает бег. С `InstantDodge` мод перехватывает физическое нажатие Space и отправляет короткое нажатие длиной `TapDurationMs` — перекат начинается сразу.

**Клавиша бега.** С `ToggleSprint` или `HoldSprint` бегом управляет клавиша из `SprintKey`: мод сам удерживает виртуальный Space. `ToggleSprint` включает и выключает бег одним нажатием, `HoldSprint` — бег, пока клавиша зажата. `SmartWASD` отключает бег, когда не нажата ни одна клавиша движения. `MinHoldDurationMs` удерживает виртуальный Space достаточно долго, чтобы игра сочла это бегом, а не перекатом. `ResumeAfterRoll` возвращает бег после переката.

**Бесплатный бег.** `InfiniteSprintStamina` убирает расход выносливости на бег — пешком и на Потоке. Перекаты, атаки и блок тратят выносливость как обычно. Мод патчит код расхода; если этот код не найден, он восполняет выносливость, пока активна клавиша бега.

**Захват цели.** `DisableTargetSwitch` не даёт захвату перескакивать на другого врага от движения мыши или самопроизвольно; чтобы выбрать новую цель, нажмите клавишу захвата ещё раз. `PreventLockBreak` сохраняет захват, когда цель ненадолго скрылась за стеной, колонной или другим врагом.

**Чат.** С `ChatSuspend` клавиша Enter приостанавливает горячие клавиши мода, следующий Enter возвращает их — набор текста в чате Seamless Co-op не вызывает перекаты.

Клавиатурные функции рассчитаны на стандартную раскладку: Space — перекат и бег, WASD — движение. Они работают, только пока окно игры активно. На геймпад мод не влияет.

### Установка

Возьмите `ErFluidControls.dll` и `ErFluidControls.ini` из папки [`release`](release).

**me3**: положите оба файла в папку `dll` вашего мода и добавьте DLL в профиль:

```toml
[[natives]]
path = './../mod/dll/ErFluidControls.dll'
```

**Mod Engine 2**: добавьте путь к DLL в `external_dlls` в `config_eldenring.toml`.

**[Elden Mod Loader](https://www.nexusmods.com/eldenring/mods/117)**: положите оба файла в `ELDEN RING\Game\mods`.

### Настройка

`ErFluidControls.ini` лежит рядом с DLL и читается один раз при запуске. Логические значения — `true` или `1`. В поставляемом ini включены правки захвата и бесплатный бег, перекат и клавиша бега выключены.

| Секция | Ключ | Значение в поставке | Описание |
| --- | --- | --- | --- |
| `[Dodge]` | `InstantDodge` | `false` | Перекат по нажатию Space, а не по отпусканию |
| `[Dodge]` | `TapDurationMs` | `25` | Длина отправляемого нажатия Space, мс |
| `[Sprint]` | `ToggleSprint` | `false` | Одно нажатие клавиши бега включает или выключает бег |
| `[Sprint]` | `HoldSprint` | `false` | Бег, пока клавиша бега зажата |
| `[Sprint]` | `SprintKey` | `LeftAlt` | `LeftAlt`, `LeftShift` или `CapsLock` |
| `[Sprint]` | `SmartWASD` | `true` | Прекращать бег, когда не нажата ни одна из WASD |
| `[Sprint]` | `MinHoldDurationMs` | `380` | Минимальное время удержания виртуального Space, мс |
| `[Sprint]` | `ResumeAfterRoll` | `false` | Возобновлять бег после переката |
| `[Sprint]` | `InfiniteSprintStamina` | `true` | Бег не тратит выносливость |
| `[Input]` | `ChatSuspend` | `true` | Enter приостанавливает и возвращает горячие клавиши мода |
| `[LockOn]` | `DisableTargetSwitch` | `true` | Цель не переключается от мыши и сама по себе |
| `[LockOn]` | `PreventLockBreak` | `true` | Захват сохраняется, когда цель ненадолго не видна |
| `[Debug]` | `Logging` | `false` | Писать `ErFluidControls.log` рядом с DLL |

Без ini-файла мод берёт те же значения, но `InstantDodge` и `ToggleSprint` включены.

Если после обновления игры функция перестала работать, поставьте `Logging = true` и найдите в логе `pattern not found`.

### Сборка

Нужен MinGW-w64 g++ с поддержкой C++20. Из корня репозитория:

```powershell
.\build.ps1
```

Скрипт собирает четыре файла из `src` в статически слинкованный `ErFluidControls.dll`.

### Лицензия

MIT, см. [LICENSE](LICENSE). Проект не связан с FromSoftware и Bandai Namco.
