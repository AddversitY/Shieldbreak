# Character shortlist (task C1)

Free Sketchfab soldier candidates for the prototype. Metadata comes from the public API
(`https://api.sketchfab.com/v3/models/<uid>`, no login) and is saved in `candidates.json`. Each entry has the uid,
name, author, licence, face/vertex/animation counts, whether it's downloadable, tags, an AI-generated flag
(`isGeneratedWithAI` or AI tags), any franchise keywords found, and review notes. Thumbnails are `<uid>.jpg`, the
largest available that is at most 1024 px wide. **No models have been downloaded.**

- `source: given` is the 9 uids from the task.
- `source: search` is 5 picks from API searches with downloadable=true, sorted by likes.
  - Queries: "sci-fi soldier", "armored soldier rigged", "sci-fi trooper", "futuristic armor character",
    "exosuit", "sci-fi armor suit" and similar.
  - Every pick is CC Attribution, not flagged AI and has no franchise keywords, and its thumbnail was reviewed by eye.

Excluded during review:
- Everything by `jungle_jim`: the descriptions say the models were made with AI tools.
- "Cyborg Soldier SciFi Character": the description says it was made with an AI generator.
- "Heavy Trooper": inspired by Command & Conquer's Zone Trooper.
- "Futuristic Soldier": Crysis and nanosuit tags.
- Halo, Star Wars, Iron Man and Doom fan art.
- Three picks that turned out not to be armored soldiers: a hoodie character, a bare exoskeleton frame, and an
  unarmored body.

Given uid flagged: `eb1cc340` "Chinese Power Armor" has `fallout` and `power armor` tags (likely Fallout fan art).

Best fits on first look:
- `de876bfd` Sci-Fi Soldier / Futuristic Combat Trooper (30.8k faces)
- `6c53691f` SciFi Soldier Character Low-poly (33.8k)
- `daeecbd4` Hazard Unit (5.7k)
- `e9aed326` female hard-surface suit (96k)
- `88351469` AEPC suit (177k)

All are CC BY, so credit the author if one is used.
