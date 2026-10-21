Time Pilot 3D - recordings that replace the synthesized effects.

A WAV file here with one of these names is played instead of the sound the
game makes itself (mono or stereo, 16 bit, any rate; keep them short and
normalized). Files that are missing are made in code, as before.

  portal.wav       the time tunnel's mouth opens (intro and outro of an era)
  whoosh.wav       the plane rushes by: out of the tunnel, round its loop
  warp.wav         the flight through the time tunnel (14 seconds)
  shot.wav         the player's guns
  enemy_shot.wav   an enemy fires
  explode.wav      an enemy goes down
  big_explode.wav  a boss goes down
  death.wav        the player's plane goes down
  hit.wav          the player's plane takes a hit
  hit_boss.wav     a shot lands on a boss
  missile.wav      a missile is launched
  burner.wav       the afterburner lights (a thump)
  fall.wav         a plane shot down goes into its dive (the howl before it hits)
  ring.wav         a ring of the time tunnel passed (played higher through its middle)
  scream.wav       a parachutist shot (scream_2.wav to scream_6.wav: more takes, one at
                   random). The ones here are the Wilhelm screams of the Star Wars
                   Ultimate Sound Effects Collection (freely available)
  warn.wav         warning beep (missile, salvo, laser lock)
  stall.wav        stall warning beep
  boss.wav         the boss arrives (alarm)
  formation.wav    a formation comes
  bonus.wav        a whole formation shot down
  pickup.wav       a parachutist taken aboard
  oneup.wav        an extra plane
  coin.wav         a coin
  start.wav        a game begins
  tink.wav         a step in a menu
  flyby.wav        something rushing past close, for every kind that has none of its own
  flyby_1910.wav   a biplane going by close     flyby_1940.wav     a fighter of 1940
  flyby_rotor.wav  a helicopter                 flyby_jet.wav      a jet
  flyby_ufo.wav    a saucer                     flyby_missile.wav  a missile
  flyby_boss.wav   a boss
  (16-bit PCM recordings, and the made ones, come from the side the thing is
  on; so do explosions, enemy shots and hits on a boss)

Loops (a recording here should loop cleanly; the game changes their pitch
with the speed):

  engine_outside.wav  the player's engine heard from outside (chase view, films)
  engine_cockpit.wav  the same from inside the cockpit (duller, deeper)
  wind.wav            the wind over the plane, louder in the cockpit
  afterburner.wav     the afterburner's roar while it burns
  engine_1910.wav ... engine_2001.wav, engine_missile.wav
                      the enemies' engines of each era and a missile's motor,
                      heard from their side (16-bit PCM); missing ones are made

To hear what the game makes now: run it with TP3D_SOUNDS_OUT=<folder> and
every effect is written there under these names (see BUILD.md).
