# The Hollow Crown
## Cutscene Map and Script Bible (sample)

A small invented sample in the script-bible format Cinemafy reads: a scene map with acts, then every scene's contract and performance script.

## 1. Campaign cutscene coverage

| Campaign entry | Required scene IDs | Count |
|---|---|---|
| HC-01 · The Gate | HC-01-CS01 to HC-01-CS02 | 2 |

## 2. Mandatory scene map

### Prologue

| Scene | Type / target | Mandatory trigger | Control handoff |
|---|---|---|---|
| [HC-PRO-CS01 · The Old Song](#hc-pro-cs01) | Locked cinematic / 0:40 | New game | None → None |

### Act 1: The Gate

| Scene | Type / target | Mandatory trigger | Control handoff |
|---|---|---|---|
| [HC-01-CS01 · Up the Stair](#hc-01-cs01) | Gameplay-to-scene bridge / 1:30 | Arin reaches the gate stair | None → Arin |
| [HC-01-CS02 · The Warden's Record](#hc-01-cs02) | Mandatory record / Witness / 1:10 | Arin touches the seal | Arin → Arin |

## 3. Complete performance scripts

### PROLOGUE

#### HC-PRO-CS01

**The Old Song**

**Source:** Prologue; printed p. 1. A voice remembers the crown before it was hollow  
**Type / planning time:** Locked cinematic / 0:40; 12 spoken words.  
**Location / cast:** Ruined throne hall, the Hollow Keep; Unseen singer.  
**Trigger:** New game  
**Entry state:** Nothing established  
**Control:** None → None.

**Camera and performance:** Extreme close on dust in a shaft of light. Never show the singer.

**Required dramatic beats:** The crown is lost; Someone still remembers.

**Performance script**

*Dust turns in a shaft of light above an empty throne.*

**ON-SCREEN DISPLAY [A]:** LONG AGO

**SINGER [M · Prologue]:** The crown was never gold.

**SINGER [M · Prologue]:** It was a promise.

**Canonical completion state (play or skip, after all gameplay prerequisites):**

- Prologue seen

**Return to play / next scene:** Begin HC-01 at the gate stair.

**Continuity and compression notes:** Framing only.

---

### HC-01 · The Gate

#### HC-01-CS01

**Up the Stair**

**Source:** Chapter 1; printed p. 4. Arin and Vex climb to the gate, arguing about the warden  
**Type / planning time:** Gameplay-to-scene bridge / 1:30; 40 spoken words.  
**Location / cast:** Gate stair, Hollow Keep outer ward; Arin Tal, Vex Morrow.  
**Trigger:** Arin reaches the gate stair  
**Entry state:** Arin carries the iron key  
**Control:** None → Arin.

**Camera and performance:** Warm two-shot on the stair; let the wind carry the first line.

**Required dramatic beats:** Vex doubts the warden; Arin climbs anyway.

**Performance script**

*Wind tears at the banners. Arin looks up at the gate.*

**VEX [M · Chapter 1]:** You trust a dead man's map?

**ARIN [M · Chapter 1]:** I trust the warden.

*Arin starts up the stair, rising into player control.*

**VEX [A]:** Then climb faster.

*Playable climbing continues past the broken statues.*

**VEX [M · Chapter 1]:** There. The seal.

*Arin's point of view settles on the cracked seal above the gate.*

**ARIN [M · Chapter 1]:** Still warm.

**Canonical completion state (play or skip, after all gameplay prerequisites):**

- Arin reaches the seal
- Vex still doubts the warden

**Return to play / next scene:** Arin touches the seal.

**Continuity and compression notes:** Climbing time is additional.

---

#### HC-01-CS02

**The Warden's Record**

**Source:** Chapter 2; printed p. 9. The seal plays the warden's last record  
**Type / planning time:** Mandatory record / Witness / 1:10; 30 spoken words.  
**Location / cast:** Gate seal, Hollow Keep; Arin Tal, Vex Morrow, Warden (record).  
**Trigger:** Arin touches the seal  
**Entry state:** The seal is warm  
**Control:** Arin → Arin.

**Camera and performance:** Slow push in on the seal as the record wakes.

**Required dramatic beats:** The warden chose the gate; Vex is shaken.

**Performance script**

*Light spills from the seal and becomes the warden.*

**WARDEN (RECORD) [M · Chapter 2]:** If you hear this, the gate held.

**VEX [M · Chapter 2]:** He knew.

**ARIN [M · Chapter 2]:** He chose.

**Canonical completion state (play or skip, after all gameplay prerequisites):**

- Warden's record heard
- Gate open

**Return to play / next scene:** Arin and Vex pass the gate.

**Continuity and compression notes:** Record plays once.

---
