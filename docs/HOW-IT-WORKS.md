# How It Works

## How does this compare to other accent input methods?

Typing accented characters on Linux has long been possible — but every existing method breaks the touch-typing flow somewhere. This addon was built to fill that gap.

| Method | Rhythm break? | Mode switch? | Layout switch? | Learning curve |
|---|---|---|---|---|
| Compose key | Yes (syntax `" a`) | Yes | No | Medium |
| Dead keys | Yes (dedicated key) | Yes | No | Small |
| AltGr layouts (Neo, US-Intl) | No | No | **Yes** (entire layout) | High |
| Mobile long-press (Android/iOS) | Yes (popup wait) | Yes | — | Small |
| **Schnelle Umlaute** | **No** | **No** | **No** | Small |

**Not a mobile long-press on the desktop.** On a phone you press a key and *wait* 200–500 ms for a popup to appear, then tap or swipe to pick a variant. This addon has **no wait**: you press the mapped key and the leader key in the natural rhythm of touch typing — both keypresses can overlap, exactly as they already do while typing any two adjacent letters quickly.

**Why this matters for touch typists.** The *hold-letter-then-press-leader* pattern is not a new motor skill. A touch typist already produces small timing overlaps between neighbouring finger movements all day long. The addon gives that existing overlap a meaning; it does not ask the typist to slow down, switch modes, or relearn a keyboard layout.

## Gesture Flow

```mermaid
stateDiagram-v2
    [*] --> Waiting: Press 'a'
    Waiting --> Umlaut: Leader key within 400ms
    Waiting --> Normal: Release or timeout
    Umlaut --> [*]: ä ✨
    Normal --> [*]: a
```

**Note:** Leader key is <kbd>Space</kbd> by default. You can configure it to Arrow keys (<kbd>←</kbd><kbd>→</kbd><kbd>↑</kbd><kbd>↓</kbd>), <kbd>Alt</kbd>/<kbd>AltGr</kbd>, or custom keys in `fcitx5-config-qt`.

## Why Does Typing Feel Different?

This addon works differently than normal typing. Understanding this helps you adapt faster.

### Scenario 1: Normal Letter (unmapped, e.g. 'b', 'c', 'd')

```mermaid
graph LR
    N1["🔽 Press 'b'"] -->|instant| N2["'b' on screen ✓"]
    N2 --> N3["🔼 Release 'b'"]
    N3 -.->|no action| N4["Done"]

    style N1 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style N2 fill:#c8e6c9,stroke:#388e3c,stroke-width:3px,color:#000
    style N3 fill:#f5f5f5,stroke:#757575,stroke-width:2px,color:#000
    style N4 fill:#f5f5f5,stroke:#757575,stroke-width:2px,color:#000
```

**Timing:** 0ms delay - Output on **Press** ✓

---

### Scenario 2: Mapped Letter (a, o, u, s) - The Decision Point

The addon intercepts the key and waits to see what happens next:

```mermaid
graph TD
    A1["🔽 Press mapped key 'o'"] --> A2["Addon intercepts key<br/>⏳ Waiting for decision..."]
    A2 --> A3{What happens next?}
    A3 -->|"🔼 Key Release"| A4["Output 'o'<br/>normal letter"]
    A3 -->|"⎵ Leader Key"| A5["Output 'ö'<br/>umlaut ✓"]
    A3 -->|"⏰ Timeout 400ms"| A6["Output 'o'<br/>fallback"]

    style A1 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style A2 fill:#fff9c4,stroke:#f57f17,stroke-width:3px,color:#000
    style A3 fill:#fff3e0,stroke:#e65100,stroke-width:3px,color:#000
    style A4 fill:#ffccbc,stroke:#d84315,stroke-width:3px,color:#000
    style A5 fill:#c8e6c9,stroke:#388e3c,stroke-width:3px,color:#000
    style A6 fill:#ffccbc,stroke:#d84315,stroke-width:3px,color:#000
```

**Timing:** 100-400ms delay - Output depends on user action ⚠

---

### The Critical Difference: Timing Expectation

```mermaid
sequenceDiagram
    participant User
    participant Addon
    participant Screen

    Note over User,Screen: Normal letter (unmapped)
    User->>Screen: Press 'b' → appears INSTANTLY
    Note right of Screen: ✓ 0ms delay

    Note over User,Screen: ━━━━━━━━━━━━━━━━━━━━━━━━━━

    Note over User,Screen: Mapped letter
    User->>Addon: Press 'o'
    Note right of Addon: Intercepted, waiting...
    alt Leader Key pressed
        User->>Addon: Press Space
        Addon->>Screen: 'ö' (umlaut) ✓
    else Key released / Timeout
        User->>Addon: Release 'o'
        Addon->>Screen: 'o' (normal) ⚠
    end
    Note right of Screen: 100-400ms delay
```

### Quick Comparison

| Action | Normal Letter | Mapped Letter (a,o,u,s) |
|--------|--------------|------------------------|
| **Output Trigger** | Key **Press** | Key **Release** or Leader Key |
| **Timing** | Instant (0ms) | Delayed (100-400ms) |
| **Feel** | Direct feedback | Slight "lag" |

**Why the delay?** The addon must wait after a mapped key press to determine whether the leader key follows (→ accent) or the key is simply released (→ normal letter).

---

### Scenario 3: Mapped Letter → Next Key (Fast Typing Shortcut)

When you type the next character before releasing the mapped key, the addon doesn't wait — it commits the mapped letter **immediately** and lets the next key through. This avoids the release delay entirely:

```mermaid
graph TD
    B1["🔽 Press mapped key 'o'"] --> B2["Addon intercepts key<br/>⏳ Waiting for decision..."]
    B2 --> B3["🔽 Press next key 'k'"]
    B3 --> B4["Addon decides:<br/>'k' is not a leader key"]
    B4 --> B5["Output 'o' instantly<br/>+ 'k' passes through"]

    style B1 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style B2 fill:#fff9c4,stroke:#f57f17,stroke-width:3px,color:#000
    style B3 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style B4 fill:#fff3e0,stroke:#e65100,stroke-width:3px,color:#000
    style B5 fill:#c8e6c9,stroke:#388e3c,stroke-width:3px,color:#000
```

**Timing:** Near-instant — no need to wait for release or timeout. The next key press resolves the decision immediately. This means fast typists rarely notice the delay: as long as you keep typing, mapped letters flow through without any perceivable lag.

---

### Scenario 4: Accent Cycling (Multiple Variants)

When a mapping has multiple outputs (e.g. `a → ä, à, á, â, ã`), pressing the leader key repeatedly cycles through them:

```mermaid
sequenceDiagram
    participant User
    participant Addon
    participant Screen

    User->>Addon: 🔽 Hold 'a'
    Note right of Addon: Intercepted, waiting...
    User->>Addon: ⎵ Press leader
    Addon->>Screen: Preview: ä (1st variant)
    User->>Addon: ⎵ Press leader again
    Addon->>Screen: Preview: à (2nd variant)
    User->>Addon: ⎵ Press leader again
    Addon->>Screen: Preview: á (3rd variant)
    User->>Addon: 🔼 Release 'a'
    Addon->>Screen: Commit: á ✓
```

The preview updates in the preedit area — nothing is committed until you release the input key. You can cycle as many times as you want.

---

### Scenario 5: New Mapped Key During Gesture

When you press a second mapped key while the first is still waiting, the addon commits the first one immediately and starts a new gesture for the second:

```mermaid
graph TD
    C1["🔽 Press 'a'"] --> C2["Addon intercepts 'a'<br/>⏳ Waiting..."]
    C2 --> C3["🔽 Press 's'<br/>(also mapped)"]
    C3 --> C4["Addon commits 'a' instantly<br/>Starts new gesture for 's'"]
    C4 --> C5{What happens next?}
    C5 -->|"⎵ Leader Key"| C6["Output 'ß' ✓"]
    C5 -->|"🔼 Release 's'"| C7["Output 's'"]

    style C1 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style C2 fill:#fff9c4,stroke:#f57f17,stroke-width:3px,color:#000
    style C3 fill:#e3f2fd,stroke:#1976d2,stroke-width:3px,color:#000
    style C4 fill:#c8e6c9,stroke:#388e3c,stroke-width:3px,color:#000
    style C5 fill:#fff3e0,stroke:#e65100,stroke-width:3px,color:#000
    style C6 fill:#c8e6c9,stroke:#388e3c,stroke-width:3px,color:#000
    style C7 fill:#ffccbc,stroke:#d84315,stroke-width:3px,color:#000
```

This means typing `a` `s` quickly just outputs "as" — you'd have to deliberately hold `a` and press Space to get "ä".
