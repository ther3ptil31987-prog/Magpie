// Executed by the color-component adapter / RTX Video HDR backend.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME SDR to HDR

//!PARAMETER
//!LABEL Mapping
//!DEFAULT 0
//!OPTION 0 Auto: restore pair or map SDR white
//!OPTION 1 Native SDR white mapping
//!OPTION 2 Restore compatibility pair
int mode;

//!PARAMETER
//!LABEL SDR white brightness (nits)\nHigher values brighten the entire image; default 203. Legacy dim values remain unchanged until edited.
//!DEFAULT 203
//!MIN 8
//!MAX 4000
//!STEP 1
float nativeWhiteNits;

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
