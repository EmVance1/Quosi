# Quosi

Quosi is a library designed to make adding complex branching dialogue to your C/C++ game as easy as possible. It combines the
familiarity of common programming language constructs with syntax designed to read like a script, aswell as flexible game state
integration. An simple Quosi script might look something like this:
```
module Brian

START = <Brian: "Oh hello there. Fancy a game of cards?", "It's good fun you know."> (
    "Love to. Names ${player} by the way." => happy
    "If that's what counts for fun around here." :: ( Brian.Approval -= 10 ) => chat
    if (Player.Origin == Twinvayne) then
        "Piss of Brian ya daft git." => EXIT
    end
)

happy = <Brian: "Good on ye ${player}."> => game
chat  = <Brian: "Ok slow down pal, you just got here. Where are you from?"> (
    match (Player.Origin) with
      (Twinvayne) "Twinvayne." => fasc
      (Domali)    "Domali."    => fasc
    end
    "Your moms house." => rude
    "Nowhere."         => enig
)

fasc = <Brian: "How fascinating."> => game
enig = <Brian: "How enigmatic.">   => game
rude = <Brian: "How rude.">        => game
game = <Brian: "Why don't you pick a card, any card."> (
    "*Pick high*" => pick
    "*Pick low*"  => pick
)
pick = match (rng3) with
    (0) <Narrator: "*You draw an ace.*">            => EXIT
    (1) <Narrator: "*You draw a 3.*">               => EXIT
    (_) <Narrator: "*You draw a 9. As did Brian.*"> => EXIT
end

endmod
```
As you can see, branching paths, situational response options, and game state modification are first class citizens. Integrating the
library itself into your project is just as easy. The main API consists of just 3 functions - compile, load, execute. The compiled
binary format can be saved and loaded completely as is, meaning all compilation can be done ahead of time to negate load times.

As a side note, the language is, as best I can tell, Turing-complete. In some sense it directly models a turing machine, complete with
state transitions and (theoretically) unbounded memory. This means that it can be abused to make poor Brian compute the 10th fibonacci
number. For example. I do not claim that you *should* ever do this, I am simply stating that you can.
```
module Fib

START = <Brian: "init"> :: ( fib.i = 0, fib.a = 0, fib.b = 1 ) => loop

loop = if (fib.i < 10) then
    <Brian: "loop"> :: ( fib.temp = fib.a + fib.b, fib.a = fib.b, fib.b = fib.temp, fib.i += 1 ) => loop
else
    <Brian: "end"> => EXIT
end

endmod
```
