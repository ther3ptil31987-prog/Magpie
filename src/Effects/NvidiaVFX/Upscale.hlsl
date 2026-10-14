// Native NVIDIA VFX Upscale; the renderer resolves its exact native dimensions.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME Nvidia VFX Upscale

//!PARAMETER
//!LABEL Sharpness
//!DEFAULT 0.4
//!MIN 0
//!MAX 1
//!STEP 0.01
float sharpness;

//!TEXTURE
Texture2D INPUT;
//!TEXTURE
Texture2D OUTPUT;
//!SAMPLER
//!FILTER LINEAR
SamplerState sam;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
MF4 Pass1(float2 pos) { return INPUT.SampleLevel(sam, pos, 0); }
