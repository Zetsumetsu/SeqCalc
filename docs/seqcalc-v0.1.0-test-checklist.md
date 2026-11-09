# SeqCalc v0.1.0 test checklist - JUCE VST3 port of Steve Zydek's SeqCalc - Forte lookups, PC-set entry, pattern language, Calc to Send, MIDI out, drag export

Build: v0.1.0
JUCE: 8.0.6 (FetchContent)
Date: 2026-11-08

Date tested: 
Live set used: 

_Tick each box as you test. This file saves in place, inside the build documentation._

## Forte lookups - the six fixed rows

- [ ] 1. Fresh Live set, fresh plugin load. The note display reads C4 D#4 G4 (the default minor triad).
- [ ] 2. Type 5-13 in the Forte field and press CalcS. The PC-set field becomes the 5-13 prime form and the display shows its notes.
- [ ] 3. Type 6-2 in the Forte field and press CalcS. The display shows the 6-2 prime form notes.
- [ ] 4. Type 6-31 in the Forte field and press CalcS. The PC-set field reads 0 1 3 5 8 9 and the display reads C4 C#4 D#4 F4 G#4 A4.
- [ ] 5. Type 6-35 in the Forte field and press CalcS. The PC-set field reads 0 2 4 6 8 10.
- [ ] 6. Type 6-Z12 in the Forte field and press CalcS. The PC-set field reads 0 1 2 4 6 7 (the prime form fix, not the old interval vector 333231).
- [ ] 7. Type 6-Z43 in the Forte field and press CalcS. The display shows the 6-Z43 prime form notes.
- [ ] 8. Type a bogus Forte number (9-99) and press CalcS. The display shows an error in magenta and the previous pattern keeps playing.

## Forte lookups - the six formerly-Rahn rows (Forte primes throughout)

- [ ] 9. Type 5-20 and press CalcS. The PC-set field reads 0 1 3 7 8.
- [ ] 10. Type 6-Z29 and press CalcS. The PC-set field reads 0 1 3 6 8 9.
- [ ] 11. Type 7-Z18 and press CalcS. The PC-set field reads 0 1 2 3 5 8 9.
- [ ] 12. Type 7-20 and press CalcS. The PC-set field reads 0 1 2 4 7 8 9.
- [ ] 13. Type 8-26 and press CalcS. The PC-set field reads 0 1 2 4 5 7 9 10.

## PC-set entry - all three ways

- [ ] 14. Grid: press the PC action button, then toggle grid buttons 0, 4 and 7 on and off. The PC-set field and the magenta grid buttons follow each toggle, and the display goes stale until CalcS.
- [ ] 15. Typable field: type 0 2 5 in the PC-set field and press Enter, then CalcS. The display reads C4 D4 F4 and the grid buttons 0, 2 and 5 light up.
- [ ] 16. Typable field order: type 5 2 0 and press CalcS. The display reads F4 D4 C4, the same notes in the order typed.
- [ ] 17. Forte number plus CalcS: type 6-31, press CalcS, and the prime form appears in the PC-set field. Now edit the set by hand and press CalcS again: your edit stays (the Forte field cleared after the first CalcS).
- [ ] 18. Grid actions Root, Roct, Range: press Root then grid button 5, press CalcS, and the display starts on F4. Press Roct then grid button 6, press CalcS, and the notes move up two octaves. Press Range then grid button 2, press CalcS, and the set spans two octaves.
- [ ] 19. clrS clears both the Forte and PC-set fields and the display empties. RESET restores the default clip string (C4 D#4 G4, pattern xxxx, 4n, repeat 1).

## Pattern language

- [ ] 20. Pattern xxxx at 4n: press SEND and the external instrument plays four quarter notes, C4 D#4 G4 C4, then loops.
- [ ] 21. Pattern x_x- at 4n: the first note sustains two beats, then a rest. No note sounds on beats 3 or 4.
- [ ] 22. Pattern [xx]x at 4n: the first beat holds two eighth notes, then a quarter note. The bar is two beats long.
- [ ] 23. Pattern with R: type xRxR with R In Scale on and press CalcS, then SEND several times. The R steps play notes from the current set at a softer velocity.
- [ ] 24. Subdiv: set the pattern to xx and the subdiv to 8n. The two notes take one beat total. Set subdiv to 16n and they take half a beat.
- [ ] 25. Repeat: set the pattern to xx and Repeat to 3, press CalcS, and the pattern plays six notes (three repeats) before looping.
- [ ] 26. Shuffle: turn Shuffle on, press CalcS, and the note order changes. Turn it off and CalcS again to restore the written order.

## The five behaviour toggles

- [ ] 27. R In Scale off: with R in the pattern, the random notes come from outside the current set (chromatic notes not in the set).
- [ ] 28. Root Only on: the pattern plays the root note only, whatever the set is.
- [ ] 29. Rests For R on: the R steps become random rests instead of random notes.
- [ ] 30. Use PC Set off: the scale menus take over. Pick root C, octave 3, scale Major, press CalcS, and the display reads C3 D3 E3 F3 G3 A3 B3.
- [ ] 31. Stale rule: change any input after a CalcS and the STALE label appears in magenta. Press CalcS and it clears.

## Calc to Send flow

- [ ] 32. Type a new set and a new pattern, press CalcS, and the note display and piano strip update together.
- [ ] 33. Press SEND without pressing CalcS after an edit: the old calculated pattern plays, not the edited inputs.
- [ ] 34. Press CalcS while the pattern is playing: playback restarts from step 1 with the new pattern.

## MIDI-out playback and transport slaving

- [ ] 35. Standalone play: with the Live transport stopped, press SEND. The pattern plays from step 1 out of the MIDI output into the external instrument.
- [ ] 36. VST Stop: press STOP. The notes stop immediately with no hanging notes, and the external instrument is silent.
- [ ] 37. DAW start: start the Live transport. The pattern starts at step 1 and follows the Live tempo.
- [ ] 38. DAW stop: stop the Live transport. Playback stops and the notes are silenced.
- [ ] 39. Latch: while the Live transport runs, press STOP, then start the transport again from the top. The pattern does NOT restart on its own (the VST Stop latches). Press SEND and it plays again.
- [ ] 40. Latch cleared by DAW stop: stop the Live transport, then start it again. The pattern starts at step 1 this time.
- [ ] 41. Loop: loop a section in Live and let it cycle. The pattern restarts at step 1 at the top of each loop pass.
- [ ] 42. Save the Live set, close it, reopen it: the PC set, pattern, Forte field, toggles and menus all come back, and the calculated pattern is intact.

## Drag MIDI export

- [ ] 43. Drag the Drag MIDI to DAW handle onto a MIDI track in Live. A MIDI clip appears with the calculated notes, at the right pitches and lengths.
- [ ] 44. Drag twice in a row after changing the pattern: the second clip holds the new pattern, not a stale copy.
- [ ] 45. Drag with an empty pattern (clrS, then CalcS): nothing lands in Live, and no error appears.

## Editor layout vs the device

- [ ] 46. The editor shows the six action buttons, the 12-button grid, the PC-set / Forte / pattern fields, the scale menus, the five toggles, CalcS / SEND / STOP, the note display and the piano strip, in the same arrangement as the Max device.
- [ ] 47. The piano strip highlights exactly the calculated notes in magenta, and follows a new CalcS.
- [ ] 48. The active action button and the active grid buttons are magenta. No orange, amber or copper is used as a meaningful colour anywhere.
- [ ] 49. Resize the Live window and reopen the plugin: the editor opens at its full size with nothing clipped.

## The running gate

- [ ] 50. Play for real: build a short passage with SeqCalc driving an external instrument, using at least one Forte lookup and one bracketed rhythm. v0.1.0 earns a clean sheet only with no new notes asking for changes or fixes.

## Exceptions and notes

(write them here)
