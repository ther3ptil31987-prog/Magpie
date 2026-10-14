// Executed by the color-component adapter / RTX Video HDR backend.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME Nvidia VFX TrueHDR

//!PARAMETER
//!LABEL Contrast
//!DEFAULT 100
//!MIN 0
//!MAX 200
//!STEP 1
int contrast;

//!PARAMETER
//!LABEL Saturation
//!DEFAULT 100
//!MIN 0
//!MAX 200
//!STEP 1
int saturation;

//!PARAMETER
//!LABEL Middle gray
//!DEFAULT 50
//!MIN 10
//!MAX 100
//!STEP 1
int middleGray;

//!PARAMETER
//!LABEL Peak brightness (nits)
//!DEFAULT 1000
//!MIN 400
//!MAX 2000
//!STEP 1
int peakNits;

//!PARAMETER
//!LABEL Debanding
//!DEFAULT 1
//!MIN 0
//!MAX 1
//!STEP 1
int debanding;

//!TEXTURE
Texture2D INPUT;
//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;
//!SAMPLER
//!FILTER LINEAR
SamplerState sam;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
MF4 Pass1(float2 pos) { return INPUT.SampleLevel(sam, pos, 0); }
