# How It Works

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
