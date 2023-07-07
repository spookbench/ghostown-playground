
TrackT EffectNumber = {
  .curr = NULL,
  .next = NULL,
  .type = 0,
  .interval = 0,
  .delta = 0,
  .pending = false,
  .data = {
    { -2, 1 },
    { 0, 0 },
    { -1, 0 },
  }
};

TrackT GhostownLogoPal = {
  .curr = NULL,
  .next = NULL,
  .type = 0,
  .interval = 0,
  .delta = 0,
  .pending = false,
  .data = {
    { -2, 1 },
    { 0, 0 },
    { 96, 1 },
    { 96, 2 },
    { 96, 3 },
    { 8928, 3 },
    { 48, 2 },
    { 48, 1 },
    { 48, 0 },
    { -1, 0 },
  }
};

static TrackT *AllTracks[] = {
  &EffectNumber,
  &GhostownLogoPal,
  NULL
};
