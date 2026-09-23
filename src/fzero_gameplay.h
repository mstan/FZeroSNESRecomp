#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum FzeroRule {
  FZERO_RULE_TUNING,
  FZERO_RULE_BOOST,
  FZERO_RULE_EXHAUST,
  FZERO_RULE_ANIMATION,
  FZERO_RULE_DASH,
  FZERO_RULE_SPIN,
  FZERO_RULE_BOUNCE,
  FZERO_RULE_CPU_SPIN,
  FZERO_RULE_ROTATION,
  FZERO_RULE_LEGEND,
  FZERO_RULE_DMAG,
  FZERO_RULE_LAPS,
  FZERO_RULE_RAINBOW,
  FZERO_RULE_RED_BUMPER,
  FZERO_RULE_HITBOX,
  FZERO_RULE_UP_MAGNET,
  FZERO_RULE_FOG,
  FZERO_RULE_MSU,
  FZERO_RULE_CREDITS,
  FZERO_RULE_COUNT
} FzeroRule;
typedef struct FzeroGameplaySettings {
  uint32_t enabled;
  unsigned tuning, boost, exhaust; /* 0=P1, 1=P2, 2=P3. Retained while off. */
  unsigned vehicle_packs; /* Independent CGP P1/P2/P3 bits. */
  unsigned stock_rebalance; /* Explicit BF/WG/GF/FS identity bits. */
} FzeroGameplaySettings;
typedef struct FzeroRuleInfo {
  const char *id, *name, *description;
} FzeroRuleInfo;
extern const FzeroRuleInfo fzero_rules[FZERO_RULE_COUNT];
bool FzeroRuleEnabled(FzeroRule rule);
void FzeroGameplayConfigure(const FzeroGameplaySettings *settings, bool cars,
                            bool tracks);
void FzeroGameplayHeadless(bool deluxe);
bool FzeroBsCars(void);
bool FzeroBsTracks(void);
bool FzeroGameplayActive(void);
const FzeroGameplaySettings *FzeroGameplaySettingsCurrent(void);
const uint8_t *FzeroGameplaySignature(void);
bool FzeroGameplayPrepare(uint8_t **rom, size_t *size);
const char *FzeroGameplayError(void);
void FzeroGameplayInstallHooks(void);
void FzeroGameplayActivateVehicles(const FzeroGameplaySettings *settings, uint8_t *rom);
void FzeroGameplaySetSignature(const uint8_t hash[32]);
uint16_t FzeroGameplayMenuInput(uint16_t input, const uint8_t *ram);
