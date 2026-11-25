# Better Startpos

A faster startpos switcher that can also link a practice copy of a level to the level you're trying to beat.

## Switching startposes

- Press **Left** / **Right** (rebindable) to move between startposes. The display at the bottom of the screen shows where you are, like `< 4/21 >`. You can click its arrows too.
- Press a key several times quickly and the level restarts only once, after you stop.
- Each startpos keeps its saved state, so going back to one you've already used is near-instant.
- **Go to:** press **G**. The level freezes, you type a startpos number, then press Enter (or G again) to jump straight there. Esc cancels.
- **Fast SP:** tick the box under the Play button and every startpos is prepared before the level starts, so every switch is near-instant. Esc skips the preparing.
- Startposes you place while playing (with GD's own pause-menu tool) are included too.
- Starting from a startpos uses test mode, so it never counts as progress. Starting from 0 is a normal attempt.

## Smart startpos

Startposes are often placed with the wrong speed, gamemode, size, gravity, dual or mirror. With **Smart startpos** on (it is by default), each startpos uses what the level's portals before it say, so you start the way you would when playing from 0.

Gravity orbs and pads depend on your clicks, so no mod can know whether they flipped you. After one of those, a startpos keeps its own gravity until you've played past it once. From then on it uses the gravity you actually had there.

The level itself is never changed.

## Linked levels

Practicing on a copy of a level full of startposes? Link it to the real level (the **Target**):

- On the Practice level, going back past the first startpos takes you to the Target level, so runs from 0 count.
- On the Target level, Right takes you to the Practice level's first startpos, and Left to its last.
- The other level downloads in the background, so swapping is quick.
- **Seamless swap** keeps the level on screen while the other one loads. **Keep Practice loaded** makes swapping back instant.

Open the links from the **Better Startpos** button in the pause menu.

## Settings

From the same pause menu button:

- **Default startpos:** where a level starts when you open it: Last (what GD does on its own), First, From 0, or a number. Set it for all levels, or give one level its own.
- **Smart startpos**, and **Fast SP on every Play**.

All settings apply as soon as you unpause. You never need to reopen the level.

## Notes

- If you use another startpos switcher (such as Mega Hack's) on the same keys, turn one of them off. Better Startpos warns you once if it notices.
- Made by **dwerked**. Report bugs and ideas on [GitHub](https://github.com/dwerked/better-startpos/issues).
