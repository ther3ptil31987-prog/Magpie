// Experimental XeSS Frame Generation marker effect. The renderer keeps this
// pass in the regular effect chain while XeSSFGPresenter owns interpolation
// and presentation through Intel's D3D12 proxy swap chain.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME XeSS_FrameGeneration

//!PARAMETER
//!LABEL Frame Multiplier
//!DEFAULT 2
//!MIN 2
//!MAX 4
//!STEP 1
int multiplier;

//!PARAMETER
//!LABEL Optical Flow Method
//!DEFAULT 1
//!OPTION 0 None
//!OPTION 1 AMDOF
//!OPTION 2 NVOF
int opticalFlowMethod;

//!PARAMETER
//!LABEL OF Quality
//!DEFAULT 1
//!OPTION 0 Performance
//!OPTION 1 Quality
int amdOpticalFlowMode;

//!PARAMETER
//!LABEL OF Quality
//!DEFAULT 2
//!OPTION 1 Performance
//!OPTION 2 Balanced (Recommended)
//!OPTION 3 Quality
//!OPTION 4 High Quality (High Cost)
//!OPTION 5 Highest Quality (Very High Cost)
int nvidiaOpticalFlowQuality;

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

MF4 Pass1(float2 pos) {
	return INPUT.SampleLevel(sam, pos, 0);
}
