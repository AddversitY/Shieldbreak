# Character shortlist, round 2 (task C2)

Vincent liked option 4 but wants true AAA detail. `candidates.json` lists each candidate's link, licence, poly count, textures where
known, rig, and review notes. All images are JPG.

## Option 4 baseline (measured)

`opt4_front34.jpg`, `opt4_back.jpg` and `opt4_helmet.jpg` are Cycles renders at 1600x900 with neutral studio light. The full model is
in `Saved/CharacterCandidates/opt4/` (untracked): a 110 MB GLB plus 66 textures.

- **96,052 triangles**, 55k vertices, 19 meshes and 19 materials, **no skeleton**.
- Each material has its own 2048x2048 set: base colour, metal/roughness, normal, and emissive on 13. That's 71 maps in total, none at 4K.
- **Verdict:**
  - Good: clean panel lines, and a glossy visor with three lamps.
  - Weak: the purple paint reads as toy-like, it has a female body shape, and 19 separate 2K sets is inefficient for a game.
  - Detail is mid-tier, not AAA.

## Paragon heroes (Fab, Epic Games)

Free, shipped-game quality, built on the UE skeleton with animations and AnimBPs. The listings say "free to use as you like in your
Unreal Engine projects". You may not use the PARAGON trademark. Poly counts and texture sizes aren't on the listings.

| Hero | Armour and helmet | Fit |
|---|---|---|
| Wraith | Sleek full-helmet armour (black, white and red skins), rifle | **Best Halo-style silhouette** |
| Murdock | Heavy armoured cop; blue and black full-helmet skins | Strong: bulky trooper |
| Lt. Belica | Default skin has an exposed face; one skin (image 6) is a full-helmet bronze armour | Good if that skin is used |
| Steel | Heavy cyborg armour, face exposed, brute proportions | Enemy or heavy class |
| Crunch, Howitzer, Riktor | Power-armour brute, mech, cyborg brute | Not player silhouettes |
| Dekker | Sci-fi suit, face exposed | Weak |
| Greystone, Terra, Kwang | Fantasy plate armour | Off-theme |

## Sketchfab high-detail

All CC Attribution, not flagged AI. Franchise and fan art were excluded, as was every `jungle_jim` model, since their descriptions say AI was used.

- `d7698f6a` Rank 3 Police Unit (nikvili): staff pick, **1.46M faces**, very detailed helmet and gear. It needs heavy optimisation.
- `404837f3` FREE Female light assault armour (mornersul): 168k faces, and the author links a blend with **4K textures and a rig**.
- `09e0bd9c` SCI-FI SuperSoldier (sihagames): 67k faces, full helmet, animated. The helmet mesh comes from another author, so check that licence.
- `2707c5a2` Sci-Fi Armor Clothing (kaanTezcan): a torso piece only, useful for kitbashing.
- `26ccafad` Cyber Samurai (duongminhkhoa231): staff pick, a robot rather than a soldier.

Full-body, full-helmet, original sci-fi soldiers at true AAA detail under CC BY are rare on Sketchfab. The Paragon heroes are the
only shipped-game-quality, rigged option found.
