# Research: Godfried Toussaint & Bjorklund Algorithm in C++
- **Author:** Cascade Agent
- **Date:** 2026-02-15
- **Audience:** Solo JUCE/C++ developer building Euclidean rhythm generators
- **Goal:** Connect Toussaint/Bjorklund theory to practical, real-time-safe C++ implementations
- **Confidence:** Medium – canonical academic sources plus vetted implementation articles

## Scope & Questions
1. Summarise how Toussaint ties Euclidean rhythms to traditional patterns and why Bjorklund's construction works mathematically.
2. Define a production-ready C++ API for generating and rotating Euclidean hit patterns with deterministic complexity.
3. Identify data structures and optimisations that keep allocations predictable for audio/MIDI plug-ins.
4. Highlight validation and testing approaches to ensure parity with published rhythm tables.

- **Research mode:** Manual synthesis drawing on Toussaint's paper, Bjorklund's SNS timing note, and implementation-focused articles (standard-depth pass).
- **Success criteria:** Each objective has at least one traceable source, C++ guidance maps directly to plugin work, and open issues are documented for follow-up.

## Source Log
| Type | Reference | Date | Notes |
| --- | --- | --- | --- |
| Academic paper | Godfried T. Toussaint, "The Euclidean Algorithm Generates Traditional Musical Rhythms" (BRIDGES 2005) – https://cgm.cs.mcgill.ca/~godfried/publications/banff.pdf | 2026-02-15 | Maps Euclidean algorithm outputs to world rhythms and formalises rotation/duality |
| Technical report | E. Bjorklund, "The Theory of Rep-Rate Patterns in the SNS Timing System" (SNS/ORNL, 2003) – https://ics-web.sns.ornl.gov/timing/Rep-Rate%20Theory.pdf | 2026-02-15 | Original description of the algorithm and proof of bounded discrepancy |
| Technical article | Chris McCormick, "Euclidean Rhythms" (2014) – https://chrismccormick.ai/2014/08/31/euclidean-rhythms | 2026-02-15 | Step-by-step implementation guidance plus iterative version suited to high-level languages |

## Key Findings
1. **Bjorklund's construction distributes `k` pulses over `n` steps with maximal evenness, allowing at most a single-step discrepancy between any two inter-onset intervals.** The SNS note frames the algorithm as repeated division with remainders, yielding a sequence whose histogram matches evenly spaced triggers up to rounding error, which Toussaint later mapped directly to Afro-Cuban, Balkan, and Middle Eastern rhythms.[1][2]
2. **Toussaint formalised rotation (`rot`) and complement operations as cyclic permutations, ensuring every Euclidean rhythm has `n` unique phase offsets and a dual pattern (`n-k` pulses) useful for rests/accents.** Implementations can therefore treat rotation as a cheap `std::rotate` or modular index adjustment rather than rerunning the generator.[1]
3. **An iterative remainder algorithm (stack-based) offers predictable O(n) behaviour and avoids recursion, making it appropriate for plug-ins and embedded targets.** McCormick demonstrates building two queues (hits and rests) and interleaving them while counts remain, mirroring Bjorklund's textual algorithm but using contiguous vectors that can be pre-sized to avoid heap churn.[2][3]
4. **Verifying implementations requires comparing against Toussaint's canonical rhythm tables and ensuring the generated sequence degenerates gracefully when `k` ∈ {0, n} or when rounds are small.** Both Toussaint and McCormick provide examples (clave, Baladi, Rumba) that can serve as fixtures in automated tests.[1][3]

## Implementation / Application Notes
- **Canonical API shape:** `std::vector<uint8_t> generateEuclideanPattern(std::size_t steps, std::size_t pulses, std::size_t rotation = 0)` returning 0/1 hits allows callers to reuse the same buffer for GUI rendering, scheduling, or MIDI conversion.[2][3]
- **Algorithm outline:** Store two working vectors representing `pulses` and `steps - pulses` groups, repeatedly append the shorter vector to each element of the longer until remainders vanish, then flatten. Pre-size with `pattern.reserve(steps)` to avoid reallocations, and clamp rotation via `rotation %= steps` before applying `std::rotate`.[2]
- **Real-time safety:** Generate or update the pattern off the audio thread, then publish via lock-free swap. Output as `std::array<bool, maxSteps>` when a compile-time ceiling exists to leverage stack allocation and deterministic cache lines.
- **Testing:** Build fixtures from Toussaint's table (e.g., `steps=16, pulses=5` → Bossa Nova pattern) and assert equality against expected bit patterns during CI. Include property tests ensuring average spacing difference ≤ 1 sample equivalent once mapped to transport.
- **Extensibility:** Support metadata per step (velocity/accent) by maintaining parallel arrays sized like the pattern; Bjorklund output can act as mask to gate pre-filled values without touching the generator's complexity.

## Risks & Pitfalls
- Misinterpreting Bjorklund's initial remainder ordering can yield mirrored rhythms; always normalise by comparing against Toussaint's reference orientation before exposing to users.[1]
- Naïvely rebuilding patterns on every timer tick can cause UI/audio desync; debounce parameter changes and reuse cached vectors.
- Using dynamic allocation or recursion on the audio thread risks glitches; ensure builders run on worker threads and only publish immutable snapshots.
- Tests that rely solely on visual inspection miss off-by-one errors—encode fixture rhythms as binary literals to keep regression coverage high.[3]

## Open Questions & Next Steps
- Decide whether the plugin should expose both rotation and offset parameters or lock rotation to maintain consistent downbeats across tracks.
- Evaluate storing both the Euclidean rhythm and its complement to drive accent lanes or muting logic without recomputation.
- Investigate SIMD or bitset encodings when supporting very high step counts (>128) to keep cache usage bounded.
