// Marker selecting the SDR VFG stage, before per-output TrueHDR processing.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME Nvidia VFX VFG

//!PARAMETER
//!LABEL Frame multiplier
//!DEFAULT 2
//!MIN 2
//!MAX 8
//!STEP 1
int multiplier;

//!PARAMETER
//!LABEL Model
//!DEFAULT 1
//!OPTION 0 Performance
//!OPTION 1 Balanced
int model;

//!PARAMETER
//!LABEL Automatic shot detection
//!DEFAULT 1
//!MIN 0
//!MAX 1
//!STEP 1
int automaticShotDetection;

//!TEXTURE
Texture2D INPUT;
//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;
//!SAMPLER
//!FILTER POINT
SamplerState sam;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
MF4 Pass1(float2 pos) { return INPUT.SampleLevel(sam, pos, 0); }
