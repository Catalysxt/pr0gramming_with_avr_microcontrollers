# Pocket Casino — State Machine Diagrams

## Top-Level UI FSM

```mermaid
stateDiagram-v2
    [*] --> SPLASH : power on

    SPLASH --> MENU : any button press

    MENU --> SLOTS       : BTN_A (Slots selected)
    MENU --> COINFLIP    : BTN_A (Coin Flip selected)
    MENU --> HIGHERLOWER : BTN_A (Hi or Lo selected)
    MENU --> DICE        : BTN_A (Dice selected)

    SLOTS       --> MENU : BTN_B long-press
    COINFLIP    --> MENU : BTN_B long-press
    HIGHERLOWER --> MENU : BTN_B long-press
    DICE        --> MENU : BTN_B long-press

    SLOTS       --> GAMEOVER : credits == 0
    COINFLIP    --> GAMEOVER : credits == 0
    HIGHERLOWER --> GAMEOVER : credits == 0
    DICE        --> GAMEOVER : credits == 0

    GAMEOVER --> MENU : BTN_A (reset credits to 100)
```

---

## Slots Sub-FSM

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> IDLE     : BTN_B — cycle bet (1/5/10/25)
    IDLE --> SPINNING : BTN_A — deduct bet, randomise final positions

    SPINNING --> SPINNING : every 80 ms — advance reel animation frames
    note right of SPINNING
        Reel 1 stops at 800 ms
        Reel 2 stops at 1100 ms
        Reel 3 stops at 1400 ms
    end note
    SPINNING --> RESULT : all 3 reels stopped — evaluate payout

    RESULT --> IDLE : any button press
```

Payout table:

| Outcome | Multiplier | SFX |
|---|---|---|
| 3× sevens | 50× bet | JACKPOT |
| 3× any match | 10× bet | WIN |
| 2× any match | 2× bet | WIN |
| No match | 0 | LOSE |

---

## Coin Flip Sub-FSM

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> IDLE     : BTN_B — toggle HEADS / TAILS call
    IDLE --> FLIPPING : BTN_A — deduct 10 credits, start animation

    FLIPPING --> FLIPPING : every 80 ms — alternate coin glyph, rising pitch tick
    FLIPPING --> RESULT   : 600 ms elapsed — reveal random outcome

    RESULT --> IDLE : any button press
```

Animation detail: pitch starts at 400 Hz and rises 50 Hz per frame over ~7 frames.

---

## Higher or Lower Sub-FSM

```mermaid
stateDiagram-v2
    [*] --> GUESSING : randomise first card (1–13)

    GUESSING --> RESULT : BTN_A (guess Higher) — draw next card, evaluate
    GUESSING --> RESULT : BTN_B (guess Lower)  — draw next card, evaluate

    RESULT --> GUESSING : 1 s auto-advance OR any button press
    note right of RESULT
        Correct: +10 credits, streak++
        Wrong or Equal: streak resets, no credit change
        Streak 10: JACKPOT SFX
    end note
```

Card values: 1=A, 2–10=face value, 11=J, 12=Q, 13=K.
Equal value always loses.

---

## Dice Sub-FSM

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> IDLE    : BTN_B — cycle target (DOUBLES / OVER 7 / UNDER 7 / EXACT 7)
    IDLE --> ROLLING : BTN_A — deduct 5 credits, randomise dice, start animation

    ROLLING --> ROLLING : every 100 ms — update CGRAM pip pattern, reel tick SFX
    ROLLING --> RESULT  : 700 ms elapsed — evaluate sum / pattern against target

    RESULT --> IDLE : any button press
```

Payout table:

| Target | Win Condition | Multiplier |
|---|---|---|
| DOUBLES | d1 == d2 | 5× bet |
| OVER 7 | d1 + d2 > 7 | 2× bet |
| UNDER 7 | d1 + d2 < 7 | 2× bet |
| EXACT 7 | d1 + d2 == 7 | 4× bet |
