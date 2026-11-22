# 2D-Orbital-Launch-Simulator
2D, customisable, multi-stage rocket ascent and orbital insertion simulator in C (Visual Studios). A physics-based 2D launch vehicle simulator written in C that models multi-stage rocket ascent, atmospheric drag, inverse-square law of gravitation, variable mass depletion, Earth curvature, and orbital insertion.

---

## Features

* **Curved-Earth 2D Orbital Mechanics:** Tracks vehicle position in Earth-centered Cartesian coordinates (`X Pos`, `Y Pos`) alongside downrange angular displacement (`Earth Arc`).
* **Local-Horizon Pitch Guidance:** Automatically rotates user-defined pitch angles relative to the local horizon as the vehicle travels downrange.
* **Multi-Stage Vehicle Modeling:** Configurable dry mass, propellant mass, burn time, thrust, drag coefficient ($C_d$), and cross-sectional area per stage.
* **Programmable Flight Computer:** Execute sequential, timed guidance commands (duration, throttle, pitch angle, and stage decoupling) for each stage.
* **1-Second Telemetry Logging:** Exports full flight telemetry (`fl_data_*.txt`) with stage and command completion markers for post-flight analysis and plotting.

---

## Physics & Guidance Model

* **Initial State:** Launches from Earth's surface at $(X, Y) = (0.0\text{ m},\ 6,378,000.0\text{ m})$ with $0.0\text{ m/s}$ initial velocity.
* **Thrust Vectoring:** Pitch commands (`angle` in degrees, where `90°` is radially outward and `0°` is tangent to the local horizon) are transformed into global coordinates using the vehicle's downrange Earth arc (`arc`):
  ```cpp
  double thr_dir_x = cos(radian - arc);
  double thr_dir_y = sin(radian - arc);
  ```
* **Aerodynamics & Mass Flow:** Computes atmospheric density as a function of altitude and applies aerodynamic drag ($F_d = \frac{1}{2}\rho v^2 C_d A$) while updating vehicle mass every timestep based on throttle setting.

---

## Getting Started

### Prerequisites
* A C/C++ compiler (`g++`, `clang++`, or MSVC)

### Build & Run
```bash
git clone [https://github.com/](https://github.com/)<your-username>/<your-repo-name>.git
cd <your-repo-name>
g++ -O2 -std=c++17 main.cpp -o rocket_sim -lm
./rocket_sim
```

---

## Configuration & Flight Commands

### 1. Vehicle Stage Parameters
Define each stage using the following parameters:

| Parameter | Description | Units |
| :--- | :--- | :---: |
| `m_dry` | Structural dry mass (plus upper payload) | $\text{kg}$ |
| `m_fuel` | Usable propellant mass | $\text{kg}$ |
| `t_burn` | Rated full-throttle burn duration | $\text{s}$ |
| `thr` | Maximum engine thrust | $\text{N}$ |
| `C_d` | Aerodynamic drag coefficient | — |
| `crs_sct` | Reference cross-sectional area | $\text{m}^2$ |

### 2. Guidance Command Format
Each stage executes a list of sequential commands:
* **Duration (`s`):** Length of the command step.
* **Throttle (`0.0–1.0`):** Fraction of maximum thrust and fuel flow rate.
* **Angle (`deg`):** Target pitch angle relative to the local horizon (`90°` = vertical, `0°` = horizontal).
* **Decouple / Finish (`Y/N`):** Jettisons the current stage and ignites the next stage upon command completion.

---

## Example Preset: Saturn V to Low Earth Orbit (LEO)

### Vehicle Configuration
| Parameter | Stage 1 (S-IC) | Stage 2 (S-II + Interstage) | Stage 3 (S-IVB + IU + Apollo) |
| :--- | :---: | :---: | :---: |
| **`m_dry`** | `131000` kg | `40100` kg | `59000` kg |
| **`m_fuel`** | `2150000` kg | `444000` kg | `107000` kg |
| **`t_burn`** | `162` s | `360` s | `445` s |
| **`thr`** | `34500000` N | `5115000` N | `1023000` N |
| **`C_d`** | `0.35` | `0.30` | `0.25` |
| **`crs_sct`** | `80.1` $\text{m}^2$ | `80.1` $\text{m}^2$ | `34.2` $\text{m}^2$ |

### Orbital Flight Plan
| Stage | Cmd | Duration (s) | Throttle | Angle (deg) | Decouple (`Y/N`) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Stage 1** | 1 | `15` | `1.0` | `90` | `N` |
| **Stage 1** | 2 | `35` | `1.0` | `78` | `N` |
| **Stage 1** | 3 | `50` | `1.0` | `62` | `N` |
| **Stage 1** | 4 | `35` | `1.0` | `48` | `N` |
| **Stage 1** | 5 | `33` | `0.8` | `38` | `Y` |
| **Stage 2** | 1 | `120` | `1.0` | `34` | `N` |
| **Stage 2** | 2 | `120` | `1.0` | `24` | `N` |
| **Stage 2** | 3 | `120` | `0.9` | `14` | `Y` |
| **Stage 3** | 1 | `150` | `1.0` | `6` | `N` |
| **Stage 3** | 2 | `135` | `1.0` | `2` | `Y` |

---

## Telemetry Output

Each simulation run outputs a tab-separated file (`fl_data_*.txt`) containing:

| Column | Description |
| :--- | :--- |
| `Time (s)` | Elapsed mission time since liftoff |
| `Velocity (ms-1)` | Total inertial speed |
| `Altitude (m)` | Height above Earth's surface ($R_\oplus = 6,378,000\text{ m}$) |
| `Mass (kg)` | Current total vehicle mass |
| `Air Density (kgm-3)` | Local atmospheric density |
| `X Pos` / `Y Pos` | Earth-centered 2D coordinates ($\text{m}$) |
| `Earth Arc (rad)` | Downrange angular distance traveled around Earth |

---

## Future Improvements

* Automated gravity-turn and closed-loop orbital guidance (e.g., PEG / Q-guidance)
* Multi-burn mission profiles (parking orbit coast + Trans-Lunar Injection)
* Support for rotating Earth surface velocity boost at launch
* Built-in Python visualization script for trajectory and orbital element analysis

---

## License

Distributed under the MIT License. See `LICENSE` for more information.
