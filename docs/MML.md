# SGE MML Commands Reference

This document is a work in progress as of 2025/03/09.

## Basic Commands (Quick Reference)

Real commands:
| Command | Effect                               |
| ------- | ------------------------------------ |
| a..g    | Note (Note: Octaves start at note C) |
| r       | Rest                                 |
| >       | Octave up                            |
| <       | Octave down                          |
| o       | Octave change (5 = Middle-C octave)  |
| v       | Velocity                             |
| V       | Volume                               |
| E       | Expression                           |
| P       | Panning                              |
| T       | Tuning (pitch bend)                  |
| t       | Tempo (for all tracks)               |
| @       | Program                              |

Pseudo-commands:
| Command | Effect                                              |
| ------- | --------------------------------------------------- |
| #       | Begin track (followed by track name until new line) |
| l       | Default note duration                               |

Notes:
 * Commands are case-sensitive, as there are some letter overlaps.

## Extended Commands (Quick Reference)

| Command         | Effect                                                 |
| --------------- | ------------------------------------------------------ |
| (s)[...]        | Pattern definition                                     |
| (s)[...]i       | Pattern definition (with repeat)                       |
| (s)             | Pattern call                                           |
| (s)i            | Pattern call (with repeat)                             |
| [...]i          | Pattern definition (anonymous, with repeat)            |
| *               | Pattern recall (on last-defined pattern)               |
| *i              | Pattern recall (on last-defined pattern, with repeat)  |
| [[...]]i        | Section repeat                                         |
| |               | Pattern/repeat break (for last iteration)              |
| :s              | Label                                                  |
| $goto(Target)   | Jump (Target = Jump target)                            |
| $if([i,]Target) | Conditional jump (Target = Jump target)                |
| $signal([i])    | External call                                          |
| $priority = i   | Track priority (-8 .. +7)                              |
| $transpose = i  | Set note transposition                                 |
| $transpose += i | Add to note transposition                              |
| $transpose -= i | Sutract from note transposition                        |
| $portamento+    | Enable portamento                                      |
| $p+             | Enable portamento                                      |
| $portamento-    | Disable portamento                                     |
| $p-             | Disable portamento                                     |
| q[+i]           | Note duration quantization (multiply-add)              |
| qi[+i]          | Note duration quantization (multiply-add)              |
| qi/i[+i]        | Note duration quantization (multiply-add)              |

Where:
 * s: String  (alphanumeric characters only; case-sensitive)
 * i: Integer (decimal or hexadecimal)

Notes:
 * Named patterns may not share the same name as a label.
 * Labels may be placed inside patterns to define a sub-pattern.
 * $goto() and $if() accept labels OR pattern names as the jump target.
 * For $if() and $signal(), an empty payload is assumed to be 0.
 * All jump-related commands are allowed jump to other tracks' data.

## Global commands

These commands may only appear before any tracks are defined.

| Command              | Effect                        |
| -------------------- | ----------------------------- |
| $tpq = i             | Set ticks per quarter note    |
| $globaltones = true  | Override global-tones setting |
| $globaltones = false                                 |

Notes:
 * Setting `$tpq = i` does not actually set the number of ticks per quarter
   note; rather, tempo changes are scaled to match the driver's internal count
   of 48 ticks per beat. This allows, for example, longer sweep durations at
   the cost of reduced granularity.

Note Syntax
-----------

Each note letter may be appended with a `+` or `-` to denote a sharp or flat
accidental, respectively. These may be stacked (eg. `a++` is A double-sharp).

At the start of playback, notes play on octave 5 (corresponding to the middle-C
octave). To move up an octave, the `>` character may be used. Likewise, the `<`
character may be used to move down an octave. To instead explicitly set the
octave, the `o` command may be used, followed by the octave number (in the
range of 0..10). Additionally, notes may use a default duration to simplify
writing; this is entirely a construct within the MML that does NOT translate
into code, unlike the octave commands, and has no value prior to being set.
Additionally, it is possible to jump multiple octaves using `>x` or `<x`, where
x is some integer.

To override the default note duration currently in use, a note may then contain
a duration specifier. For standard note durations, it is only necessary to use
the fraction of a whole note (eg. `a4` is a quarter note). To write triplets,
a similar notation may be used (eg. `a3` is a half-note triplet), or all notes
that are part of the tuplet may be encased with curly braces (eg. `{{a8a8a8}}`
is equivalent to `a12a12a12`).

To set the default note duration, the `l` command may be used (eg. `l16` will
set the default duration to 1/16 notes). Note that this "default note duration"
is a pseudo-command, and is not an explicit command of the driver; jumping to a
section that uses this command will NOT set the note duration.

A duration may be extended with dots or ties. Dotted notes work as expected,
where each dot adds progressively smaller halves of the original note value
(eg. `a4...` is equal to a 1/4 note tied to a 1/8 note tied to a 1/16 note),
and may use the default note duration as a base (eg. `l4 a...` is equivalent to
the earlier example).
Durations may also be tied by using a `^` between each value (eg. `a4^16^64` is
a 1/4 note tied to a 1/16 note tied to a 1/64 note), and likewise may use the
default note duration as a base (eg. `l4 a^16^64` is equivalent).
Additionally, it is also possible to subtract from a duration using a `-` sign
followed by the duration to remove (eg. `a1^-4` plays for a whole note minus a
quarter note - that is, it plays for 3 beats).
As an alternative to musical notation, the duration may also be specified in
ticks directly, by prepending with the `#` character (eg. `a#48` plays note A
for 48 ticks).

By default, all notes will cause the driver to rest for the duration of each
note. It is possible to override this behaviour, however, by appending `_` at
the end of the note; this will stop the driver from resting after that command
(eg. `c_{eg}` is functionally equivalent to `{ceg}`).

Separately from the internal rest timings, notes can also have their durations
modified with a simple multiply-add model, allowing for easier articulations
(staccato, legato, etc). The `q` command by itself resets all modifications;
this command can be followed by a fraction to scale the durations by (such as
`31/32` or `33/32`; the denominator must be a power of 2 and less than 32), or
a fraction of 8 (eg. `q1` is equivalent to `q1/8`; this is for legacy reasons,
where many MML-based drivers use 8 as the denominator). This fraction (if used)
can also be followed by a duration to add or subtract; this duration must be
less than 256 ticks, and is specified in standard note duration notation (eg.
`q+32` will add a 1/32 note duration for all subsequent notes).

Finally, each note may also carry a velocity associated with it. This velocity
can be set to apply to that note only, or to become "sticky" and apply to all
further notes, as though a `v` command had been used. This is achieved by
appending a `=` character immediately after the note (and its duration, if any)
followed by the velocity to use (1..128). If the velocity needs to "stick", a
`!` character may be appended after the velocity value (eg. `{ce=64g}` will
override the velocity only for the E note, whereas `{ce=64!g}` overrides the
velocity for the E note, and uses it for all notes that follow).

Multiple notes may be played simultaneously. The syntax for this is to enclose
all notes inside curly braces (eg. `{ceg}`); all notes inside the braces are
then treated as a single note for all notation purposes. Note that the velocity
of each note can still be overridden, and octave commands are still available.

Rests may be placed between notes during periods where no notes need to play.
The syntax for rests is to append the `r` command with a duration, in the same
way that notes have a duration appended.

Controller Syntax
-----------------

SGE supports a number of controllers to alter playback. They are as follows:
 * Velocity
 * Volume
 * Expression
 * Panning
 * Tuning (pitch bend)
 * Tempo

All of the above controllers (with the exception of Tempo) are unique to each
track (and do not affect other tracks). Tempo is always applied at the song
level, and applies to all tracks, regardless of which track issues the command.

Velocity is conceptually the level at which a note is physically being played
at, with a range of 1..128 (1 = Quietest, 128 = Loudest). When a note command
is issued, the key and velocity level at that time form a tuple that is sent to
the synthesizer. The synthesizer then selects the appropriate waveform(s) to be
played for such a key and velocity tuple, and begins playback. Note that once a
note has started playing, its velocity cannot be altered (ie. SGE does not
support aftertouch effects).

Volume is conceptually the master mixing level for a track, and ranges 1..128.
Generally, this control should not be used to simulate an instrument changing
its volume (eg. an instrument quieting down), but rather to balance the mix.

Expression is more-or-less equivalent to the volume the instrument is currently
being played at, with a range of 1..128. For example, a brass instrument might
perform a 'swell' effect, which could be simulated by sweeping this controller
up from a low level to a high level.

Panning controls the stereo position of this track in the mix, and is expressed
in the range of -63 for hard-left, through to +63 for hard-right (with 0 being
perfectly centered). This control is generally useful for stereo effects, or
more generally to simulate moving microhone positions.

Pitch bend is used to alter the pitch of all notes associated with this track,
and ranges -127.0..+127.0 semitones. This can be used to simulate pitch slides
on string and brass instruments, for example, or simply for effects.
Relative pitch bends (relative to the current bend) can be performed by using
a `~` symbol prior to the value (eg. `T~-1->+3=2` would set the bend to -1.0
semitones relative to the current bend value, then ramp to an absolute value
of +3.0 semitones over two beats).

Tempo controls the playback speed of the song, and is specified as 1..1024BPM.

All of these controllers may be set to an immediate value, or swept from one
value to another over a specified period of time. The general syntax for this
is to provide a value to set the controller to immediately, and if a sweep is
desired, follow it with a `->` character sequence and the target value plus the
duration of the sweep (prepended with a `=` sign). Alternatively, it is also
possible to sweep from the current value (whatever that may be) towards a
target value; in this case, an immediate value should not be provided, and
instead, the command should be followed immediately by the `->` sequence and
target value plus sweep duration.

For example, to set velocity to 64, use `v64`. To sweep from a volume of 32
towards a volume of 128 over two whole notes, use `V32->128=1^1`. And to sweep
from whatever the current expression value may be towards 1 over a whole note,
use `E->1=1`.

All controllers may be specified using either standard values, or raw values by
prepending each raw value with a `#` character.

Flow Control
------------

When a song has a repetitive motif, the MML supports looping of sections. In
SGE, there are 3 kinds of loops: Patterns, section repeats, and jump commands.

Patterns are sections of music enclosed by `[` and `]` characters, optionally
prefixed with a label (enclosed in `(` and `)`), and optionally followed by the
number of times to play the pattern when it is first encountered (this defaults
to 1). For example, `(lab)[abcd]2` defines a pattern labelled "lab", and plays
it twice, whereas `(lab)[abcd]` plays it only once. These patterns may later be
recalled by specifying the name of the pattern (enclosed in `(` and `)`), and
optionally followed by the number of times to play it (eg. to recall the "lab"
pattern from earlier, `(lab)` may be used. To play it twice, `(lab)2`, etc.).
When a pattern is defined without a label, this is referred to as an anonymous
pattern; these operate as a normal pattern, but may not be recalled by label;
instead, they can be recalled by using a `*` character (optionally followed by
the number of times to play it). Note that the `*` character only recalls the
last-defined pattern; if another pattern definition follows the anonymous one,
then the anonymous pattern may no longer be recalled.

All patterns may be recalled for up to 256 iterations. Note that patterns are
played first, and then begin repeating. For this reason, the first definition
of a pattern may play for up to 257 iterations (the first pattern defintion,
followed by 256 recalls).

If a pattern repeats but will not be recalled later, then a repeat section may
be used instead. These essentially behave as anonymous pattern definitions, but
are slightly more efficient in data storage. Similar to the first definition of
a pattern, section repeats can be played up to 257 iterations. The syntax is
very similar to that of anonymous patterns, albeit using `[[` and `]]` followed
by the number of iterations to play the section.

Some of the more advanced commands may require a target location. While it is
possible to specify the target as the start of a pattern, it is also possible
to define a label directly. Labels are specified using the `:` character, and
are followed by a name. This name is a case-sensitive alphanumeric sequence,
and must contain no spaces (eg. `:MyLabel` is valid, but `:My Label` is not).

Note that it is possible to place a label inside a pattern, and later recall it
as such; this gives a "sub-pattern recall", where the start of the label and
the end of the pattern form a new "sub-pattern". This may be useful for saving
space in some rare cases.

It is possible for a track to jump directly to somewhere else, and continue
playback from that point. This is a "jump" or "goto" command, and may be used
with the `$goto(Target)` syntax, where "Target" is a destination label. This is
useful, for example, to loop a track infinitely.

If it is desired to jump only when an external check gives the okay, this can
be accomplished using the `$if([Data,]Target)` command. The `Data` argument is
an optional 32-bit integer to pass to a music player's external handler, and
`Target` is a label to jump to on success for the condition. If the condition
fails, then playback continues as normal, but on success, playback resumes from
the target location. Note that `Data` is an optional argument; it is possible
to invoke the external handler without giving it any data.

Finally, the `$signal([Data])` command invokes the external handler, but does
not alter the playback position, and should generally be used to signal events
that may have happened (for example, entering a specific section of a song).
Similar to the `$if` command, `Data` is optional.
