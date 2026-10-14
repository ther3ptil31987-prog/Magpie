// NVIDIA VideoSuperRes native backend replaces this placeholder pass.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME Nvidia VFX VSR

//!PARAMETER
//!LABEL Category
//!DEFAULT 0
//!OPTION 0 Standard
//!OPTION 1 High bitrate
//!OPTION 2 Streaming
//!OPTION 3 Denoise
//!OPTION 4 Deblur
int category;

//!PARAMETER
//!LABEL Quality level
//!DEFAULT 1
//!OPTION 0 Low
//!OPTION 1 Medium
//!OPTION 2 High
//!OPTION 3 Ultra
int strength;

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
