# 13: Child safety, privacy and compliance

Carries forward `../../docs/parental-safety.md` in full, and extends it to web and phone.

## 1. Child experience safety
- No failure screens, pressure timers, scores, lives, streaks, ads, purchases, external links,
  social features or chat.
- No dark patterns: no "come back" notifications (no notifications at all), no reward schedules
  designed to prolong play, and no loot/random rewards gating content. Surprise (e.g. which creature
  hatches) is fine; scarcity is not.
- Session timer (parent-set) with a gentle wind-down; sleep after 10 min idle.
- Photosensitivity rules (`06`, `11`), and a reduced-motion setting.
- Hearing: muted start (device), parent cap, limiter, onset guard, measured SPL.
- Content: friendly characters only. No scary content, weapons, or teeth-baring sharks. No gendered
  stereotypes. Diverse, non-human cast avoids representation pitfalls; the narrator is warm and
  neutral.

## 2. Privacy
| Data | Policy |
|---|---|
| Microphone audio (voice commands) | Streamed into the recogniser's RAM ring buffer only during push-to-talk; zeroed after; never stored or transmitted |
| Microphone audio (Sound Jar / Beat Sharks) | One clip ≤ 3 s in RAM per game; zeroed on exit/pause/sleep/parent gate/OS background; never stored, logged or transmitted; parent toggle (default on device = on (v1); default on phone/web = **off until the parent enables it**, because of OS permission UX) |
| Settings | Volume, toggles, calibration, session timer, game visibility; stored locally (NVS / IndexedDB / app storage). No identifiers |
| Telemetry, crash reports, analytics | None. Local debug logs only in dev builds |
| Network | None at runtime. Web: same-origin install only, CSP `connect-src 'self'` |
| Camera, location, contacts, identifiers | Never accessed |

## 3. Parent gate
Hold a corner for 5 s (with a visible progress fill, tolerant of small slips; v1 behaviour) + an adult
challenge (e.g. a sum, or "tap the numbers in order"). It is not a security boundary, just a toddler
filter. It guards settings, the mic permission request, and licences/about.

## 4. Compliance research tasks (Phase 0 / pre-release; cite current sources)
- [ ] COPPA (US) applicability given zero data collection; document the position.
- [ ] GDPR / UK Age-Appropriate Design Code: data minimisation statement.
- [ ] Google Play Families Policy + Designed for Families requirements; target audience declaration;
      audit SDKs.
- [ ] Apple App Store Kids Category (Guidelines 1.3, 5.1.4): no third-party analytics/ads; parental
      gate.
- [ ] Toy safety for the physical device: EN 71 / ASTM F963 (acoustics, small parts, battery
      access), IEC 62368-1 for electronics, and battery transport. The physical product is out of
      scope for software, but record open items for the owner.
- [ ] Open-source licence obligations (notices in the parent About screen; copyleft analysis in
      `decisions/licence_register.md`).

## 5. Asset licensing
- Original work, CC0/public domain, or a licence that allows commercial use without share-alike
  obligations on the app. Every asset has a provenance record (author, source URL, licence, date,
  SHA-256, and any modifications).
- v1 animal calls are CC0 with recorded provenance (`../../assets/audio/animal_sounds/sources.json`).
  Re-verify them, or replace them with higher-quality CC0 or original recordings.
- No copyrighted characters or likenesses. Avoid resemblance to famous robots, sharks, monsters or
  dinosaurs from existing media.
