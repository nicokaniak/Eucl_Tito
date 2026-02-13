# Research: Logging and Error Handling in the JUCE Framework
- **Author:** Cascade Agent
- **Date:** 2026-02-13
- **Audience:** Juce-based app/plugin developers (solo)
- **Goal:** Understand JUCE-native logging and error-handling options to guide debugging/telemetry strategies in new projects
- **Confidence:** High – sourced from current JUCE reference docs

## Scope & Questions
1. Catalogue JUCE logging primitives (Logger, FileLogger, DBG, output sinks) and how to configure them.
2. Summarise JUCE error-handling helpers (Result, jassert family, exception hooks) and when to use each.
3. Highlight build/config flags that alter logging or error reporting in production.
4. Capture practical considerations for combining logging + error handling inside audio plug-ins and apps.

## Source Log
| Type | Reference | Date | Notes |
| --- | --- | --- | --- |
| Official docs | [juce::Logger](https://docs.juce.com/master/classLogger.html) | 2026-02-13 | Lifecycle, static helpers, log routing |
| Official docs | [juce::FileLogger](https://docs.juce.com/master/classFileLogger.html) | 2026-02-13 | Persistent logging implementation |
| Official docs | [juce::Result](https://docs.juce.com/master/classResult.html) | 2026-02-13 | Success/failure wrapper semantics |
| Official docs | [juce_PlatformDefs macros](https://bill-auger.github.io/JUCE/doxygen/doc/juce__PlatformDefs_8h.html) | 2026-02-13 | DBG, jassert, logging macros |
| Official docs | [juce_core config macros](https://docs.juce.com/master/juce__core_8h.html) | 2026-02-13 | JUCE_LOG_ASSERTIONS, JUCE_CATCH_UNHANDLED_EXCEPTIONS |
| Official docs | [JUCEApplicationBase](https://docs.juce.com/master/classJUCEApplicationBase.html) | 2026-02-13 | unhandledException hook |

## Key Findings
1. **Logger is the central sink**, accessed via static `Logger::writeToLog()`; set a custom subclass through `setCurrentLogger()` to route all framework log calls (including `DBG`) to a destination such as files, OS consoles, or remote transports.[1]
2. **FileLogger ships as a ready-made Logger** that writes to disk, exposes rotation helpers (`createDateStampedLogger`, `trimFileSize`) and convenience factory `createDefaultAppLogger()` that respects per-OS log directories; remember JUCE does not own the logger pointer, so manage its lifetime carefully.[2]
3. **DBG macro and `Logger::outputDebugString()` only fire in debug builds** (unless `JUCE_LOG_ASSERTIONS` is enabled) and should never carry side effects; they map to stderr / debugger channels, making them ideal for verbose dev telemetry while production builds remain silent by default.[4][5]
4. **JUCE error handling prefers return-type signalling via `juce::Result`**—callers inspect `wasOk()`/`failed()` and retrieve user-facing strings via `getErrorMessage()`, enabling consistent propagation without exceptions in realtime contexts.[3]
5. **Assertions (`jassert`, `jassertfalse`, `static_jassert`) break into debuggers when `JUCE_DEBUG && !JUCE_DISABLE_ASSERTIONS`**; enabling `JUCE_LOG_ASSERTIONS` keeps assertion text flowing to logs even in release builds, aiding post-mortems.[4][5]
6. **`JUCE_CATCH_UNHANDLED_EXCEPTIONS` funnels crashes to `JUCEApplicationBase::unhandledException()`**, letting apps capture stack details or flush logs before exit; override the method to integrate with your logger/FileLogger combo.[5][6]

## Implementation / Application Notes
- **Bootstrap logging early:** Instantiate a `FileLogger` inside `JUCEApplication::initialise()` (or plugin constructor) and register it with `Logger::setCurrentLogger()` so all later `DBG`/`Logger::writeToLog` calls persist. Remember to reset (set nullptr) before shutdown to avoid dangling pointers.
- **Guard realtime threads:** Prefer `Logger::writeToLog()` from Message Thread; if logging from audio threads is unavoidable, queue messages or use lock-free FIFOs to avoid disk IO stalls.
- **Structured errors with Result:** Return `Result::fail("detail")` from setup routines (e.g., file IO, `AudioProcessor::prepareToPlay`) and surface the message to UI dialogs or logs; combine with `jassertfalse` during development to flag unexpected failures while still returning `Result` for release builds.
- **Configuration flags:**
  - `JUCE_LOG_ASSERTIONS=1` → logs every assertion.
  - `JUCE_DISABLE_ASSERTIONS=1` → strip asserts (use cautiously; prefer default behaviour).
  - `JUCE_CATCH_UNHANDLED_EXCEPTIONS=1` → ensures `unhandledException()` fires before crash dialogs.
- **Bridging to host logs:** In plug-ins, supplement JUCE logging with host-facing `AudioProcessor::setLastProcessingError()` or custom UI notifications so DAWs can expose failure reasons.

## Risks & Pitfalls
- **Forgetting to clear custom loggers** can leave dangling pointers at shutdown. Always call `Logger::setCurrentLogger(nullptr)` in `shutdown()` or destructors.[1]
- **Heavy logging in audio callbacks** can cause XRuns; keep logging out of realtime paths or throttle messages using counters/time checks.[2]
- **Depending solely on DBG asserts** means release builds lose diagnostics; enable `JUCE_LOG_ASSERTIONS` or pair asserts with `Logger::writeToLog()` when issues must be traceable on user machines.[4][5]
- **Unhandled exceptions inside plug-ins** may be swallowed by hosts if `JUCE_CATCH_UNHANDLED_EXCEPTIONS` is off; enable it and implement `unhandledException()` to ensure logs capture stack/context before the host terminates the plug-in.[5][6]

## Open Questions & Next Steps
- Evaluate whether the project needs structured log formats (JSON) and implement a custom `Logger` subclass if FileLogger is insufficient.
- Decide on a cross-platform location/retention policy for logs (AppData, ~/Library/Logs, etc.).
- Prototype a lightweight log queue for audio-thread diagnostics that drains on the Message Thread without blocking.
